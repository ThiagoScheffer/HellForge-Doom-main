/* Versioned disk cache of numeric bake channels. A hit skips tracing only:
   source lists, masks, lamp groups, sky metadata and GPU uploads still run. */
#include "internal.h"
#ifndef DOOM_BAKE_CODE_HASH
#define DOOM_BAKE_CODE_HASH __DATE__ " " __TIME__
#endif
#include <filesystem>
#include <fstream>
#include <system_error>
#include <limits>

namespace doom3d {
namespace {
namespace fs=std::filesystem;
constexpr uint64_t seed=14695981039346656037ull;
constexpr uint64_t limit=1024ull*1024*1024;
constexpr uint64_t entryLimit=64ull*1024*1024;
constexpr uint32_t format=1;
#ifdef DOOM_TEST_DRIVER
bool injectWriteFailure=false;
#endif
uint64_t hashBytes(uint64_t h,const void *data,size_t size) {
    const auto *p=static_cast<const uint8_t*>(data);
    for(size_t i=0;i<size;++i)h=(h^p[i])*1099511628211ull;
    return h;
}
template<class T> void value(uint64_t &h,const T &v) {h=hashBytes(h,&v,sizeof(v));}
template<class T> void values(uint64_t &h,const std::vector<T> &v) {
    static_assert(std::is_trivially_copyable<T>::value,"No pointers or containers in cache inputs");
    value(h,v.size());h=hashBytes(h,v.data(),v.size()*sizeof(T));
}
std::string hex(uint64_t h) {char b[17];snprintf(b,sizeof(b),"%016llx",(unsigned long long)h);return b;}
fs::path root() {
    if(const char *p=SDL_getenv("HELLFORGE_MAP_CACHE"))return fs::u8path(p);
    if(const char *p=SDL_getenv("TUIN_MAP_CACHE"))return fs::u8path(p);
    char *p=SDL_GetPrefPath(nullptr,"DOOM-HellforgeLauncher");
    if(!p)return {};
    fs::path result=fs::u8path(p)/"Cache"/"Maps";SDL_free(p);return result;
}
uint64_t resources() {
    static uint64_t digest=0;
    if(digest)return digest;
    uint64_t h=seed;
    // Ordered resolved resource directory, not filenames/timestamps or map name.
    std::vector<uint8_t> bytes;
    for(int i=0;i<numlumps;++i) {
        h=hashBytes(h,lumpinfo[i].name,8);int size=W_LumpLength(i);value(h,size);
        if(size<0)throw std::runtime_error("invalid resource length");
        bytes.resize(size);if(size)W_ReadLump(i,bytes.data());h=hashBytes(h,bytes.data(),bytes.size());
    }
    digest=h?h:1;return digest;
}
static void hashResolvedEmission(uint64_t &h) {
    if(std::none_of(images.begin(),images.end(),[](const auto &entry){return entry.second.explicitEmissionHash!=0;}))return;
    std::map<int,uint64_t> used;
    auto include=[&](int key) {
        const auto image=images.find(key);
        if(image!=images.end()&&image->second.explicitEmissionHash)used.emplace(key,image->second.explicitEmissionHash);
    };
    for(int i=0;i<numsides;++i)
        for(int texture:{sides[i].toptexture,sides[i].bottomtexture,sides[i].midtexture})
            if(texture>0)include(texturetranslation[texture]);
    for(int i=0;i<numsectors;++i)
        for(int pic:{sectors[i].floorpic,sectors[i].ceilingpic})
            if(pic!=skyflatnum)include(-1-firstflat-flattranslation[pic]);
    for(const auto &[key,mask]:used) {value(h,key);value(h,mask);}
}
uint64_t worldKey(const std::string &stage) {
    uint64_t h=resources();value(h,format);
    const char algorithm[]=DOOM_BAKE_CODE_HASH;h=hashBytes(h,algorithm,sizeof(algorithm));
    h=hashBytes(h,stage.data(),stage.size());value(h,gameepisode);value(h,gamemap);
    const auto &mood=levelMood();value(h,mood.outdoor);value(h,mood.sun);value(h,mood.shineOut);value(h,mood.skyTint);
    value(h,sunElevation);value(h,skytexture);value(h,skyflatnum);
    h=hashBytes(h,flattranslation,(size_t)numflats*sizeof(*flattranslation));
    h=hashBytes(h,texturetranslation,(size_t)numtextures*sizeof(*texturetranslation));
    value(h,mapWidth);value(h,mapHeight);value(h,mapCell);value(h,mapOrigin);value(h,atlasHeight);
    const int options[]={settings.sun,settings.bakedLights,settings.emissive,settings.windowLight,
        settings.bakeOnlyLights,settings.gridSpriteLight,settings.thingShadows,settings.bounce,
        settings.skyLight,settings.bakedAO,settings.lightFlow,settings.dimLitRooms};
    h=hashBytes(h,options,sizeof(options));value(h,settings.windowStrength);value(h,settings.unlitDarkening);
    value(h,effectiveLightingModel(settings));
    if(settings.emissive&&(stage=="lights_hf2"||stage=="grid_hf2"||stage=="bounce"))hashResolvedEmission(h);
    for(int i=0;i<numsectors;++i) {
        const auto &s=sectors[i];value(h,s.floorheight);value(h,s.ceilingheight);value(h,s.floorpic);value(h,s.ceilingpic);
        value(h,s.lightlevel);value(h,s.special);value(h,s.tag);
    }
    for(int i=0;i<numsides;++i) {
        const auto &s=sides[i];value(h,s.textureoffset);value(h,s.rowoffset);
        value(h,s.toptexture);value(h,s.bottomtexture);value(h,s.midtexture);
    }
    for(const auto &s:bakeMap.sectors) {value(h,s.floor);value(h,s.ceiling);value(h,s.sky);}
    for(const auto &l:bakeMap.lines) {
        value(h,l.ax);value(h,l.ay);value(h,l.bx);value(h,l.by);value(h,l.front);value(h,l.back);
        const auto &m=l.mask;value(h,m.width);value(h,m.height);value(h,m.u);value(h,m.direction);
        value(h,m.anchor);value(h,m.bottom);value(h,m.top);
        if(m.pixels)h=hashBytes(h,m.pixels,(size_t)m.width*m.height*2);
    }
    for(const auto &t:bakeMap.things) {
        value(h,t.x);value(h,t.y);value(h,t.top);value(h,t.left);value(h,t.width);value(h,t.height);
        h=hashBytes(h,t.pixels,(size_t)t.width*t.height*2);
    }
    values(h,bakeSources);values(h,sourceGroups);values(h,bakeSourceKeys);
    // Areas contain polygon vectors, so serialize their numeric members explicitly.
    for(const auto &a:bakeAreas) {
        value(h,a.low);value(h,a.high);value(h,a.radius);value(h,a.strength);value(h,a.color);
        value(h,a.z);value(h,a.clearance);value(h,a.ceiling);value(h,a.directionality);for(const auto &piece:a.pieces)values(h,piece);
    }
    for(const auto &s:surfaceLights) {
        value(h,s.key);value(h,s.x);value(h,s.y);value(h,s.z);value(h,s.radius);value(h,s.strength);
        value(h,s.color);value(h,s.extent);value(h,s.flickerSector);
        bool broken=tileShotOut(s.key);value(h,broken);
    }
    if(settings.sun&&sunBaked) {value(h,sunLevel);value(h,sunTint);value(h,sunDirection);value(h,sunElevation);}
    return h;
}
uint64_t sizeOf(const std::vector<CacheField> &fields) {
    uint64_t size=0;
    for(auto f:fields) {
        if(f.width>f.stride||f.count>entryLimit/std::max(size_t(1),f.width))throw std::runtime_error("cache field exceeds budget");
        size+=f.count*f.width;if(size>entryLimit)throw std::runtime_error("cache entry exceeds budget");
    }
    return size;
}
std::vector<uint8_t> pack(const std::vector<CacheField> &fields) {
    std::vector<uint8_t> data;data.reserve((size_t)sizeOf(fields));
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
void evict(const fs::path &folder,uint64_t budget=limit) {
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
    for(const auto &e:entries) {if(total<=budget)break;if(fs::remove(e.path,ec))total-=e.size;ec.clear();}
}
}
CacheField cacheWallChannel(std::vector<uint8_t> &cells,int height,int channel,size_t width,bool bounce) {
    return {cells.data()+(bounce?atlasWidth*4:0)+channel,(size_t)atlasWidth*height,4,width,(size_t)atlasWidth,(size_t)atlasWidth*4};
}
BakeCache::BakeCache(const char *name,std::initializer_list<CacheField> output):stage(name),fields(output) {
    if(!settings.mapLightCache||M_CheckParm((char*)"-nobakecache"))return;
    try {
        start=SDL_GetTicksNS();key=worldKey(stage);
        // The reconstructed layout determines lengths before any disk allocation.
        for(auto f:fields){value(key,f.count);value(key,f.stride);value(key,f.width);value(key,f.row);value(key,f.gap);}
        sizeOf(fields);
        char map[24];if(gamemode==commercial)snprintf(map,sizeof(map),"MAP%02d",gamemap);else snprintf(map,sizeof(map),"E%dM%d",gameepisode,gamemap);
        fs::path folder=root();if(folder.empty())return;
        path=(folder/(std::string(map)+"-"+hex(resources()))/(stage+"-"+hex(key)+".bake")).u8string();enabled=true;
    } catch(const std::exception &e) {fprintf(stderr,"3D cache: %s unavailable: %s\n",name,e.what());}
}
bool BakeCache::restore() {
    if(!enabled)return false;
    try {
        std::ifstream in(fs::u8path(path),std::ios::binary);
        if(!in){fprintf(stderr,"3D cache: %s miss (no matching entry).\n",stage.c_str());return false;}
        uint64_t header[6]={};in.read((char*)header,sizeof(header));
        uint64_t expected=sizeOf(fields);
        if(!in||header[0]!=0x3145484341435444ull||header[1]!=format||header[2]!=key||header[3]!=expected||header[4]>expected+expected/128+16)
            throw std::runtime_error("header/version/layout mismatch");
        in.seekg(0,std::ios::end);if(in.tellg()!=std::streamoff(sizeof(header)+header[4]))throw std::runtime_error("file length mismatch");
        in.seekg(sizeof(header));std::vector<uint8_t> compressed((size_t)header[4]),data;
        in.read((char*)compressed.data(),(std::streamsize)compressed.size());
        if(!in||!decode(compressed,data,(size_t)expected)||hashBytes(seed,data.data(),data.size())!=header[5])throw std::runtime_error("payload/checksum mismatch");
        // Publish only after the entire entry validates. No partial restoration.
        size_t at=0;
        for(auto f:fields) {
            auto *p=static_cast<uint8_t*>(f.data);
            if(f.stride==f.width&&!f.gap){if(f.count)memcpy(p,data.data()+at,f.count*f.width);at+=f.count*f.width;}
            else for(size_t i=0;i<f.count;++i){memcpy(p+i*f.stride+(f.row?i/f.row*f.gap:0),data.data()+at,f.width);at+=f.width;}
        }
        std::error_code ec;fs::last_write_time(fs::u8path(path),fs::file_time_type::clock::now(),ec);
        hit=true;fprintf(stderr,"3D cache: %s hit; checksum %s; restored in %.1f ms.\n",stage.c_str(),hex(header[5]).c_str(),(SDL_GetTicksNS()-start)/1e6);return true;
    } catch(const std::exception &e) {fprintf(stderr,"3D cache: %s rejected (%s); baking normally.\n",stage.c_str(),e.what());return false;}
}
void BakeCache::store() {
    if(!enabled||hit)return;
    fs::path temporary;
    try {
        double bakeMs=(SDL_GetTicksNS()-start)/1e6;uint64_t writeStart=SDL_GetTicksNS();
        auto data=pack(fields),compressed=encode(data);
        uint64_t header[]={0x3145484341435444ull,format,key,data.size(),compressed.size(),hashBytes(seed,data.data(),data.size())};
        fs::path destination=fs::u8path(path);fs::create_directories(destination.parent_path());
        temporary=destination;temporary+=std::string(".")+std::to_string(SDL_GetPerformanceCounter())+".tmp";
        std::ofstream out(temporary,std::ios::binary|std::ios::trunc);out.write((char*)header,sizeof(header));out.write((char*)compressed.data(),(std::streamsize)compressed.size());out.flush();
#ifdef DOOM_TEST_DRIVER
        if(injectWriteFailure)out.setstate(std::ios::badbit);
#endif
        if(!out)throw std::runtime_error("write/flush failed");out.close();if(out.fail())throw std::runtime_error("close failed");
        if(!SDL_RenamePath(temporary.u8string().c_str(),path.c_str()))throw std::runtime_error(SDL_GetError());
        evict(root());
        fprintf(stderr,"3D cache: %s stored; checksum %s; bake %.1f ms, write %.1f ms, %zu bytes.\n",stage.c_str(),hex(header[5]).c_str(),bakeMs,(SDL_GetTicksNS()-writeStart)/1e6,compressed.size());
    } catch(const std::exception &e) {std::error_code ec;if(!temporary.empty())fs::remove(temporary,ec);fprintf(stderr,"3D cache: %s write skipped (%s).\n",stage.c_str(),e.what());}
    fflush(stderr);
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
        {
            Image &image=lumpImage(firstflat+flattranslation[sectors[0].floorpic],true);
            struct RestoreMask {
                Image &image;
                uint64_t original=image.explicitEmissionHash;
                ~RestoreMask() {image.explicitEmissionHash=original;}
            } restore{image};
            image.explicitEmissionHash=0;
            const auto missing=worldKey("lights_hf2");
            std::array<uint8_t,64*64> mask={};
            image.explicitEmissionHash=doom_material_light::explicitMaskIdentity(mask.data(),mask.size(),64,64);
            const auto black=worldKey("lights_hf2");
            mask.front()=255;
            image.explicitEmissionHash=doom_material_light::explicitMaskIdentity(mask.data(),mask.size(),64,64);
            const auto bright=worldKey("lights_hf2");
            require(missing!=black&&black!=bright&&missing!=bright,"resolved mask contents invalidate the same material");
            require(bright==worldKey("lights_hf2"),"resolved mask identity is deterministic");
        }
        {
            struct RestoreSettings {
                Settings original=settings;
                ~RestoreSettings() {settings=original;}
            } restore;
            settings.accelerated=settings.bakedLights=settings.emissive=1;
            settings.lightingModel=doom_lighting_model::Enhanced;
            const auto enhanced=worldKey("lighting-model-test");
            settings.lightingModel=doom_lighting_model::Hybrid;
            const auto hybrid=worldKey("lighting-model-test");
            settings.lightingModel=doom_lighting_model::Physical;
            const auto physical=worldKey("lighting-model-test");
            require(enhanced!=hybrid&&enhanced!=physical&&hybrid!=physical,
                "effective illumination models have distinct cache keys");
            settings.bakedLights=0;
            const auto fallback=worldKey("lighting-model-test");
            settings.lightingModel=doom_lighting_model::Enhanced;
            require(fallback==worldKey("lighting-model-test"),"lighting prerequisites determine cache identity");
        }
        float strength=settings.windowStrength;settings.windowStrength+=0.25f;
        BakeCache settingChange("storage-test",{cacheChannel(bytes,0,512)});
        require(settingChange.key!=test.key,"settings invalidate identity");settings.windowStrength=strength;
        auto height=sectors[0].ceilingheight;sectors[0].ceilingheight+=FRACUNIT;
        BakeCache worldChange("storage-test",{cacheChannel(bytes,0,512)});
        require(worldChange.key!=test.key,"saved world heights invalidate identity");sectors[0].ceilingheight=height;
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
        require(!test.restore()&&bytes==unchanged,"checksum rejection is transactional");
        damaged=valid;damaged.resize(20);replace(damaged);
        require(!test.restore()&&bytes==unchanged,"truncated header");
        damaged=valid;damaged.pop_back();replace(damaged);
        require(!test.restore()&&bytes==unchanged,"truncated payload");
        damaged=valid;damaged[8]^=1;replace(damaged);
        require(!test.restore()&&bytes==unchanged,"obsolete version");
        damaged=valid;damaged[16]^=1;replace(damaged);
        require(!test.restore()&&bytes==unchanged,"wrong identity");
        damaged=valid;for(int i=32;i<40;++i)damaged[i]=char(0xff);replace(damaged);
        require(!test.restore()&&bytes==unchanged,"oversized allocation blocked");
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
        fs::remove(lru/"new.bake");fs::remove(lru);fs::remove(fs::u8path(test.path));
        fprintf(stderr,"CACHE STORAGE PASS %d checks\n",checks);return 1;
    } catch(const std::exception &e){fprintf(stderr,"CACHE STORAGE FAIL after %d checks: %s\n",checks,e.what());return 0;}
}
#endif
} // namespace doom3d

#ifdef DOOM_TEST_DRIVER
extern "C" int I_TestBakeCacheStorage() {return doom3d::BakeCache::selfTest();}
#endif
