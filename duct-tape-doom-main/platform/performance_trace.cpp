#include "performance_trace.h"
#include "build_version.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

namespace doom3d::performance {
namespace {
namespace fs = std::filesystem;
constexpr uint64_t ms=1000000;
constexpr unsigned historyLimit=2048, incidentLimit=32, settingsLimit=16384;
template<class T, size_t N> struct Ring {
    std::array<T,N> values{};
    size_t next=0,count=0;
    uint64_t dropped=0;
    void push(const T &v) {
        values[next]=v;
        next=(next+1)%N;
        if(count<N)++count;
        else ++dropped;
    }
    const T &at(size_t i) const {return values[(next+N-count+i)%N];}
};
struct Key {
    std::array<int,7> bins;
    unsigned level=0;
    char map[16];
    bool operator==(const Key &b) const {return level==b.level&&bins==b.bins&&!std::strcmp(map,b.map);}
};
Key keyFor(const Frame &f) {
    Key key{};
    std::memcpy(key.map,f.map,sizeof(key.map));
    key.level=f.level;
    auto bin=[](double value,double step) {
        if(!std::isfinite(value))return 0;
        const auto bounds=std::numeric_limits<int>{};
        return int(std::clamp(std::floor(value/step),double(bounds.min()),double(bounds.max())));
    };
    const double yaw=std::isfinite(f.yaw)?std::fmod(f.yaw,360.0):0;
    key.bins={f.cameraSector,bin(f.eye[0],64),bin(f.eye[1],64),bin(f.eye[2],64),
              (bin(yaw+7.5,15)+24)%24,bin(f.pitch+7.5,15),f.subsector};
    return key;
}
struct Snapshot {
    uint64_t serial=0,time=0;
    Evidence evidence{};
};
struct Stats {
    uint64_t count=0;
    double elapsed=0;
    float maximum=0;
    void add(float interval) {
        ++count;elapsed+=interval;maximum=std::max(maximum,interval);
    }
    double fps() const {return elapsed>0?1000*count/elapsed:0;}
};
struct HotspotSegment {
    Key key{};
    uint64_t first=0,last=0,begin=0,end=0,peak=0;
    Stats stats;
    void add(const Frame &f) {
        last=f.serial;end=f.time;
        if(!peak||f.interval>stats.maximum)peak=f.serial;
        stats.add(f.interval);
    }
};
enum EvidenceSlot { Comparison, Start, Peak, Recovery, Attribution, SlotCount };
struct Incident {
    uint64_t id=0,start=0,end=0,recovery=0,recoveryStart=0,lastDegraded=0;
    uint64_t firstSerial=0,lastDegradedSerial=0,endSerial=0,preNs=0,segmentOmitted=0;
    const char *termination="in_progress";
    Frame trigger{},worst{};
    unsigned reason=0;
    int comparisonTriangles=-1;
    uint64_t omittedPre=0,omittedActive=0;
    bool detailed=false,recovering=false;
    double budget=0;
    Stats active, degraded, captured, contextRecovery, post;
    std::array<Snapshot,SlotCount> evidence{};
    std::array<HotspotSegment,32> hotspots{};
    size_t hotspotCount=0;
    Key proposedKey{};
    uint64_t proposedSince=0;
    std::array<Frame,144> first{};
    size_t firstCount=0;
    Ring<Frame,256> tail;
    void keep(const Frame &f) {
        if(firstCount<first.size())first[firstCount++]=f;
        else tail.push(f);
    }
    void measure(const Frame &f) {
        active.add(f.interval);
        if(f.interval>budget/.9) {
            degraded.add(f.interval);lastDegraded=f.time;lastDegradedSerial=f.serial;
        }
        if(!worst.serial||f.interval>worst.interval)worst=f;
        const auto k=keyFor(f);
        if(hotspotCount&&hotspots[hotspotCount-1].key==k)proposedSince=0;
        else if(hotspotCount&&(!proposedSince||!(proposedKey==k))) {
            proposedSince=f.time;proposedKey=k;
        }
        if(!hotspotCount||(proposedSince&&f.time-proposedSince>=100*ms)) {
            if(hotspotCount<hotspots.size()) {
                auto &spot=hotspots[hotspotCount++];
                spot.key=k;spot.first=f.serial;spot.begin=f.time;
            } else ++segmentOmitted;
            proposedSince=0;
        }
        if(hotspotCount==hotspots.size()&&!(hotspots[hotspotCount-1].key==k))return;
        hotspots[hotspotCount-1].add(f);
    }
};
struct Event {uint64_t time=0;char name[32]{};unsigned before=0,after=0;};
struct Revision {uint64_t time;unsigned id,previous;std::string settings;bool truncated;};

struct Report {
    Config config;
    int64_t startUtc=0;
    uint64_t number=0,droppedIncidents=0,droppedExports=0,droppedRevisions=0,overwrittenHistory=0;
    std::string backend,gpu,presentation,platform,iwad,addons;
    int vsync=-1;
    Ring<Event,512> events;
    uint64_t observed=0,eligible=0,historyRetained=0,firstSerial=0,lastSerial=0;
    std::vector<Revision> revisions;
    std::vector<Incident> incidents;
    std::vector<Frame> recent;
};
constexpr size_t retainedAndExportBytes=2*((incidentLimit+1)*sizeof(Incident)+sizeof(Report)+
    48*settingsLimit+historyLimit*sizeof(Frame))+8*sizeof(Snapshot)+maxMaskJobs*sizeof(MaskWork);
static_assert(retainedAndExportBytes<16*1024*1024,"Performance tracing memory budget");

std::string json(const std::string &value) {
    std::string out="\"";
    for(unsigned char c:value) {
        if(c=='"'||c=='\\'){out+='\\';out+=char(c);}
        else if(c<32) {
            const char *hex="0123456789abcdef";
            out+="\\u00";out+=hex[c>>4];out+=hex[c&15];
        } else out+=char(c);
    }
    return out+'"';
}
std::string csv(const char *value) {
    std::string out="\"";
    for(;*value;++value){if(*value=='"')out+='"';out+=*value;}
    return out+'"';
}
const char *classification(const Frame &f) {
    if(f.state&(Loading|Targets))return "RESOURCE_REBUILD";
    if(f.interval<=0)return "UNKNOWN";
    if(f.upload>f.interval*.4f)return "CPU_UPLOAD";
    if(f.preparation[Geometry]>f.interval*.4f)return "CPU_GEOMETRY";
    if(f.preparation[Relight]+f.preparation[Masks]+f.preparation[Packing]>f.interval*.4f)return "CPU_LIGHTING";
    if(f.acquire>f.interval*.4f)return "PRESENTATION_WAIT";
    return "UNKNOWN";
}
fs::path root(){return fs::absolute("Diagnostics/Performance");}

void writeFrame(std::ostream &out,const Frame &f,uint64_t incident,const char *phase,unsigned hotspot=0) {
    out<<incident<<','<<phase<<','<<f.serial<<','<<f.time<<','<<f.level<<','<<f.revision<<','<<f.segment<<','
       <<csv(f.map)<<','<<f.gameTic<<','<<f.levelTic;
    auto time=[&](float value){out<<',';if(value>=0&&std::isfinite(value))out<<value;};
    for(float value:{f.interval,f.outside,f.loading,f.targets,f.acquire,f.prepare,f.upload,f.encode,f.submit})time(value);
    for(float value:f.preparation)time(value);
    out<<','; // GPU execution time is unavailable.
    if(f.hasView) {
        for(float value:f.player)out<<','<<value;
        for(float value:f.eye)out<<','<<value;
        out<<','<<f.yaw<<','<<f.pitch<<','<<f.fovX<<','<<f.fovY<<','<<f.playerSector<<','<<f.cameraSector<<','
           <<f.subsector<<','<<csv(f.floor)<<','<<csv(f.ceiling)<<','<<f.floorHeight<<','<<f.ceilingHeight;
    } else for(int i=0;i<17;++i)out<<',';
    out<<','<<f.width<<','<<f.height<<','<<f.outputWidth<<','<<f.outputHeight<<','<<f.samples<<','<<f.featureFlags<<','
       <<f.passes<<','<<f.state<<','<<f.probes;
    for(unsigned n:f.lights)out<<','<<n;
    out<<','<<f.blockers<<','<<f.slicedBlockers<<','<<f.mistVertices/3<<','<<f.mistVertices<<','<<f.fogLights<<','<<f.particleVertices;
    for(int n:{f.triangles,f.subsectors,f.rejectedSubsectors,f.lines,f.rejectedLines}){out<<',';if(n>=0)out<<n;}
    out<<',';if(hotspot)out<<hotspot;
    out<<','<<(f.state==0&&f.hasView&&f.interval>0?"eligible":"context")<<",unsupported_sdl_gpu_api";
    const auto &v=f.visibility;
    if(v.xy>=0) {
        const bool requested=!(v.reasons&CullForcedOff);
        for(int value:{int(requested),int(v.enabled),int(v.reasons),int(v.valid),v.xy,int(v.valid&&v.enabled&&v.xy>0),
                       1,v.spatial,int(v.shared),f.subsectors,f.subsectors-f.rejectedSubsectors,f.lines,f.lines-f.rejectedLines,
                       v.floors,v.ceilings,v.walls,v.sky,v.rejectedFloors,v.rejectedCeilings,v.rejectedWalls})out<<','<<value;
        out<<','<<(v.solid?"solid":"bsp");
    } else for(int n=0;n<21;++n)out<<',';
    auto mask=[&](const MaskWork &m){
        if(f.maskAvailable) {
            out<<','<<m.processed<<','<<m.lit<<','<<m.pairs<<','<<m.surface<<','<<m.decoration<<','<<m.maximum<<',';
            if(m.processed)out<<double(m.pairs)/m.processed;
            out<<',';
            if(m.lit)out<<double(m.pairs)/m.lit;
        } else for(int n=0;n<8;++n)out<<',';
    };
    mask(f.masks[0]);mask(f.masks[1]);
    time(f.traceCollection);time(f.traceSnapshot);
    out<<'\n';
}
std::string changes(const std::string &before,const std::string &after) {
    auto parse=[](const std::string &text) {
        std::vector<std::pair<std::string,std::string>> values;
        std::istringstream in(text);
        std::string key,value;
        while(in>>key>>value)values.emplace_back(key,value);
        std::sort(values.begin(),values.end());
        return values;
    };
    auto old=parse(before),next=parse(after);
    std::string out="[";
    for(const auto &v:next) {
        auto it=std::lower_bound(old.begin(),old.end(),v.first,
            [](const auto &entry,const std::string &key){return entry.first<key;});
        const bool present=it!=old.end()&&it->first==v.first;
        if(present&&it->second==v.second)continue;
        if(out.size()>1)out+=',';
        out+="{\"key\":"+json(v.first)+",\"previous\":"+(present?json(it->second):"null")+
            ",\"value\":"+json(v.second)+"}";
    }
    return out+"]";
}

void writeReport(const Report &r,const fs::path &folder) {
    fs::create_directories(folder);
    auto file=[&](const char *name){
        std::ofstream out(folder/name,std::ios::binary);
        out.exceptions(std::ios::badbit|std::ios::failbit);
        out.imbue(std::locale::classic());
        out<<std::setprecision(8);
        return out;
    };
    auto incidents=file("incidents.json"), attribution=file("attribution.jsonl");
    auto session=file("session.json"), summary=file("summary.txt"), cfg=file("settings.cfg"),
         frames=file("frames.csv"), events=file("events.jsonl"), hotspots=file("hotspots.json"),
         lights=file("lights.csv");
    session<<"{\n\"schema_version\":2,\"session_start_unix_ns\":"<<r.startUtc<<",\"export\":"<<r.number
           <<",\"engine\":"<<json(DOOM_ENGINE_VERSION)<<",\"platform\":"<<json(r.platform)
           <<",\"backend\":"<<json(r.backend)<<",\"gpu\":"<<(r.gpu.empty()?"null":json(r.gpu))
           <<",\"presentation\":"<<json(r.presentation)<<",\"vsync\":"<<(r.vsync<0?"null":r.vsync?"true":"false")
           <<",\"iwad\":"<<json(r.iwad)<<",\"addons\":["<<r.addons<<"],\"gpu_execution_ms\":null"
           <<",\"target_fps\":"<<r.config.target<<",\"mode\":"<<r.config.mode<<",\"slowdown_ms\":"<<r.config.duration
           <<",\"detailed\":"<<(r.config.detailed?"true":"false")<<",\"auto_export\":"<<(r.config.autoExport?"true":"false")
           <<",\"dropped_incidents\":"<<r.droppedIncidents<<",\"dropped_exports\":"<<r.droppedExports
           <<",\"dropped_revisions\":"<<r.droppedRevisions<<",\"dropped_events\":"<<r.events.dropped
           <<",\"dropped_light_snapshots\":"<<0<<",\"history_overwritten\":"<<r.overwrittenHistory;
    session<<",\"gpu_timing_status\":\"unsupported_sdl_gpu_api\",\"sdl_version\":"<<SDL_GetVersion()
           <<",\"retained_and_export_budget_bytes\":"<<retainedAndExportBytes
           <<",\"frames_observed\":"<<r.observed<<",\"frames_eligible\":"<<r.eligible
           <<",\"frames_retained\":"<<r.historyRetained<<",\"history_wrap_evictions\":"<<r.overwrittenHistory
           <<",\"history_first_serial\":"<<r.firstSerial<<",\"history_last_serial\":"<<r.lastSerial;
    const Frame *last=r.recent.empty()?(r.incidents.empty()?nullptr:&r.incidents.back().worst):&r.recent.back();
    if(last)session<<",\"map\":"<<json(last->map)<<",\"output_resolution\":["<<last->outputWidth<<','<<last->outputHeight
                   <<"],\"render_resolution\":["<<last->width<<','<<last->height<<']';
    if(!r.revisions.empty())cfg<<r.revisions.back().settings;
    const Revision *previous=nullptr;
    size_t eventIndex=0;
    auto writeEvent=[&](const Event &e){
        events<<"{\"event\":"<<json(e.name)<<",\"time_ns\":"<<e.time
              <<",\"previous\":"<<e.before<<",\"value\":"<<e.after<<"}\n";
    };
    for(const auto &rev:r.revisions) {
        while(eventIndex<r.events.count&&r.events.at(eventIndex).time<=rev.time)writeEvent(r.events.at(eventIndex++));
        const bool known=previous&&previous->id==rev.previous&&!previous->truncated&&!rev.truncated;
        events<<"{\"event\":\"settings_revision\",\"time_ns\":"<<rev.time<<",\"previous\":"<<rev.previous
              <<",\"revision\":"<<rev.id<<",\"truncated\":"<<(rev.truncated?"true":"false")
              <<",\"changes\":"<<(known?changes(previous->settings,rev.settings):"null")
              <<",\"settings\":"<<json(rev.settings)<<"}\n";
        previous=&rev;
    }
    while(eventIndex<r.events.count)writeEvent(r.events.at(eventIndex++));
    lights<<"frame_serial,time_ns,index,category,x,y,z,radius,strength,baked_fraction,blockers,sliced_blocker_estimate,actor_type,"
             "incident_id,snapshot_reason,material,origin_sector,origin_line,origin_side,contributors,provenance_status,assigned_triangles,mask_scope\n";
    auto material=[&](const MaterialBinding &binding) {
        attribution<<",\"material\":"<<(binding.material[0]?json(binding.material):"null")
                   <<",\"next_material\":"<<(binding.next[0]?json(binding.next):"null")
                   <<",\"animation_blend\":";
        if(binding.blend>=0)attribution<<binding.blend;
        else attribution<<"null";
        if(binding.loaded)attribution<<",\"emission_enabled\":"<<(binding.emissive?"true":"false")
            <<",\"relief_bound\":"<<(binding.relief?"true":"false")<<",\"liquid_emitter\":"<<(binding.liquid?"true":"false")
            <<",\"living_profile\":"<<binding.livingProfile;
    };
    constexpr const char *reasons[]={"pre_incident","incident_start","incident_peak","recovery","sampled_attribution"};
    for(const auto &i:r.incidents)for(unsigned k=0;k<SlotCount;++k) {
        const auto &shot=i.evidence[k];
        attribution<<"{\"incident_id\":"<<i.id<<",\"reason\":"<<json(reasons[k])<<",\"snapshot_status\":"
                   <<json(shot.serial?"retained":i.detailed?"unavailable":"disabled");
        if(shot.serial) {
            attribution<<",\"frame_serial\":"<<shot.serial;
            const auto &focus=shot.evidence.focus;
            attribution<<",\"focus_method\":"<<json(focus.kind?"center_geometry_ray":"unavailable");
            if(focus.kind)material(focus.binding);
            if(focus.kind)attribution<<",\"source\":"<<json(focus.source)
                <<",\"kind\":"<<focus.kind<<",\"distance\":"<<focus.distance<<",\"sector\":"<<focus.sector
                <<",\"subsector\":"<<focus.subsector<<",\"line\":"<<focus.line<<",\"side\":"<<focus.side
                <<",\"position\":["<<focus.position[0]<<','<<focus.position[1]<<','<<focus.position[2]<<"]"
                <<",\"normal\":["<<focus.normal[0]<<','<<focus.normal[1]<<','<<focus.normal[2]<<"]";
            attribution<<",\"main_material_workload\":[";
            for(unsigned j=0;j<shot.evidence.materialCount;++j) {
                const auto &m=shot.evidence.materials[j];
                if(j)attribution<<',';
                attribution<<"{\"flat\":"<<(m.flat?"true":"false");
                material(m.binding);
                attribution
                           <<",\"mask_processed_triangles\":"<<m.masks.processed<<",\"lit_triangles\":"<<m.masks.lit
                           <<",\"light_triangle_pairs\":"<<m.masks.pairs<<'}';
            }
            attribution<<"],\"main_mask_histogram\":";
            if(!shot.evidence.maskSampleAvailable)attribution<<"null";
            else {
                attribution<<'[';
                for(size_t j=0;j<shot.evidence.maskHistogram.size();++j) {
                    if(j)attribution<<',';
                    attribution<<shot.evidence.maskHistogram[j];
                }
                attribution<<']';
            }
            for(unsigned j=0;j<shot.evidence.count;++j) {
                const auto &l=shot.evidence.lights[j];
                lights<<shot.serial<<','<<shot.time<<','<<j<<','<<l.category<<','<<l.x<<','<<l.y<<','<<l.z
                      <<','<<l.radius<<','<<l.strength<<','<<l.baked<<','<<l.blockers<<','<<l.slicedBlockers<<',';
                if(l.actorType>=0)lights<<l.actorType;
                lights<<','<<i.id<<','<<reasons[k]<<','<<csv(l.material)<<',';
                if(l.originSector>=0)lights<<l.originSector;
                lights<<',';if(l.originLine>=0)lights<<l.originLine;
                lights<<',';if(l.originSide>=0)lights<<l.originSide;
                lights<<',';if(l.contributors)lights<<l.contributors;
                lights<<','<<(l.contributors>1?(l.material[0]||l.originSector>=0?"merged_partial":"merged_origin_unavailable"):l.material[0]?"exact":"unavailable")<<',';
                if(l.assignedTriangles>=0)lights<<l.assignedTriangles<<",main";
                else lights<<',';
                lights<<'\n';
            }
        }
        attribution<<"}\n";
    }
    frames<<"incident,phase,serial,time_ns,level_serial,settings_revision,segment,map,game_tic,level_tic,"
        "frame_ms,outside_renderer_ms,load_ms,reallocate_ms,acquire_wait_ms,prepare_ms,upload_cpu_ms,encode_cpu_ms,submit_cpu_ms,"
        "relight_ms,camera_ms,geometry_ms,effects_ms,masks_ms,packing_ms,gpu_execution_ms,"
        "player_x,player_y,player_z,eye_x,eye_y,eye_z,yaw,pitch,fov_x,fov_y,player_sector,camera_sector,subsector,"
        "floor,ceiling,floor_z,ceiling_z,render_width,render_height,output_width,output_height,msaa,shader_flags,pass_flags,state_flags,probe_captures,"
        "flashlights,weapon_lights,barrel_lights,decoration_lights,projectile_lights,door_lights,surface_lights,fog_only_lights,"
        "blockers,mean_sliced_blockers,mist_triangles,mist_vertices,fog_lights,particle_vertices,world_triangles,visible_subsectors,rejected_subsectors,visible_lines,rejected_lines,"
        "hotspot_segment_id,frame_validity,gpu_timing_status,world_culling_requested,world_culling_enabled,world_culling_disable_reasons,"
        "world_camera_valid,world_frustum_xy_plane_count,world_frustum_xy_effective,world_frustum_3d_supported,world_frustum_3d_plane_count,"
        "world_geometry_shared,world_subsector_candidates,world_subsectors_accepted,world_line_candidates,world_lines_accepted,"
        "world_floor_triangles,world_ceiling_triangles,world_wall_triangles,world_sky_triangles,"
        "world_3d_rejected_floors,world_3d_rejected_ceilings,world_3d_rejected_wall_spans,world_geometry_path,"
        "mask_processed_triangles,mask_lit_triangles,mask_light_triangle_pairs,mask_surface_light_triangle_pairs,"
        "mask_decoration_light_triangle_pairs,mask_max_lights_per_triangle,mask_mean_lights_per_triangle,mask_mean_lights_per_lit_triangle,"
        "mirror_mask_processed_triangles,mirror_mask_lit_triangles,mirror_mask_light_triangle_pairs,mirror_mask_surface_light_triangle_pairs,"
        "mirror_mask_decoration_light_triangle_pairs,mirror_mask_max_lights_per_triangle,mirror_mask_mean_lights_per_triangle,mirror_mask_mean_lights_per_lit_triangle,trace_collection_cpu_ms,trace_snapshot_cpu_ms\n";
    for(const Frame &f:r.recent)writeFrame(frames,f,0,"history");
    summary<<"Hellforge performance trace, schema 2\nIncidents: "<<r.incidents.size()
           <<"\nGPU execution time: unavailable. CPU encoding is not GPU execution.\n"
             "Outside renderer can include simulation, audio, event processing and waits.\n"
             "Light categories 0-7: flashlight, weapon, barrel, decoration, projectile, door, surface, fog.\n"
             "Pass bits 1/2/4/8/16/32/64: liquid mirror, glass mirror, probes, mist depth, main, bloom, mist.\n"
             "State bits 1/2/4/8/16/32/64/128/256: panel, menu, pause, unfocused, no world, loading, targets, screenshot, no swapchain.\n"
             "GPU pressure and presentation waits cannot be separated by these CPU samples.\n";
    if(!r.incidents.empty()) {
        auto worst=std::max_element(r.incidents.begin(),r.incidents.end(),
            [](const Incident &a,const Incident &b){return a.worst.interval<b.worst.interval;});
        summary<<"Worst incident: "<<worst->id<<" ("<<classification(worst->worst)<<")\n";
    }
    double total=0;
    std::vector<Key> groups;
    unsigned retainedPeaks=0,missingPeaks=0;
    incidents<<"[";
    bool firstIncident=true;
    for(const auto &i:r.incidents) {
        std::vector<float> distribution;
        bool keptWorst=false,keptTrigger=false;
        unsigned preRetained=0,postRetained=0;
        const auto finish=i.recoveryStart?i.recoveryStart-1:i.end;
        auto write=[&](const Frame &f) {
            const bool active=f.time>=i.start&&f.time<=finish;
            if(active&&f.interval>=0)distribution.push_back(f.interval);
            keptWorst|=f.serial==i.worst.serial;
            keptTrigger|=f.serial==i.trigger.serial;
            preRetained+=f.time<i.start;
            postRetained+=i.recovery&&f.time>i.recovery;
            unsigned hotspot=0;
            if(active)for(size_t k=0;k<i.hotspotCount;++k)
                if(f.serial>=i.hotspots[k].first&&f.serial<=i.hotspots[k].last){hotspot=unsigned(k+1);break;}
            writeFrame(frames,f,i.id,active?"incident":f.time<i.start?"pre":i.recovery&&f.time>i.recovery?"post":"recovery",hotspot);
        };
        for(size_t n=0;n<i.firstCount;++n)write(i.first[n]);
        for(size_t n=0;n<i.tail.count;++n)write(i.tail.at(n));
        if(!keptWorst&&i.worst.serial)write(i.worst);
        if(!keptTrigger&&i.trigger.serial)write(i.trigger);
        std::sort(distribution.begin(),distribution.end());
        auto p=[&](double q){return distribution.empty()?0.f:distribution[size_t(std::ceil(distribution.size()*q))-1];};
        const Frame &f=i.worst;
        const double duration=finish>=i.start?double(finish-i.start)/ms:0;
        total+=i.degraded.elapsed;
        retainedPeaks+=i.evidence[Peak].serial!=0;
        missingPeaks+=i.detailed&&!i.evidence[Peak].serial;
        const uint64_t retained=distribution.size();
        const auto pjson=[&](double q){return distribution.empty()?std::string("null"):std::to_string(p(q));};
        if(!firstIncident)incidents<<',';
        firstIncident=false;
        incidents<<"\n{\"incident_id\":"<<i.id<<",\"termination\":"<<json(i.termination)
                 <<",\"flags\":"<<i.reason<<",\"target_fps\":"<<1000/i.budget<<",\"first_candidate_serial\":"<<i.firstSerial
                 <<",\"trigger_serial\":"<<i.trigger.serial<<",\"last_degraded_serial\":"<<i.lastDegradedSerial
                 <<",\"end_serial\":"<<i.endSerial<<",\"peak_serial\":"<<f.serial<<",\"start_ns\":"<<i.start
                 <<",\"trigger_ns\":"<<i.trigger.time<<",\"end_ns\":"<<i.end
                 <<",\"last_degraded_ns\":"<<i.lastDegraded<<",\"recovery_start_ns\":"
                 <<(i.recoveryStart?std::to_string(i.recoveryStart):"null")
                 <<",\"recovery_confirmed_ns\":"<<(i.recovery?std::to_string(i.recovery):"null")<<",\"active_ms\":"<<i.active.elapsed
                 <<",\"degraded_ms\":"<<i.degraded.elapsed<<",\"recovery_ms\":"<<i.contextRecovery.elapsed
                 <<",\"post_ms\":"<<i.post.elapsed<<",\"active_fps\":"<<i.active.fps()<<",\"degraded_fps\":"<<i.degraded.fps()
                 <<",\"capture_fps\":"<<i.captured.fps()<<",\"active_frames_observed\":"<<i.active.count
                 <<",\"active_frames_retained\":"<<retained<<",\"active_frames_downsampled\":"<<(i.active.count>retained?i.active.count-retained:0)
                 <<",\"incident_frames_lost\":0,\"active_max_ms\":"<<i.active.maximum
                 <<",\"retained_sample_median_ms\":"<<pjson(.5)<<",\"retained_sample_p95_ms\":"<<pjson(.95)
                 <<",\"retained_sample_p99_ms\":"<<pjson(.99)<<",\"requested_prehistory_ns\":"<<5000*ms
                 <<",\"prehistory_frames_retained\":"<<preRetained<<",\"posthistory_frames_retained\":"<<postRetained
                 <<",\"available_prehistory_ns\":"<<i.preNs<<",\"requested_posthistory_ns\":"<<3000*ms
                 <<",\"available_posthistory_ns\":"<<(i.recovery?i.end-i.recovery:0)
                 <<",\"missing_posthistory_ns\":"<<(3000*ms-std::min(3000*ms,i.recovery?i.end-i.recovery:0))
                 <<",\"truncated\":"<<(std::strcmp(i.termination,"recovered")?"true":"false")
                 <<",\"segment_limit_omissions\":"<<i.segmentOmitted<<",\"segments\":[";
        for(size_t k=0;k<i.hotspotCount;++k) {
            const auto &spot=i.hotspots[k];const auto &b=spot.key.bins;
            if(k)incidents<<',';
            incidents<<"{\"segment_id\":"<<k+1<<",\"parent_incident_id\":"<<i.id<<",\"sector\":"<<b[0]
                     <<",\"subsector\":"<<b[6]<<",\"position_bin\":["<<b[1]<<','<<b[2]<<','<<b[3]<<']'
                     <<",\"yaw_bin\":"<<b[4]<<",\"pitch_bin\":"<<b[5]<<",\"first_serial\":"<<spot.first
                     <<",\"last_serial\":"<<spot.last<<",\"start_ns\":"<<spot.begin<<",\"end_ns\":"<<spot.end
                     <<",\"observed_frames\":"<<spot.stats.count<<",\"duration_ms\":"<<spot.stats.elapsed
                     <<",\"peak_serial\":"<<spot.peak<<",\"peak_ms\":"<<spot.stats.maximum<<'}';
            if(std::find(groups.begin(),groups.end(),spot.key)==groups.end())groups.push_back(spot.key);
        }
        incidents<<"]}";

        summary<<"\nIncident "<<i.id<<" ("<<classification(f)<<") on "<<f.map
               <<", flags "<<i.reason<<" (slow=1, spike=2, severe=4), level "<<f.level<<", settings "<<f.revision
               <<"\n  Peak eye "<<f.eye[0]<<", "<<f.eye[1]<<", "<<f.eye[2]<<" yaw "<<f.yaw<<" pitch "<<f.pitch
               <<", camera sector "<<f.cameraSector<<", floor "<<f.floor
               <<"\n  Span "<<duration<<" ms, average "<<i.active.fps()
               <<" FPS, median/P95/P99/max "<<p(.5)<<'/'<<p(.95)<<'/'<<p(.99)<<'/'<<f.interval
               <<" ms from "<<distribution.size()<<" retained incident samples of "<<i.active.count
               <<"\n  Downsampled pre/active "<<i.omittedPre<<'/'<<i.omittedActive<<", overwritten "<<i.tail.dropped
               <<"\n  Peak CPU outside/acquire/prepare/upload/encode/submit "<<f.outside<<'/'<<f.acquire<<'/'<<f.prepare<<'/'
               <<f.upload<<'/'<<f.encode<<'/'<<f.submit<<" ms\n  Surface lights "<<f.lights[6]<<", blockers "<<f.blockers
               <<", world triangles "<<f.triangles<<", passes "<<f.passes<<", feature flags "<<f.featureFlags<<"\n";
        const auto &vis=f.visibility;
        if(vis.xy>=0) {
            summary<<"  World cull gate "<<vis.enabled<<", XY/3D planes "<<vis.xy<<'/'<<vis.spatial
                   <<", bypass bits "<<vis.reasons<<'\n';
            if(!vis.enabled)summary<<"  WORLD_CULLING_BYPASSED\n";
            else if(vis.valid&&!vis.xy)summary<<"  WORLD_XY_FRUSTUM_EMPTY\n";
            if(i.comparisonTriangles>0&&f.triangles>=2*i.comparisonTriangles)
                summary<<"  WORLD_GEOMETRY_WORKLOAD_INCREASE\n";
        }
        if(f.maskAvailable)summary<<"  Main mask processed/lit/pairs/surface pairs "<<f.masks[0].processed<<'/'
            <<f.masks[0].lit<<'/'<<f.masks[0].pairs<<'/'<<f.masks[0].surface<<'\n';
        const auto &peak=i.evidence[Peak].evidence;
        if(peak.focus.kind)summary<<"  Focused material "<<peak.focus.binding.material<<" (center geometry ray)\n";
        if(i.evidence[Peak].serial) {
            unsigned baked=0,blockers=0;
            for(unsigned l=0;l<peak.count;++l)if(peak.lights[l].category==6) {
                baked+=peak.lights[l].baked==1;blockers+=peak.lights[l].blockers;
            }
            summary<<"  Fully baked surface lights "<<baked<<", surface-light blockers "<<blockers<<'\n';
        }
        summary<<"  Termination "<<i.termination<<", spatial segments "<<i.hotspotCount
               <<"\n  Active/degraded/recovery/post ms "<<i.active.elapsed<<'/'<<i.degraded.elapsed<<'/'
               <<i.contextRecovery.elapsed<<'/'<<i.post.elapsed<<", degraded/capture FPS "<<i.degraded.fps()<<'/'<<i.captured.fps()
               <<"\n  Peak light evidence "<<(i.evidence[Peak].serial?"retained":i.detailed?"unavailable":"disabled")<<'\n';
    }
    incidents<<"\n]\n";
    hotspots<<"[\n";
    bool firstGroup=true;
    for(const Key &key:groups) {
        if(!firstGroup)hotspots<<",\n";
        firstGroup=false;
        const auto &b=key.bins;
        hotspots<<"{\"map\":"<<json(key.map)<<",\"sector\":"<<b[0]<<",\"position_bin\":["<<b[1]<<','<<b[2]<<','<<b[3]
                <<"],\"yaw_bin\":"<<b[4]<<",\"pitch_bin\":"<<b[5]<<",\"map_instance\":"<<key.level<<",\"subsector\":"<<b[6]<<",\"incidents\":[";
        bool first=true;
        uint64_t visits=0,frames=0;
        double elapsed=0;
        float peak=0;
        for(const auto &i:r.incidents) {
            bool matched=false;
            for(size_t k=0;k<i.hotspotCount;++k) {
                const auto &spot=i.hotspots[k];
                if(!(spot.key==key))continue;
                matched=true;++visits;
                frames+=spot.stats.count;elapsed+=spot.stats.elapsed;peak=std::max(peak,spot.stats.maximum);
            }
            if(matched){if(!first)hotspots<<',';first=false;hotspots<<i.id;}
        }
        hotspots<<"],\"visits\":"<<visits<<",\"observed_frames\":"<<frames
                <<",\"duration_ms\":"<<elapsed<<",\"peak_ms\":"<<peak<<"}";
    }
    hotspots<<"\n]\n";
    summary<<"\nNormal history wrap evictions "<<r.overwrittenHistory<<", exact peaks retained/missing "
           <<retainedPeaks<<'/'<<missingPeaks<<'\n';
    summary<<"\nTotal time in slow frames during incidents: "<<total<<" ms\nSettings revisions: "<<r.revisions.size()
           <<" (see events.jsonl for changed keys)\nDropped incidents/export requests: "<<r.droppedIncidents<<'/'<<r.droppedExports<<'\n';
    for(size_t n=1;n<r.revisions.size();++n) {
        const auto &from=r.revisions[n-1],&to=r.revisions[n];
        summary<<"  Revision "<<to.previous<<" -> "<<to.id<<' '
               <<(from.id==to.previous&&!from.truncated&&!to.truncated?changes(from.settings,to.settings):"previous settings unavailable")<<'\n';
    }
    session<<",\"peak_snapshots_retained\":"<<retainedPeaks<<",\"peak_snapshots_missing\":"<<missingPeaks
           <<",\"periodic_snapshots_evicted\":0,\"optional_snapshots_dropped\":0"
             ",\"snapshot_staging_overwrites\":0,\"snapshot_budget_exhaustions\":0}\n";
    for(auto *out:{&incidents,&attribution,&session,&summary,&cfg,&frames,&events,&hotspots,&lights})out->close();
}

class Worker {
    std::mutex mutex;
    std::condition_variable ready;
    std::thread thread;
    std::unique_ptr<Report> pending;
    bool busy=false,stop=false;
    void run() {
        std::unique_lock<std::mutex> lock(mutex);
        for(;;) {
            ready.wait(lock,[&]{return stop||pending;});
            if(!pending)return;
            auto report=std::move(pending);
            lock.unlock();
            try {
                const auto folder=root()/(std::to_string(report->startUtc)+"-"+std::to_string(report->number));
                writeReport(*report,folder);
                fprintf(stderr,"Performance report exported: %s\n",folder.u8string().c_str());
            } catch(const std::exception &e){fprintf(stderr,"Performance report export failed: %s\n",e.what());}
            report.reset();
            lock.lock();
            busy=false;
        }
    }
public:
    void start() {
        if(!thread.joinable()) {
            stop=false;
            thread=std::thread([this]{run();});
        }
    }
    template<class Copy> bool queue(Copy copy) {
        std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);
        if(!lock||busy||!thread.joinable())return false;
        pending=copy();
        busy=true;
        ready.notify_one();
        return true;
    }
    void finish() {
        if(!thread.joinable())return;
        {std::lock_guard<std::mutex> lock(mutex);stop=true;}
        ready.notify_one();
        thread.join();
    }
    ~Worker(){finish();}
};
enum class Detector { Idle, Candidate, Active, Recovering, PostCapture };
struct Session {
    Config config;
    bool enabled=false,seen=false;
    uint64_t serial=0,previous=0,slowSince=0,slowSerial=0;
    unsigned slowFrames=0;
    Detector detector=Detector::Idle;
    Snapshot latest{},candidateStart{},candidatePeak{},comparison{};
    float candidateMaximum=0, candidateFirstInterval=0;
    uint64_t lastAttribution=0;
    float filtered=0;
    uint64_t nextIncident=0,exportNumber=0;
    unsigned level=0,segment=0,revision=0,state=0;
    char map[16]{};
    int width=0,height=0,outputWidth=0,outputHeight=0;
    Ring<Frame,historyLimit> history;
    Report report;
    std::deque<std::unique_ptr<Incident>> completed;
    std::unique_ptr<Incident> active;
};
std::unique_ptr<Session> session;
Worker worker;
std::string renderer,gpuName,presentMode;
int vsync=-1;

void event(Session &s,uint64_t time,const char *name,unsigned before=0,unsigned after=0) {
    Event entry{};
    if(s.report.events.count)time=std::max(time,s.report.events.at(s.report.events.count-1).time);
    entry.time=time;entry.before=before;entry.after=after;
    std::snprintf(entry.name,sizeof(entry.name),"%s",name);
    s.report.events.push(entry);
}
bool queue(Session &s,const Incident *only) {
    try {
        if(worker.queue([&]{
            auto out=std::make_unique<Report>();
            out->config=s.config;out->startUtc=s.report.startUtc;
            out->backend=renderer;out->gpu=gpuName;out->presentation=presentMode;out->vsync=vsync;
            out->platform=SDL_GetPlatform();out->iwad=s.report.iwad;out->addons=s.report.addons;
            out->events=s.report.events;out->revisions=s.report.revisions;
            out->number=++s.exportNumber;out->droppedIncidents=s.report.droppedIncidents;
            out->droppedExports=s.report.droppedExports;out->droppedRevisions=s.report.droppedRevisions;
            out->overwrittenHistory=s.history.dropped;
            out->observed=s.serial;out->eligible=s.report.eligible;out->historyRetained=s.history.count;
            if(s.history.count){out->firstSerial=s.history.at(0).serial;out->lastSerial=s.history.at(s.history.count-1).serial;}
            if(only)out->incidents.push_back(*only);
            else {
                out->incidents.reserve(incidentLimit+1);
                for(const auto &incident:s.completed)out->incidents.push_back(*incident);
                if(s.active)out->incidents.push_back(*s.active);
                out->recent.reserve(s.history.count);
                for(size_t i=0;i<s.history.count;++i)out->recent.push_back(s.history.at(i));
            }
            return out;
        }))return true;
    } catch(const std::exception &e){fprintf(stderr,"Performance report queue failed: %s\n",e.what());}
    ++s.report.droppedExports;
    return false;
}
void finishIncident(Session &s,const char *reason) {
    s.detector=Detector::Idle;
    if(!s.active)return;
    s.active->termination=reason;
    if(s.completed.size()==incidentLimit) {
        s.completed.pop_front();
        ++s.report.droppedIncidents;
    }
    s.completed.push_back(std::move(s.active));
    if(s.config.autoExport)queue(s,s.completed.back().get());
}
void breakSegment(Session &s,const char *reason) {
    finishIncident(s,reason);
    ++s.segment;
    s.previous=s.slowSince=s.slowSerial=0;
    s.slowFrames=0;
    s.latest={};s.lastAttribution=0;s.filtered=0;
}
void trigger(Session &s,const Frame &f,unsigned reason) {
    s.active=std::make_unique<Incident>();
    auto &i=*s.active;
    i.id=++s.nextIncident;i.start=s.slowSince;i.firstSerial=s.slowSerial;
    i.trigger=f;i.reason=reason;i.budget=1000.0/s.config.target;i.detailed=s.config.detailed;
    i.evidence[Comparison]=s.comparison;i.evidence[Start]=s.candidateStart;i.evidence[Peak]=s.candidatePeak;
    size_t first=s.history.count;
    const uint64_t since=i.start>5000*ms?i.start-5000*ms:0;
    while(first>0 && s.history.at(first-1).segment==f.segment && s.history.at(first-1).time>=since)--first;
    if(first<s.history.count&&s.history.at(first).time<i.start)i.preNs=i.start-s.history.at(first).time;
    for(size_t j=s.history.count;j>first;--j)
        if(s.history.at(j-1).serial<i.firstSerial){i.comparisonTriangles=s.history.at(j-1).triangles;break;}
    const size_t count=s.history.count-first;
    const size_t stride=std::max<size_t>(1,(count+i.first.size()-1)/i.first.size());
    for(size_t j=first;j<s.history.count;++j) {
        const Frame &previous=s.history.at(j);
        if(previous.interval>0)i.captured.add(previous.interval);
        if(previous.serial>=i.firstSerial)i.measure(previous);
        if((j-first)%stride==0)i.keep(previous);
        else if(previous.serial<i.firstSerial)++i.omittedPre;
        else ++i.omittedActive;
    }
    s.detector=Detector::Active;
}
void detect(Session &s,const Frame &f,const Snapshot &shot) {
    const double budget=1000.0/s.config.target;
    const float observation=std::min(f.interval,float(2*budget));
    const float weight=1.f-std::exp(-std::min(f.interval,100.f)/150.f);
    s.filtered=s.filtered?s.filtered+weight*(observation-s.filtered):float(budget)+weight*(observation-float(budget));
    const bool slow=f.interval>budget/.90;
    const bool recovering=s.filtered<=budget/.95&&!slow;
    const bool candidateContinues=slow||s.filtered>budget/.90;
    const unsigned spike=s.config.mode!=0&&f.interval>=2*budget?(f.interval>=3*budget?6u:2u):0;
    if(s.detector==Detector::PostCapture) {
        if(slow||spike) { finishIncident(s,"recovered");s.slowFrames=0; }
        else {
            auto &i=*s.active;
            i.post.add(f.interval);i.captured.add(f.interval);i.keep(f);i.end=f.time;i.endSerial=f.serial;
            if(f.time-i.recovery>=3000*ms)finishIncident(s,"recovered");
            return;
        }
    }
    if(!s.active) {
        if(!candidateContinues&&!spike) {s.detector=Detector::Idle;return;}
        if(s.detector!=Detector::Candidate) {
            s.detector=Detector::Candidate;
            s.slowSince=f.time;s.slowSerial=f.serial;s.slowFrames=0;
            s.candidateStart=s.candidatePeak=shot;s.comparison=s.latest;s.candidateMaximum=f.interval;s.candidateFirstInterval=f.interval;
        }
        ++s.slowFrames;
        if(f.interval>s.candidateMaximum){s.candidateMaximum=f.interval;s.candidatePeak=shot;}
        // Include the candidate's first elapsed interval. Multiple samples are
        // required so a single stalled frame cannot establish sustained load.
        const bool sustained=s.config.mode!=1&&slow&&s.slowFrames>1&&
            f.time-s.slowSince+uint64_t(s.candidateFirstInterval*ms)>=uint64_t(s.config.duration)*ms;
        if(!spike&&!sustained)return;
        trigger(s,f,spike|(sustained?1u:0u));
    }
    auto &i=*s.active;
    i.reason|=spike;
    if(!(i.reason&1)&&s.config.mode!=1) {
        if(slow) {
            if(!s.slowFrames)s.slowSince=f.time;
            if(s.slowSerial!=f.serial)++s.slowFrames;
            if(s.slowFrames>1&&f.time-s.slowSince>=uint64_t(s.config.duration)*ms)i.reason|=1;
        } else s.slowFrames=0;
    }
    i.end=f.time;i.endSerial=f.serial;i.captured.add(f.interval);i.keep(f);
    if(shot.evidence.attributionAttempted&&(shot.evidence.focus.kind||!i.evidence[Attribution].serial))
        i.evidence[Attribution]=shot;
    if(recovering) {
        if(s.detector!=Detector::Recovering) {
            i.recoveryStart=f.time;i.contextRecovery={};
            s.detector=Detector::Recovering;
        }
        i.contextRecovery.add(f.interval);
        if(f.time-i.recoveryStart>=500*ms) {
            i.recovery=f.time;i.evidence[Recovery]=shot;s.detector=Detector::PostCapture;
        }
        return;
    }
    if(s.detector==Detector::Recovering) {
        // An interrupted recovery belongs to the active window and its
        // child segments. Fold in any samples that exceed history coverage.
        size_t first=s.history.count;
        while(first&&s.history.at(first-1).time>=i.recoveryStart)--first;
        Stats replayed;
        for(size_t n=first;n<s.history.count;++n) {
            const auto &frame=s.history.at(n);
            if(frame.segment!=f.segment||frame.state||!frame.hasView||!std::isfinite(frame.interval)||frame.interval<=0)continue;
            i.measure(frame);replayed.add(frame.interval);
        }
        i.active.count+=i.contextRecovery.count-replayed.count;
        i.active.elapsed+=i.contextRecovery.elapsed-replayed.elapsed;
        i.contextRecovery={};i.recoveryStart=0;
    }
    s.detector=Detector::Active;
    if(!i.worst.serial||f.interval>i.worst.interval)i.evidence[Peak]=shot;
    i.measure(f);
}
} // namespace

void configure(bool enabled,Config config,unsigned revision,std::string settings) {
    if(!session&&!enabled)return;
    uint64_t now=SDL_GetTicksNS();
    if(!session) {
        session=std::make_unique<Session>();
        SDL_Time utc=0;
        SDL_GetCurrentTime(&utc);
        session->report.startUtc=utc;
        session->report.revisions.reserve(48);
        try {worker.start();}
        catch(const std::exception &e){fprintf(stderr,"Performance export worker unavailable: %s\n",e.what());}
    }
    auto &s=*session;
    if(s.history.count)now=std::max(now,s.history.at(s.history.count-1).time);
    if(s.enabled!=enabled||s.revision!=revision||s.config.target!=config.target||
       s.config.mode!=config.mode||s.config.duration!=config.duration)
        breakSegment(s,!enabled?"tracer_disabled":"settings_changed");
    if(s.enabled!=enabled)event(s,now,enabled?"tracer_enabled":"tracer_disabled");
    const bool truncated=settings.size()>settingsLimit;
    if(truncated)settings.resize(settingsLimit);
    if(s.report.revisions.size()==48) {
        s.report.revisions.erase(s.report.revisions.begin()+1);
        ++s.report.droppedRevisions;
    }
    s.report.revisions.push_back({now,revision,s.revision,std::move(settings),truncated});
    s.enabled=enabled;
    s.config=config;
    s.revision=revision;
}
void identify(const char *backend,const char *gpu,const char *presentation,int sync) {
    if(session&&session->enabled&&(renderer!=(backend?backend:"")||
       presentMode!=(presentation?presentation:"")||vsync!=sync))
        boundary(SDL_GetTicksNS(),"presentation_changed");
    renderer=backend?backend:"";gpuName=gpu?gpu:"";presentMode=presentation?presentation:"";vsync=sync;
}
void files(const char *iwad,std::string addons) {
    if(!session)return;
    session->report.iwad=iwad?iwad:"";
    std::istringstream stream(std::move(addons));
    std::string name,encoded;
    while(std::getline(stream,name)){if(!encoded.empty())encoded+=',';encoded+=json(name);}
    session->report.addons=std::move(encoded);
}
void boundary(uint64_t timestamp,const char *reason) {
    if(session&&session->enabled&&session->previous) {
        event(*session,timestamp,reason);
        breakSegment(*session,reason);
    }
}
void submit(Frame &f,bool fromSave,const Evidence *evidence) {
    if(!session||!session->enabled)return;
    auto &s=*session;
    f.serial=++s.serial;
    f.revision=s.revision;
    if(f.hasView&&(!s.seen||f.level!=s.level)) {
        const char *name=fromSave?"savegame_load":s.seen&&!std::strcmp(s.map,f.map)?"map_restart":"map_load";
        event(s,f.time,name,s.level,f.level);
        breakSegment(s,name);
        s.level=f.level;
        std::memcpy(s.map,f.map,sizeof(s.map));
        s.seen=true;
    }
    if(f.width!=s.width||f.height!=s.height||f.outputWidth!=s.outputWidth||f.outputHeight!=s.outputHeight) {
        event(s,f.time,"resolution_change",unsigned(s.width),unsigned(f.width));
        breakSegment(s,"resolution_change");
        s.width=f.width;s.height=f.height;s.outputWidth=f.outputWidth;s.outputHeight=f.outputHeight;
    }
    constexpr unsigned barriers=Panel|Menu|Paused|Unfocused|NoWorld|NoSwapchain;
    for(const auto &flag:{std::pair<unsigned,const char*>{Panel,"settings_panel"},{Menu,"game_menu"},
        {Paused,"pause"},{Unfocused,"focus_lost"},{NoWorld,"no_world"},{NoSwapchain,"swapchain_unavailable"}})
        if((s.state^f.state)&flag.first) {
            event(s,f.time,flag.second,(s.state&flag.first)!=0,(f.state&flag.first)!=0);
            if(flag.first&barriers)breakSegment(s,flag.second);
        }
    if(f.state&(Loading|Targets|Screenshot)) {
        if(f.state&Loading)event(s,f.time,"renderer_rebuild",0,unsigned(std::max(0.f,f.loading)));
        if(f.state&Targets)event(s,f.time,"target_reallocation",0,unsigned(std::max(0.f,f.targets)));
        if(f.state&Screenshot)event(s,f.time,"screenshot");
        breakSegment(s,f.state&Loading?"renderer_rebuild":f.state&Targets?"target_reallocation":"screenshot");
    }
    s.state=f.state;
    f.segment=s.segment;
    if(!s.previous)f.interval=f.outside=-1;
    s.previous=f.time;
    if(f.state==0&&f.hasView&&std::isfinite(f.interval)&&f.interval>0) {
        ++s.report.eligible;
        Snapshot shot{};
        if(evidence&&s.config.detailed) {shot.serial=f.serial;shot.time=f.time;shot.evidence=*evidence;}
        detect(s,f,shot);
        s.latest=shot;
    }
    s.history.push(f);
}
bool detailed(){return session&&session->enabled&&session->config.detailed;}
bool wantsAttribution(const Frame &f) {
    if(!detailed()||f.state||!f.hasView||f.interval<=0)return false;
    auto &s=*session;
    if(s.lastAttribution&&f.time-s.lastAttribution<100*ms)return false;
    const double budget=1000.0/s.config.target;
    const bool interest=!s.lastAttribution||f.time-s.lastAttribution>=500*ms||
        (s.active&&f.interval>s.active->worst.interval)||
        (f.interval>budget/.9&&s.detector==Detector::Idle)||
        (s.detector==Detector::Recovering&&f.time-s.active->recoveryStart>=500*ms);
    if(interest)s.lastAttribution=f.time;
    return interest;
}
bool exportReport(){return session&&queue(*session,nullptr);}
bool openFolder() {
    try {
        const auto folder=root();
        fs::create_directories(folder);
        const std::string raw=folder.generic_u8string();
        std::string url=raw.front()=='/'?"file://":"file:///";
        for(unsigned char c:raw) {
            if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='/'||c==':'||c=='_'||c=='-'||c=='.')url+=char(c);
            else {const char *hex="0123456789ABCDEF";url+='%';url+=hex[c>>4];url+=hex[c&15];}
        }
        return SDL_OpenURL(url.c_str());
    } catch(const std::exception &e){fprintf(stderr,"Cannot open performance diagnostics: %s\n",e.what());return false;}
}
void shutdown() {
    if(session) {
        event(*session,SDL_GetTicksNS(),"tracer_shutdown");
        finishIncident(*session,"tracer_shutdown");
        worker.finish();
        session.reset();
    }
}
} // namespace doom3d::performance

namespace doom3d {
void exportPerformanceReport() {
    if(!performance::exportReport())
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,"Performance trace",
            "No trace session is available, or an export is already running. Apply Enable Performance Tracer and try again.",nullptr);
}
void openPerformanceFolder() {
    if(!performance::openFolder())
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING,"Performance trace","Could not open the diagnostics folder.",nullptr);
}
}
