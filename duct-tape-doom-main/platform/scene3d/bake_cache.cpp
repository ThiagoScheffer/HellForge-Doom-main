/* Versioned disk cache of numeric bake channels. A hit skips tracing only:
   source lists, masks, lamp groups, sky metadata and GPU uploads still run. */
#include "internal.h"
#ifndef DOOM_BAKE_CODE_HASH
#error The bake cache requires a configured numerical fingerprint
#endif
#include <filesystem>
#include <fstream>
#include <system_error>
#include <limits>
#include <random>
#include <optional>
#include <tuple>

namespace doom3d {
namespace {
namespace fs=std::filesystem;
constexpr uint64_t seed=14695981039346656037ull;
constexpr uint64_t limit=1024ull*1024*1024;
constexpr uint64_t entryLimit=64ull*1024*1024;
constexpr uint32_t format=1;
constexpr uint32_t keySchema=2;
#ifdef DOOM_TEST_DRIVER
bool injectWriteFailure=false;
#endif
uint64_t hashBytes(uint64_t h,const void *data,size_t size) {
    const auto *p=static_cast<const uint8_t*>(data);
    for(size_t i=0;i<size;++i)h=(h^p[i])*1099511628211ull;
    return h;
}
std::string hex(uint64_t h) {char b[17];snprintf(b,sizeof(b),"%016llx",(unsigned long long)h);return b;}
fs::path configuredRoot() {
    if(const char *p=SDL_getenv("HELLFORGE_MAP_CACHE");p&&*p)return fs::u8path(p);
    if(const char *p=SDL_getenv("TUIN_MAP_CACHE");p&&*p)return fs::u8path(p);
#ifdef _WIN32
    // The stable launcher and direct Windows launches share Local AppData.
    // The experimental launcher supplies its own root through the override.
    if(const char *p=SDL_getenv("LOCALAPPDATA");p&&*p)
        return fs::u8path(p)/"DOOM-HellforgeLauncher"/"Cache"/"Maps";
#endif
    char *p=SDL_GetPrefPath(nullptr,"DOOM-HellforgeLauncher");
    if(!p)return {};
    fs::path result=fs::u8path(p)/"Cache"/"Maps";SDL_free(p);return result;
}
const fs::path &root() {
    // Resolve explicit relative overrides once, at engine use, before any level reload.
    static const fs::path resolved=[] {
        try {
            auto path=configuredRoot();
            return path.empty()?path:fs::absolute(path).lexically_normal();
        } catch(const std::exception &) {return fs::path{};}
    }();
    return resolved;
}
bool cacheEnabled() {
    return settings.mapLightCache&&!M_CheckParm((char*)"-nobakecache");
}
std::string mapName() {
    char map[24];
    if(gamemode==commercial)snprintf(map,sizeof(map),"MAP%02d",gamemap);
    else snprintf(map,sizeof(map),"E%dM%d",gameepisode,gamemap);
    return map;
}
struct CacheSession {
    size_t id=0,eligible=0,restored=0,calculated=0,uploaded=0,failures=0,verified=0;
    uint64_t dispatches=0,bytes=0;
    std::optional<uint64_t> geometry,atlas,sources,sunOutput,directOutput;
    bool wrote=false;
    double geometryMs=0,reconcileMs=0;
    size_t evicted=0;
    double keyMs=0,lookupMs=0,readMs=0,calculateMs=0,writeMs=0,uploadMs=0;
} report;
double elapsed(uint64_t start) {return (SDL_GetTicksNS()-start)/1e6;}
const char *algorithmFor(const std::string &stage) {
    if(stage=="sun")return DOOM_SUN_BAKE_HASH;
    if(stage=="lights_hf2")return DOOM_DIRECT_BAKE_HASH;
    if(stage=="grid_hf2")return DOOM_GRID_BAKE_HASH;
    if(stage=="bounce")return DOOM_BOUNCE_BAKE_HASH;
    if(stage=="ambient")return DOOM_AMBIENT_BAKE_HASH;
    if(stage=="flow")return DOOM_FLOW_BAKE_HASH;
    return DOOM_BAKE_CODE_HASH;
}
template<class Build> uint64_t component(std::optional<uint64_t> &memo,Build build) {
    if(report.id&&memo)return *memo;
    BakeKeyHash hash;
    build(hash);
    if(report.id)memo=hash.result();
    return hash.result();
}
uint64_t geometryKey() {
    return component(report.geometry,[](BakeKeyHash &h) {
        h.fields(mapWidth,mapHeight,mapCell,mapOrigin);
        h.length(seamCells.size());
        for(const auto &cell:seamCells)h.value(cell.own);
        h.length(bakeMap.sectors.size());
        for(size_t i=0;i<bakeMap.sectors.size();++i) {
            const auto &s=bakeMap.sectors[i];
            h.fields(s.floor,s.ceiling,s.sky,sectors[i].floorpic==skyflatnum,sectors[i].ceilingpic==skyflatnum);
            h.length(s.lines.size());
            for(int line:s.lines)h.value(line);
            h.length(s.things.size());
            for(int thing:s.things)h.value(thing);
        }
        // Images shared by several lines/things need only be scanned once.
        // Pointer reuse is local to this call. Only image content enters the key.
        std::map<std::tuple<const unsigned char *,int,int>,uint64_t> images;
        auto mask=[&](const unsigned char *pixels,int width,int height) {
            h.value(pixels!=nullptr);
            if(!pixels)return;
            if(width<=0||height<=0||size_t(width)>std::numeric_limits<size_t>::max()/size_t(height)/2)
                throw std::runtime_error("invalid cache mask");
            const auto image=std::make_tuple(pixels,width,height);
            auto [it,inserted]=images.try_emplace(image);
            if(inserted) {
                BakeKeyHash coverage;
                const size_t size=size_t(width)*height*2;
                for(size_t i=1;i<size;i+=2)coverage.byte(pixels[i]);
                it->second=coverage.result();
            }
            h.component(it->second);
        };
        h.length(bakeMap.lines.size());
        for(const auto &line:bakeMap.lines) {
            const auto &m=line.mask;
            h.fields(line.ax,line.ay,line.bx,line.by,line.front,line.back);
            h.fields(m.width,m.height,m.u,m.direction,m.anchor,m.bottom,m.top);
            mask(m.pixels,m.width,m.height);
        }
        h.length(bakeMap.things.size());
        for(const auto &thing:bakeMap.things) {
            h.fields(thing.x,thing.y,thing.top,thing.left,thing.width,thing.height);
            mask(thing.pixels,thing.width,thing.height);
        }
    });
}
uint64_t atlasKey() {
    return component(report.atlas,[](BakeKeyHash &h) {
        h.fields(atlasWidth,atlasHeight,atlasStride);
        h.length(bakeStrips.size());
        for(const auto &s:bakeStrips)
            h.fields(s.x,s.y,s.width,s.height,s.sector,s.low,s.high,s.scale,s.ax,s.ay,s.dx,s.dy,s.length);
        h.length(bakeOrder.size());
        for(int strip:bakeOrder)h.value(strip);
    });
}
uint64_t sourceKey() {
    return component(report.sources,[](BakeKeyHash &h) {
        h.fields(firstWindowSource,effectiveLightingModel(settings));
        h.length(bakeSources.size());
        for(const auto &light:bakeSources)
            h.fields(light.x,light.y,light.z,light.radius,light.strength,light.directionality,
                     light.color,light.extent,light.nx,light.ny,light.beam);
        h.length(sourceGroups.size());
        for(auto group:sourceGroups)h.byte(group);
        h.length(bakeAreas.size());
        for(const auto &a:bakeAreas) {
            h.fields(a.low,a.high,a.radius,a.strength,a.color,a.z,a.clearance,a.ceiling,a.directionality);
            h.length(a.pieces.size());
            for(const auto &piece:a.pieces) {
                h.length(piece.size());
                for(const auto &vertex:piece)h.value(vertex);
            }
        }
    });
}
void flowBspKey(BakeKeyHash &h) {
    // R_PointInSubsector walks the loaded BSP and returns its leaf's sector.
    h.value(numnodes);
    for(int i=0;i<numnodes;++i) {
        const auto &node=nodes[i];
        h.fields(node.x,node.y,node.dx,node.dy,node.children);
    }
    h.value(numsubsectors);
    for(int i=0;i<numsubsectors;++i)h.value(int(subsectors[i].sector-sectors));
}
uint64_t sizeOf(const std::vector<CacheField> &fields) {
    uint64_t size=0;
    constexpr auto max=std::numeric_limits<size_t>::max();
    for(auto f:fields) {
        if(!f.width||f.width>f.stride||(f.count&&!f.data)||(f.gap&&!f.row))
            throw std::runtime_error("invalid cache field");
        if(!f.count)continue;
        const auto last=f.count-1;
        if(last>(max-f.width)/f.stride||
           (f.gap&&last/f.row>(max-f.width-last*f.stride)/f.gap)||
           f.count>(std::numeric_limits<uint64_t>::max()-size)/f.width)
            throw std::overflow_error("cache field size overflow");
        size+=f.count*f.width;
    }
    return size;
}
std::vector<uint8_t> pack(const std::vector<CacheField> &fields,size_t size) {
    std::vector<uint8_t> data;data.reserve(size);
    for(auto f:fields) {
        auto *p=static_cast<const uint8_t*>(f.data);
        if(f.stride==f.width&&!f.gap) {if(f.count)data.insert(data.end(),p,p+f.count*f.width);}
        else for(size_t i=0;i<f.count;++i)data.insert(data.end(),p+i*f.stride+(f.row?i/f.row*f.gap:0),p+i*f.stride+(f.row?i/f.row*f.gap:0)+f.width);
    }
    return data;
}
// Byte runs keep zero-filled atlases small, without a new compression dependency.
std::vector<uint8_t> encode(const std::vector<uint8_t> &data) {
    std::vector<uint8_t> out;
    for(size_t i=0;i<data.size();) {
        size_t run=1;while(run<128&&i+run<data.size()&&data[i+run]==data[i])++run;
        if(run>=3) {out.push_back(uint8_t(128+run-1));out.push_back(data[i]);i+=run;continue;}
        size_t begin=i;i+=run;
        while(i<data.size()&&i-begin<128) {
            if(i+2<data.size()&&data[i]==data[i+1]&&data[i]==data[i+2])break;
            ++i;
        }
        out.push_back(uint8_t(i-begin-1));out.insert(out.end(),data.begin()+begin,data.begin()+i);
    }
    return out;
}
bool decode(const std::vector<uint8_t> &in,std::vector<uint8_t> &out,size_t expected) {
    out.reserve(expected);
    for(size_t i=0;i<in.size();) {
        unsigned tag=in[i++],count=(tag&127)+1;
        if(count>expected-out.size())return false;
        if(tag&128) {if(i==in.size())return false;out.insert(out.end(),count,in[i++]);}
        else {if(count>in.size()-i)return false;out.insert(out.end(),in.begin()+i,in.begin()+i+count);i+=count;}
    }
    return out.size()==expected;
}
size_t evict(const fs::path &folder,uint64_t budget=limit) {
    struct Entry {fs::path path;fs::file_time_type time;uint64_t size;};std::vector<Entry> entries;uint64_t total=0;
    std::error_code ec;
    for(fs::recursive_directory_iterator i(folder,fs::directory_options::skip_permission_denied,ec),end;i!=end;i.increment(ec)) {
        if(ec){ec.clear();continue;}
        if(i->is_symlink(ec)){i.disable_recursion_pending();continue;}
        if(!i->is_regular_file(ec)||i->path().extension()!=".bake")continue;
        auto size=i->file_size(ec);if(ec){ec.clear();continue;}
        auto time=i->last_write_time(ec);if(ec){ec.clear();continue;}
        total+=size;entries.push_back({i->path(),time,size});
    }
    std::sort(entries.begin(),entries.end(),[](const Entry&a,const Entry&b){return a.time<b.time;});
    size_t removed=0;
    for(const auto &e:entries) {
        if(total<=budget)break;
        if(fs::last_write_time(e.path,ec)==e.time&&!ec&&fs::remove(e.path,ec)) {total-=e.size;++removed;}
        ec.clear();
    }
    return removed;
}
}
CacheField cacheWallChannel(std::vector<uint8_t> &cells,int height,int channel,size_t width,bool bounce) {
    return {cells.data()+(bounce?atlasWidth*4:0)+channel,(size_t)atlasWidth*height,4,width,(size_t)atlasWidth,(size_t)atlasWidth*4};
}
void beginBakeCacheSession(bool level) {
    static size_t generation=0;
    report={};
    report.id=++generation;
    fprintf(stderr,"[LIGHT CACHE] event=map_prepare map=%s enabled=%d format=%u key_schema=%u trigger=%s root=\"%s\"\n",
        mapName().c_str(),cacheEnabled(),format,keySchema,level?"level_setup":"lighting_update",
        cacheEnabled()?root().u8string().c_str():"");
}
void recordBakeGeometry(uint64_t start) {if(report.id)report.geometryMs+=elapsed(start);}
void recordBakeReconcile(uint64_t start) {if(report.id)report.reconcileMs+=elapsed(start);}
void endBakeCacheSession(uint64_t preparationStart,double mapMs) {
    if(!report.id)return;
    if(report.wrote)try {report.evicted=evict(root());}
    catch(const std::exception &e) {fprintf(stderr,"[LIGHT CACHE] event=eviction_error detail=\"%s\"\n",e.what());}
    fprintf(stderr,"[LIGHT CACHE] event=summary eligible=%zu restored=%zu calculated=%zu compute_dispatches=%llu uploaded=%zu hit_rate=%.1f stored_bytes=%llu write_failures=%zu verified=%zu evicted=%zu key_ms=%.3f lookup_ms=%.3f read_ms=%.3f calculate_ms=%.3f write_ms=%.3f upload_ms=%.3f geometry_ms=%.3f reconcile_ms=%.3f total_ms=%.3f\n",
        report.eligible,report.restored,report.calculated,(unsigned long long)report.dispatches,report.uploaded,
        report.eligible?100.*report.restored/report.eligible:0.,(unsigned long long)report.bytes,
        report.failures,report.verified,report.evicted,report.keyMs,report.lookupMs,report.readMs,
        report.calculateMs,report.writeMs,report.uploadMs,mapMs+report.geometryMs,report.reconcileMs,elapsed(preparationStart));
    report.id=0;
}
uint64_t bakeUpstreamIdentity(bool sunlight) {
    const auto &memo=sunlight?report.sunOutput:report.directOutput;
    if(report.id&&memo)return *memo;
    const size_t cells=(size_t)mapWidth*mapHeight;
    // Match the leading cache fields exactly when an upstream stage was
    // not just restored/calculated by this preparation pass.
    auto flat=sunlight?cacheChannel(contactCells,2,cells):cacheChannel(flatLightCells,0,cells*2,3);
    auto wall=sunlight?cacheWallChannel(wallBakeCells,atlasHeight,3,1):cacheWallChannel(wallBakeCells,atlasHeight,0,3);
    uint64_t digest=seed;
    const auto *data=static_cast<const uint8_t*>(flat.data);
    for(size_t i=0;i<flat.count;++i)digest=hashBytes(digest,data+i*flat.stride,flat.width);
    data=static_cast<const uint8_t*>(wall.data);
    for(int y=0;y<atlasHeight;++y)for(int x=0;x<atlasWidth;++x)
        digest=hashBytes(digest,data+4*((size_t)y*atlasStride+x),wall.width);
    return digest;
}
uint64_t BakeCache::payloadChecksum(const std::vector<uint8_t> &data,uint64_t &upstream) const {
    if((stage!="sun"&&stage!="lights_hf2")||fields.size()<2||data.empty())
        return hashBytes(seed,data.data(),data.size());
    const size_t prefix=fields[0].count*fields[0].width+fields[1].count*fields[1].width;
    upstream=hashBytes(seed,data.data(),prefix);
    return hashBytes(upstream,data.data()+prefix,data.size()-prefix);
}
BakeCache::BakeCache(const char *name,std::initializer_list<CacheField> output,
    const std::function<void(BakeKeyHash &)> &stageInputs)
    :stage(name),fields(output),session(report.id) {
    const auto start=SDL_GetTicksNS();
    try {
        expected=sizeOf(fields);
        if(!cacheEnabled()) {readResult=writeResult="CACHE_DISABLED";return;}
        if(expected>entryLimit) {readResult=writeResult="BYPASS_ENTRY_TOO_LARGE";return;}
        if(root().empty()) {readResult="READ_FAILED";writeResult="WRITE_FAILED";return;}
        geometry=geometryKey();
        if(stage!="grid_hf2"&&stage!="flow")atlas=atlasKey();
        if(stage=="lights_hf2"||stage=="grid_hf2")sources=sourceKey();
        BakeKeyHash extra;
        if(stage=="sun")extra.value(sunDirection);
        if(stage=="lights_hf2")hashGlassBakeInputs(extra);
        if(stage=="flow")flowBspKey(extra);
        if(stageInputs)stageInputs(extra);
        inputs=extra.result();

        BakeKeyHash outputLayout;
        outputLayout.length(fields.size());
        for(auto f:fields) {
            outputLayout.length(f.count);outputLayout.length(f.stride);outputLayout.length(f.width);
            outputLayout.length(f.row);outputLayout.length(f.gap);
        }
        layout=outputLayout.result();
        algorithm=algorithmFor(stage);
        BakeKeyHash identity;
        identity.text("HELLFORGE_CPU_BAKE");
        identity.fields(format,keySchema,gameepisode,gamemap,int(gamemode));
        identity.text(stage);
        identity.text(algorithm);
        identity.text(DOOM_BAKE_NUMERIC_ABI);
        const uint16_t endian=1;
        identity.byte(*reinterpret_cast<const uint8_t*>(&endian));
        identity.component(geometry);identity.component(atlas);identity.component(sources);
        identity.component(inputs);identity.component(layout);
        key=identity.result();
        path=(root()/(mapName()+"-m1")/(stage+"-"+hex(key)+".bake")).u8string();
        enabled=true;
        keyMs=elapsed(start);
    } catch(const std::bad_alloc &) {readResult=writeResult="RESOURCE_LIMIT";}
      catch(const std::overflow_error &) {readResult=writeResult="RESOURCE_LIMIT";}
      catch(const std::exception &) {readResult=writeResult="INVALID_INPUT";}
}
BakeCache::~BakeCache() {
    fprintf(stderr,"[LIGHT CACHE] stage=%s result=%s write=%s key=%016llx checksum=%016llx geometry=%016llx atlas=%016llx sources=%016llx inputs=%016llx layout=%016llx algorithm=%s expected_bytes=%llu stored_bytes=%llu entry_limit_bytes=%llu restored=%d calculated=%d uploaded=%d dispatches=%llu verification_dispatches=%llu key_ms=%.3f lookup_ms=%.3f read_ms=%.3f calculate_ms=%.3f write_ms=%.3f upload_ms=%.3f\n",
        stage.c_str(),readResult,writeResult,(unsigned long long)key,(unsigned long long)checksum,
        (unsigned long long)geometry,(unsigned long long)atlas,(unsigned long long)sources,
        (unsigned long long)inputs,(unsigned long long)layout,algorithm,
        (unsigned long long)expected,(unsigned long long)stored,(unsigned long long)entryLimit,
        hit&&!verifying,calculated&&!verifying,uploaded,(unsigned long long)(verifying?0:dispatches),
        (unsigned long long)(verifying?dispatches:0),keyMs,lookupMs,readMs,calculateMs,writeMs,uploadMs);
    if(!session||session!=report.id)return;
    if(hasOutputIdentity) {
        if(stage=="sun")report.sunOutput=outputIdentity;
        else if(stage=="lights_hf2")report.directOutput=outputIdentity;
    }
    ++report.eligible;
    report.restored+=hit&&!verifying;report.calculated+=calculated&&!verifying;
    report.uploaded+=uploaded;report.dispatches+=verifying?0:dispatches;
    report.verified+=verifying&&hit;
    report.bytes+=stored;
    report.failures+=strcmp(writeResult,"STORED")&&strcmp(writeResult,"NOT_ATTEMPTED")&&
        strcmp(writeResult,"CACHE_DISABLED")&&strcmp(writeResult,"BYPASS_ENTRY_TOO_LARGE");
    report.keyMs+=keyMs;report.lookupMs+=lookupMs;report.readMs+=readMs;
    report.calculateMs+=verifying?0:calculateMs;report.writeMs+=writeMs;report.uploadMs+=uploadMs;
}
namespace {
struct StageLabels {const char *restore,*calculate;};
StageLabels labels(const std::string &stage) {
    if(stage=="sun")return {"Restoring cached sunlight","Baking sunlight"};
    if(stage=="lights_hf2")return {"Restoring cached static lighting","Baking static lighting"};
    if(stage=="grid_hf2")return {"Restoring cached sprite lighting","Calculating sprite lighting"};
    if(stage=="bounce")return {"Restoring cached bounce lighting","Calculating bounce lighting"};
    if(stage=="ambient")return {"Restoring cached ambient lighting","Calculating ambient occlusion"};
    return {"Restoring cached sector light flow","Calculating sector light flow"};
}
}
bool BakeCache::restore() {
    hit=verifying=hasOutputIdentity=false;
    if(enabled)loadingLabel("Checking lighting cache");
    const auto start=SDL_GetTicksNS();
    if(enabled)hit=readEntry();
    readMs=enabled?std::max(0.,elapsed(start)-lookupMs):0;
    if(!hit||verifying) {
        loadingLabel(verifying?"Verifying cached lighting":labels(stage).calculate);
        loadingTick();
    }
    clock=SDL_GetTicksNS();
    return hit&&!verifying;
}
bool BakeCache::readEntry() {
    try {
        const auto start=SDL_GetTicksNS();
        const auto file=fs::u8path(path);
        std::ifstream in(file,std::ios::binary);
        lookupMs=elapsed(start);
        if(!in) {
            std::error_code ec;
            const auto status=fs::status(file,ec);
            readResult=status.type()==fs::file_type::not_found&&
                (!ec||ec==std::errc::no_such_file_or_directory)?"MISS_NO_FILE":"READ_FAILED";
            return false;
        }
        uint64_t header[6]={};
        in.read((char*)header,sizeof(header));
        if(!in||header[0]!=0x3145484341435444ull) {readResult="REJECTED_INVALID_HEADER";return false;}
        if(header[1]!=format) {readResult="REJECTED_FORMAT_MISMATCH";return false;}
        if(header[2]!=key) {readResult="REJECTED_KEY_MISMATCH";return false;}
        if(header[3]!=expected||header[4]>expected+expected/128+16) {readResult="REJECTED_SIZE_MISMATCH";return false;}
        in.seekg(0,std::ios::end);
        if(!in) {readResult="READ_FAILED";return false;}
        if(in.tellg()!=std::streamoff(sizeof(header)+header[4])) {readResult="REJECTED_SIZE_MISMATCH";return false;}
        loadingLabel(labels(stage).restore);
        loadingTick();
        in.seekg(sizeof(header));
        std::vector<uint8_t> compressed((size_t)header[4]),data;
        in.read((char*)compressed.data(),(std::streamsize)compressed.size());
        if(!in) {readResult="READ_FAILED";return false;}
        if(!decode(compressed,data,(size_t)expected)) {readResult="DECODE_FAILED";return false;}
        if(payloadChecksum(data,outputIdentity)!=header[5]) {readResult="REJECTED_CHECKSUM";return false;}
        // Publish only after the entire entry validates. No partial restoration.
        verifying=M_CheckParm((char*)"-verifybakecache")!=0;
        if(verifying)verification=std::move(data);
        else {
            size_t at=0;
            for(auto f:fields) {
                auto *p=static_cast<uint8_t*>(f.data);
                if(f.stride==f.width&&!f.gap) {
                    if(f.count)memcpy(p,data.data()+at,f.count*f.width);
                    at+=f.width*f.count;
                } else for(size_t i=0;i<f.count;++i) {
                    memcpy(p+i*f.stride+(f.row?i/f.row*f.gap:0),data.data()+at,f.width);
                    at+=f.width;
                }
            }
        }
        hasOutputIdentity=!verifying;
        std::error_code ec;
        fs::last_write_time(file,fs::file_time_type::clock::now(),ec);
        stored=sizeof(header)+header[4];checksum=header[5];readResult="HIT";
        return true;
    } catch(const std::bad_alloc &) {readResult="RESOURCE_LIMIT";}
      catch(const std::exception &) {readResult="READ_FAILED";}
    return false;
}
void BakeCache::beginUpload() {
    loadingLabel("Uploading lighting resources");
    loadingTick();
    clock=SDL_GetTicksNS();
}
void BakeCache::endUpload() {uploadMs=elapsed(clock);uploaded=true;}
void BakeCache::store() {
    calculated=dispatches!=0;
    calculateMs=clock?elapsed(clock):0;
    if(!enabled||(hit&&!verifying))return;
    fs::path temporary;
    const auto start=SDL_GetTicksNS();
    const char *failure="ENCODE_FAILED";
    try {
        auto data=pack(fields,(size_t)expected);
        if(verifying) {
            size_t offset=0,channel=0;
            for(auto field:fields) {
                size_t bytes=field.count*field.width;
                auto diff=std::mismatch(data.begin()+offset,data.begin()+offset+bytes,verification.begin()+offset);
                if(diff.first!=data.begin()+offset+bytes) {
                    fprintf(stderr,"[LIGHT CACHE] event=verify stage=%s key=%s result=MISMATCH channel=%zu element=%zu byte=%zu\n",
                        stage.c_str(),hex(key).c_str(),channel,(size_t)(diff.first-data.begin()-offset)/field.width,
                        (size_t)(diff.first-data.begin()-offset)%field.width);
                    hit=false;readResult="REJECTED_NUMERIC_MISMATCH";break;
                }
                offset+=bytes;++channel;
            }
            std::vector<uint8_t>().swap(verification);
            if(hit) {
                hasOutputIdentity=true;
                fprintf(stderr,"[LIGHT CACHE] event=verify stage=%s key=%s result=MATCH\n",stage.c_str(),hex(key).c_str());
                return;
            }
        }
        checksum=payloadChecksum(data,outputIdentity);
        hasOutputIdentity=true;
        auto compressed=encode(data);
        uint64_t header[]={0x3145484341435444ull,format,key,data.size(),compressed.size(),checksum};
        failure="WRITE_FAILED";
        fs::path destination=fs::u8path(path);
        fs::create_directories(destination.parent_path());
        std::random_device nonce;
        temporary=destination;
        temporary+="."+std::to_string(SDL_GetPerformanceCounter())+"."+std::to_string(nonce())+".tmp";
        std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
        out.write((char*)header,sizeof(header));
        out.write((char*)compressed.data(),(std::streamsize)compressed.size());
        out.flush();
#ifdef DOOM_TEST_DRIVER
        if(injectWriteFailure)out.setstate(std::ios::badbit);
#endif
        if(!out)throw std::runtime_error("write/flush failed");
        out.close();
        if(out.fail())throw std::runtime_error("close failed");
        failure="ATOMIC_PUBLISH_FAILED";
        if(!SDL_RenamePath(temporary.u8string().c_str(),path.c_str()))throw std::runtime_error(SDL_GetError());
        stored=sizeof(header)+compressed.size();writeResult="STORED";
        if(session&&session==report.id)report.wrote=true;
        else evict(root());
    } catch(const std::exception &e) {
        if(verifying) {hit=false;hasOutputIdentity=false;}
        std::error_code ec;
        if(!temporary.empty())fs::remove(temporary,ec);
        if(strcmp(writeResult,"STORED"))writeResult=failure;
        fprintf(stderr,"[LIGHT CACHE] event=storage_error stage=%s detail=\"%s\"\n",stage.c_str(),e.what());
    }
    writeMs=elapsed(start);
}

#ifdef DOOM_TEST_DRIVER
int BakeCache::selfTest() {
    int checks=0;
    auto require=[&](bool okay,const char *label){++checks;if(!okay)throw std::runtime_error(label);};
    try {
        std::vector<uint8_t> bytes(2048,0x7b);
        for(size_t i=0;i<bytes.size();i+=4)bytes[i]=(uint8_t)(i/4);
        auto original=bytes;
        BakeCache test("storage-test",{cacheChannel(bytes,0,512)});require(test.enabled,"cache enabled");
        auto make=[&](const char *name,const std::function<void(BakeKeyHash &)> &extra={}) {
            return BakeCache(name,{cacheChannel(bytes,0,512)},extra);
        };
        const auto ambient=make("ambient").key;
        const auto sun=make("sun").key;
        const auto flow=make("flow").key;
        auto height=bakeMap.sectors[0].ceiling;
        bakeMap.sectors[0].ceiling+=1;
        require(make("ambient").key!=ambient&&make("sun").key!=sun&&make("flow").key!=flow,
            "changed saved ray geometry invalidates geometry consumers");
        bakeMap.sectors[0].ceiling=height;
        require(make("ambient").key==ambient,"geometry key stable after restoring height");
        const auto azimuth=sunDirection[0];
        sunDirection[0]+=0.01f;
        require(make("sun").key!=sun&&make("ambient").key==ambient,"sun direction invalidates sun only");
        sunDirection[0]=azimuth;
        auto texture=texturetranslation[0];
        texturetranslation[0]^=1;
        require(make("sun").key==sun&&make("ambient").key==ambient&&make("flow").key==flow,
            "unused translation state does not invalidate resolved inputs");
        texturetranslation[0]=texture;
        auto direct=make("lights_hf2").key;
        auto grid=make("grid_hf2").key;
        const auto sourceCount=bakeSources.size();
        bakeSources.reserve(sourceCount+32);
        require(make("lights_hf2").key==direct,"vector capacity is not a semantic input");
        bakeSources.push_back({1,2,3,100,.5f,.4f,{1,.5f,.25f}});
        require(make("lights_hf2").key!=direct&&make("grid_hf2").key!=grid&&make("ambient").key==ambient,
            "source change invalidates direct and grid independently of ambient");
        auto withLight=make("lights_hf2").key;
        bakeSources.back().strength+=0.25f;
        require(make("lights_hf2").key!=withLight,"source strength enters production key");
        bakeSources.resize(sourceCount);
        auto bounceA=make("bounce",[](BakeKeyHash &h){h.component(1);}).key;
        auto bounceB=make("bounce",[](BakeKeyHash &h){h.component(2);}).key;
        require(bounceA!=bounceB,"upstream bytes enter bounce key");
        require(std::string(algorithmFor("sun"))!=algorithmFor("lights_hf2"),"algorithm domains separated");
        require(BakeCache("storage-test",{cacheChannel(bytes,0,511)}).key!=test.key,"output layout enters key");
        uint8_t dummy=0;
        BakeCache oversized("storage-test",{{&dummy,size_t(entryLimit+1),1,1}});
        require(!oversized.restore()&&oversized.expected==entryLimit+1&&
            !strcmp(oversized.readResult,"BYPASS_ENTRY_TOO_LARGE"),"oversized bypass");
        BakeCache overflowing("storage-test",{{&dummy,std::numeric_limits<size_t>::max(),4,1}});
        require(!overflowing.restore()&&!strcmp(overflowing.readResult,"RESOURCE_LIMIT"),"strided field overflow");
        int map=gamemap;++gamemap;BakeCache mapChange("storage-test",{cacheChannel(bytes,0,512)});
        require(mapChange.key!=test.key,"map identity");gamemap=map;
        test.store();std::ifstream input(fs::u8path(test.path),std::ios::binary);
        std::vector<char> valid((std::istreambuf_iterator<char>(input)),{});input.close();
        require(valid.size()>48,"stored entry");
        auto replace=[&](const std::vector<char>& content){std::ofstream out(fs::u8path(test.path),std::ios::binary|std::ios::trunc);out.write(content.data(),content.size());};
        std::fill(bytes.begin(),bytes.end(),0x7b);
        require(test.restore(),"round trip restore");require(bytes==original,"strided channels and untouched channels");
        auto unchanged=bytes;
        auto damaged=valid;damaged.back()^=7;replace(damaged);
        require(!test.restore()&&bytes==unchanged&&!strcmp(test.readResult,"REJECTED_CHECKSUM"),
            "checksum rejection is transactional");
        damaged=valid;damaged.resize(20);replace(damaged);
        require(!test.restore()&&bytes==unchanged&&!strcmp(test.readResult,"REJECTED_INVALID_HEADER"),"truncated header");
        damaged=valid;damaged.pop_back();replace(damaged);
        require(!test.restore()&&bytes==unchanged&&!strcmp(test.readResult,"REJECTED_SIZE_MISMATCH"),"truncated payload");
        damaged=valid;damaged[8]^=1;replace(damaged);
        require(!test.restore()&&bytes==unchanged,"obsolete version");
        damaged=valid;damaged[16]^=1;replace(damaged);
        require(!test.restore()&&bytes==unchanged,"wrong identity");
        damaged=valid;for(int i=32;i<40;++i)damaged[i]=char(0xff);replace(damaged);
        require(!test.restore()&&bytes==unchanged,"oversized allocation blocked");
        damaged=valid;damaged[24]^=1;replace(damaged);
        require(!test.restore()&&bytes==unchanged&&!strcmp(test.readResult,"REJECTED_SIZE_MISMATCH"),"wrong decoded length");
        damaged=valid;damaged.resize(49);uint64_t encodedLength=1;
        memcpy(damaged.data()+32,&encodedLength,sizeof(encodedLength));damaged[48]=char(0xff);replace(damaged);
        require(!test.restore()&&bytes==unchanged&&!strcmp(test.readResult,"DECODE_FAILED"),"decode failure is transactional");
        replace(valid);require(test.restore(),"recovery from corruption");
        BakeCache aborted("storage-test",{cacheChannel(bytes,0,512)});
        bytes[0]^=1;injectWriteFailure=true;aborted.store();injectWriteFailure=false;
        std::ifstream preserved(fs::u8path(test.path),std::ios::binary);
        std::vector<char> previous((std::istreambuf_iterator<char>(preserved)),{});preserved.close();
        require(previous==valid,"failed write preserves previous destination");std::copy(original.begin(),original.end(),bytes.begin());
        bool orphan=false;for(const auto &entry:fs::directory_iterator(fs::u8path(test.path).parent_path()))
            if(entry.path().extension()==".tmp")orphan=true;
        require(!orphan,"failed write removes sibling temporary");
        // A failed sibling write must preserve the existing destination.
        BakeCache fail("storage-test",{cacheChannel(bytes,0,512)});
        fs::path blocker=fs::u8path(test.path+".blocker");{std::ofstream out(blocker);out<<"blocker";}
        fail.path=(blocker/"entry.bake").u8string();fail.store();require(fs::file_size(blocker)==7,"write failure nonfatal");
        require(test.restore(),"previous entry preserved after failure");fs::remove(blocker);
        std::vector<uint8_t> random(4096);for(size_t i=0;i<random.size();++i)random[i]=(uint8_t)(i*31);
        std::vector<uint8_t> decoded;require(decode(encode(random),decoded,random.size())&&decoded==random,"literal encoding");
        random.assign(4096,0);decoded.clear();require(decode(encode(random),decoded,random.size())&&decoded==random,"run encoding");
        decoded.clear();require(!decode({255},decoded,128),"invalid run encoding");
        fs::path lru=fs::u8path(test.path+".lru");fs::create_directories(lru);
        {std::ofstream out(lru/"old.bake");out<<std::string(80,'a');}
        fs::last_write_time(lru/"old.bake",fs::file_time_type::clock::now()-std::chrono::hours(1));
        {std::ofstream out(lru/"new.bake");out<<std::string(80,'b');}
        evict(lru,100);require(!fs::exists(lru/"old.bake")&&fs::exists(lru/"new.bake"),"LRU budget");
        fs::remove(lru/"new.bake");fs::remove(lru);

        // Competing writers use the same key while a reader tries the file.
        // Every accepted snapshot must be an entire valid publication.
        auto first=original,second=original;
        first[0]^=0x31;second[0]^=0x73;
        BakeCache writerA("storage-test",{cacheChannel(first,0,512)});
        BakeCache writerB("storage-test",{cacheChannel(second,0,512)});
        BakeCache reader("storage-test",{cacheChannel(bytes,0,512)});
        std::atomic<bool> start{false};
        auto publish=[&](BakeCache &writer) {
            while(!start.load())SDL_Delay(1);
            for(int i=0;i<4;++i)writer.store();
        };
        std::thread a(publish,std::ref(writerA));
        std::thread b;
        try {b=std::thread(publish,std::ref(writerB));}
        catch(...) {start=true;a.join();throw;}
        start=true;
        bool validSnapshots=true;
        for(int i=0;i<8;++i) {
            if(reader.restore()&&bytes!=original&&bytes!=first&&bytes!=second)validSnapshots=false;
            SDL_Delay(1);
        }
        a.join();b.join();
        require(validSnapshots,"concurrent readers only publish complete snapshots");
        require(reader.restore()&&(bytes==first||bytes==second),"concurrent publication produces a valid entry");
        fs::remove(fs::u8path(test.path));
        fprintf(stderr,"CACHE STORAGE PASS %d checks\n",checks);return 1;
    } catch(const std::exception &e){fprintf(stderr,"CACHE STORAGE FAIL after %d checks: %s\n",checks,e.what());return 0;}
}
#endif
} // namespace doom3d

#ifdef DOOM_TEST_DRIVER
extern "C" int I_TestBakeCacheStorage() {return doom3d::BakeCache::selfTest();}
#endif
