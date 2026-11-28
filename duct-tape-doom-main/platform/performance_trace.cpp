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
    std::array<int,6> bins;
    char map[16];
    bool operator==(const Key &b) const {return bins==b.bins&&!std::strcmp(map,b.map);}
};
Key keyFor(const Frame &f) {
    Key key{};
    std::memcpy(key.map,f.map,sizeof(key.map));
    key.bins={f.cameraSector,int(std::floor(f.eye[0]/64)),int(std::floor(f.eye[1]/64)),
        int(std::floor(f.eye[2]/64)),int(std::floor((f.yaw+7.5f)/15))%24,int(std::floor((f.pitch+7.5f)/15))};
    return key;
}
struct Incident {
    uint64_t id=0,start=0,end=0,recovery=0;
    Key key{};
    Frame trigger{},worst{};
    unsigned reason=0,count=0,omittedPre=0,omittedActive=0;
    double elapsed=0,slowMs=0,budget=0;
    std::array<Frame,224> first{};
    size_t firstCount=0;
    Ring<Frame,416> tail;
    void keep(const Frame &f) {
        if(firstCount<first.size())first[firstCount++]=f;
        else tail.push(f);
    }
    void measure(const Frame &f) {
        end=f.time;
        if(f.interval>worst.interval)worst=f;
        if(f.interval>=0) {
            ++count;elapsed+=f.interval;
            if(f.interval>budget/.9)slowMs+=f.interval;
        }
    }
};
struct Event {uint64_t time=0;char name[32]{};unsigned before=0,after=0;};
struct Revision {uint64_t time;unsigned id,previous;std::string settings;bool truncated;};
struct Snapshot {uint64_t serial=0,time=0;unsigned count=0;std::array<Light,maxTraceLights> values{};};
struct Report {
    Config config;
    int64_t startUtc=0;
    uint64_t number=0,droppedIncidents=0,droppedExports=0,droppedRevisions=0,overwrittenHistory=0;
    std::string backend,gpu,presentation,platform,iwad,addons;
    int vsync=-1;
    Ring<Event,512> events;
    Ring<Snapshot,64> snapshots;
    std::vector<Revision> revisions;
    std::vector<Incident> incidents;
    std::vector<Frame> recent;
};
static_assert(2*((incidentLimit+1)*sizeof(Incident)+sizeof(Report)+
    48*settingsLimit+historyLimit*sizeof(Frame))<16*1024*1024,"Performance tracing memory budget");

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

void writeFrame(std::ostream &out,const Frame &f,uint64_t incident,const char *phase) {
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
    auto session=file("session.json"), summary=file("summary.txt"), cfg=file("settings.cfg"),
         frames=file("frames.csv"), events=file("events.jsonl"), hotspots=file("hotspots.json"),
         lights=file("lights.csv");
    session<<"{\n\"schema_version\":1,\"session_start_unix_ns\":"<<r.startUtc<<",\"export\":"<<r.number
           <<",\"engine\":"<<json(DOOM_ENGINE_VERSION)<<",\"platform\":"<<json(r.platform)
           <<",\"backend\":"<<json(r.backend)<<",\"gpu\":"<<(r.gpu.empty()?"null":json(r.gpu))
           <<",\"presentation\":"<<json(r.presentation)<<",\"vsync\":"<<(r.vsync<0?"null":r.vsync?"true":"false")
           <<",\"iwad\":"<<json(r.iwad)<<",\"addons\":["<<r.addons<<"],\"gpu_execution_ms\":null"
           <<",\"target_fps\":"<<r.config.target<<",\"mode\":"<<r.config.mode<<",\"slowdown_ms\":"<<r.config.duration
           <<",\"detailed\":"<<(r.config.detailed?"true":"false")<<",\"auto_export\":"<<(r.config.autoExport?"true":"false")
           <<",\"dropped_incidents\":"<<r.droppedIncidents<<",\"dropped_exports\":"<<r.droppedExports
           <<",\"dropped_revisions\":"<<r.droppedRevisions<<",\"dropped_events\":"<<r.events.dropped
           <<",\"dropped_light_snapshots\":"<<r.snapshots.dropped<<",\"history_overwritten\":"<<r.overwrittenHistory;
    const Frame *last=r.recent.empty()?(r.incidents.empty()?nullptr:&r.incidents.back().worst):&r.recent.back();
    if(last)session<<",\"map\":"<<json(last->map)<<",\"output_resolution\":["<<last->outputWidth<<','<<last->outputHeight
                   <<"],\"render_resolution\":["<<last->width<<','<<last->height<<']';
    session<<"}\n";
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
    lights<<"frame_serial,time_ns,index,category,x,y,z,radius,strength,baked_fraction,blockers,sliced_blocker_estimate,actor_type\n";
    for(size_t i=0;i<r.snapshots.count;++i) {
        const auto &snapshot=r.snapshots.at(i);
        for(unsigned j=0;j<snapshot.count;++j) {
            const auto &l=snapshot.values[j];
            lights<<snapshot.serial<<','<<snapshot.time<<','<<j<<','<<l.category<<','<<l.x<<','<<l.y<<','<<l.z
                  <<','<<l.radius<<','<<l.strength<<','<<l.baked<<','<<l.blockers<<','<<l.slicedBlockers<<',';
            if(l.actorType>=0)lights<<l.actorType;
            lights<<'\n';
        }
    }
    frames<<"incident,phase,serial,time_ns,level_serial,settings_revision,segment,map,game_tic,level_tic,"
        "frame_ms,outside_renderer_ms,load_ms,reallocate_ms,acquire_wait_ms,prepare_ms,upload_cpu_ms,encode_cpu_ms,submit_cpu_ms,"
        "relight_ms,camera_ms,geometry_ms,effects_ms,masks_ms,packing_ms,gpu_execution_ms,"
        "player_x,player_y,player_z,eye_x,eye_y,eye_z,yaw,pitch,fov_x,fov_y,player_sector,camera_sector,subsector,"
        "floor,ceiling,floor_z,ceiling_z,render_width,render_height,output_width,output_height,msaa,shader_flags,pass_flags,state_flags,probe_captures,"
        "flashlights,weapon_lights,barrel_lights,decoration_lights,projectile_lights,door_lights,surface_lights,fog_only_lights,"
        "blockers,mean_sliced_blockers,mist_triangles,mist_vertices,fog_lights,particle_vertices,world_triangles,visible_subsectors,rejected_subsectors,visible_lines,rejected_lines\n";
    for(const Frame &f:r.recent)writeFrame(frames,f,0,"history");
    summary<<"Hellforge performance trace, schema 1\nIncidents: "<<r.incidents.size()
           <<"\nGPU execution time: unavailable. CPU encoding is not GPU execution.\n"
             "Outside renderer can include simulation, audio, event processing and waits.\n"
             "Gameplay CPU, focused surface, lit triangles, light-triangle pairs and light material provenance are unavailable.\n"
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
    for(const auto &i:r.incidents) {
        std::vector<float> distribution;
        bool keptWorst=false,keptTrigger=false;
        const auto finish=i.recovery?i.recovery:i.end;
        auto write=[&](const Frame &f) {
            const bool active=f.time>=i.start&&f.time<=finish;
            if(active&&f.interval>=0)distribution.push_back(f.interval);
            keptWorst|=f.serial==i.worst.serial;
            keptTrigger|=f.serial==i.trigger.serial;
            writeFrame(frames,f,i.id,active?"incident":f.time<i.start?"pre":"recovery");
        };
        for(size_t n=0;n<i.firstCount;++n)write(i.first[n]);
        for(size_t n=0;n<i.tail.count;++n)write(i.tail.at(n));
        if(!keptWorst&&i.worst.serial)write(i.worst);
        if(!keptTrigger&&i.trigger.serial)write(i.trigger);
        std::sort(distribution.begin(),distribution.end());
        auto p=[&](double q){return distribution.empty()?0.f:distribution[size_t(std::ceil(distribution.size()*q))-1];};
        const Frame &f=i.worst;
        const double duration=finish>=i.start?double(finish-i.start)/ms:0;
        total+=i.slowMs;
        summary<<"\nIncident "<<i.id<<" ("<<classification(f)<<") on "<<f.map
               <<", flags "<<i.reason<<" (slow=1, spike=2, severe=4), level "<<f.level<<", settings "<<f.revision
               <<"\n  Peak eye "<<f.eye[0]<<", "<<f.eye[1]<<", "<<f.eye[2]<<" yaw "<<f.yaw<<" pitch "<<f.pitch
               <<", camera sector "<<f.cameraSector<<", floor "<<f.floor
               <<"\n  Span "<<duration<<" ms, average "<<(i.elapsed>0?1000*i.count/i.elapsed:0)
               <<" FPS, median/P95/P99/max "<<p(.5)<<'/'<<p(.95)<<'/'<<p(.99)<<'/'<<f.interval
               <<" ms from "<<distribution.size()<<" retained incident samples of "<<i.count
               <<"\n  Downsampled pre/active "<<i.omittedPre<<'/'<<i.omittedActive<<", overwritten "<<i.tail.dropped
               <<"\n  Peak CPU outside/acquire/prepare/upload/encode/submit "<<f.outside<<'/'<<f.acquire<<'/'<<f.prepare<<'/'
               <<f.upload<<'/'<<f.encode<<'/'<<f.submit<<" ms\n  Surface lights "<<f.lights[6]<<", blockers "<<f.blockers
               <<", world triangles "<<f.triangles<<", passes "<<f.passes<<", feature flags "<<f.featureFlags<<"\n";
        if(std::find(groups.begin(),groups.end(),i.key)==groups.end())groups.push_back(i.key);
    }
    hotspots<<"[\n";
    bool firstGroup=true;
    for(const Key &key:groups) {
        if(!firstGroup)hotspots<<",\n";
        firstGroup=false;
        const auto &b=key.bins;
        hotspots<<"{\"map\":"<<json(key.map)<<",\"sector\":"<<b[0]<<",\"position_bin\":["<<b[1]<<','<<b[2]<<','<<b[3]
                <<"],\"yaw_bin\":"<<b[4]<<",\"pitch_bin\":"<<b[5]<<",\"incidents\":[";
        bool first=true;
        for(const auto &i:r.incidents)if(i.key==key){if(!first)hotspots<<',';first=false;hotspots<<i.id;}
        hotspots<<"]}";
    }
    hotspots<<"\n]\n";
    summary<<"\nTotal time in slow frames during incidents: "<<total<<" ms\nSettings revisions: "<<r.revisions.size()
           <<" (see events.jsonl for changed keys)\nDropped incidents/export requests: "<<r.droppedIncidents<<'/'<<r.droppedExports<<'\n';
    for(size_t n=1;n<r.revisions.size();++n) {
        const auto &from=r.revisions[n-1],&to=r.revisions[n];
        summary<<"  Revision "<<to.previous<<" -> "<<to.id<<' '
               <<(from.id==to.previous&&!from.truncated&&!to.truncated?changes(from.settings,to.settings):"previous settings unavailable")<<'\n';
    }
    for(auto *out:{&session,&summary,&cfg,&frames,&events,&hotspots,&lights})out->close();
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
struct Session {
    Config config;
    bool enabled=false,seen=false;
    uint64_t serial=0,previous=0,slowSince=0,recoveredSince=0,shiftedSince=0,lastLights=0;
    uint64_t nextIncident=0,exportNumber=0;
    unsigned level=0,segment=0,revision=0,state=0;
    char map[16]{};
    int width=0,height=0,outputWidth=0,outputHeight=0;
    float smoothed=0;
    Key shiftedKey{};
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
            out->events=s.report.events;out->snapshots=s.report.snapshots;out->revisions=s.report.revisions;
            out->number=++s.exportNumber;out->droppedIncidents=s.report.droppedIncidents;
            out->droppedExports=s.report.droppedExports;out->droppedRevisions=s.report.droppedRevisions;
            out->overwrittenHistory=s.history.dropped;
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
void finishIncident(Session &s) {
    if(!s.active)return;
    if(s.completed.size()==incidentLimit) {
        s.completed.pop_front();
        ++s.report.droppedIncidents;
    }
    s.completed.push_back(std::move(s.active));
    if(s.config.autoExport)queue(s,s.completed.back().get());
}
void breakSegment(Session &s) {
    finishIncident(s);
    ++s.segment;
    s.previous=s.slowSince=s.recoveredSince=s.shiftedSince=0;
    s.smoothed=0;
}
void trigger(Session &s,const Frame &f,unsigned reason,uint64_t start) {
    s.active=std::make_unique<Incident>();
    auto &i=*s.active;
    i.id=++s.nextIncident;i.start=start;i.key=keyFor(f);i.trigger=f;i.reason=reason;
    i.budget=1000.0/s.config.target;
    size_t first=s.history.count;
    const uint64_t since=start>5000*ms?start-5000*ms:0;
    while(first>0 && s.history.at(first-1).segment==f.segment && s.history.at(first-1).time>=since)--first;
    const size_t count=s.history.count-first;
    const size_t stride=std::max<size_t>(1,(count+i.first.size()-1)/i.first.size());
    for(size_t j=first;j<s.history.count;++j) {
        const Frame &previous=s.history.at(j);
        if(previous.time>=start)i.measure(previous);
        if((j-first)%stride==0)i.keep(previous);
        else if(previous.time<start)++i.omittedPre;
        else ++i.omittedActive;
    }
    s.recoveredSince=s.shiftedSince=0;
}
void detect(Session &s,const Frame &f) {
    const double budget=1000.0/s.config.target;
    const float factor=1.f-std::exp(-std::min(f.interval,100.f)/150.f);
    if(!s.smoothed)s.smoothed=float(budget);
    s.smoothed+=factor*(std::min(f.interval,float(2*budget))-s.smoothed);
    if(s.smoothed>budget/.9) {if(!s.slowSince)s.slowSince=f.time;}
    else if(s.smoothed<budget/.94)s.slowSince=0;
    unsigned reason=0;
    if(s.config.mode!=0&&f.interval>=2*budget)reason|=f.interval>=3*budget?6u:2u;
    if(s.config.mode!=1&&s.slowSince&&f.time-s.slowSince>=uint64_t(s.config.duration)*ms)reason|=1u;
    if(s.active) {
        const auto key=keyFor(f);
        if(reason&&!(key==s.active->key)) {
            if(!s.shiftedSince||!(key==s.shiftedKey)){s.shiftedSince=f.time;s.shiftedKey=key;}
            if(s.active->recovery||f.time-s.shiftedSince>=250*ms) {
                finishIncident(s);
                trigger(s,f,reason,f.time);
            }
        } else s.shiftedSince=0;
        if(reason&&s.active->recovery)s.active->recovery=0;
    }
    if(!s.active&&reason)trigger(s,f,reason,(reason&1)?s.slowSince:f.time);
    if(!s.active)return;
    auto &i=*s.active;
    i.reason|=reason;
    if(s.smoothed<=budget/.96&&!reason) {
        if(!s.recoveredSince)s.recoveredSince=f.time;
        if(!i.recovery&&f.time-s.recoveredSince>=500*ms)i.recovery=f.time;
    } else s.recoveredSince=0;
    if(!i.recovery||i.recovery==f.time)i.measure(f);
    if(i.recovery&&f.time-i.recovery>3000*ms)finishIncident(s);
    else i.keep(f);
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
    if(s.enabled!=enabled||s.revision!=revision)breakSegment(s);
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
        breakSegment(*session);
    }
}
void submit(Frame &f,bool fromSave) {
    if(!session||!session->enabled)return;
    auto &s=*session;
    f.serial=++s.serial;
    f.revision=s.revision;
    if(f.hasView&&(!s.seen||f.level!=s.level)) {
        const char *name=fromSave?"savegame_load":s.seen&&!std::strcmp(s.map,f.map)?"map_restart":"map_load";
        event(s,f.time,name,s.level,f.level);
        breakSegment(s);
        s.level=f.level;
        std::memcpy(s.map,f.map,sizeof(s.map));
        s.seen=true;
    }
    if(f.width!=s.width||f.height!=s.height||f.outputWidth!=s.outputWidth||f.outputHeight!=s.outputHeight) {
        event(s,f.time,"resolution_change",unsigned(s.width),unsigned(f.width));
        breakSegment(s);
        s.width=f.width;s.height=f.height;s.outputWidth=f.outputWidth;s.outputHeight=f.outputHeight;
    }
    constexpr unsigned barriers=Panel|Menu|Paused|Unfocused|NoWorld|NoSwapchain;
    for(const auto &flag:{std::pair<unsigned,const char*>{Panel,"settings_panel"},{Menu,"game_menu"},
        {Paused,"pause"},{Unfocused,"focus_lost"},{NoWorld,"no_world"},{NoSwapchain,"swapchain_unavailable"}})
        if((s.state^f.state)&flag.first)event(s,f.time,flag.second,(s.state&flag.first)!=0,(f.state&flag.first)!=0);
    if((s.state^f.state)&barriers)breakSegment(s);
    if(f.state&(Loading|Targets|Screenshot)) {
        if(f.state&Loading)event(s,f.time,"renderer_rebuild",0,unsigned(std::max(0.f,f.loading)));
        if(f.state&Targets)event(s,f.time,"target_reallocation",0,unsigned(std::max(0.f,f.targets)));
        if(f.state&Screenshot)event(s,f.time,"screenshot");
        breakSegment(s);
    }
    s.state=f.state;
    f.segment=s.segment;
    if(!s.previous)f.interval=f.outside=-1;
    s.previous=f.time;
    if(f.state==0&&f.hasView&&f.interval>0)detect(s,f);
    s.history.push(f);
}
bool wantsLights(const Frame &f) {
    return session&&session->enabled&&session->config.detailed&&session->active&&f.state==0&&
        (!session->lastLights||f.time-session->lastLights>=250*ms);
}
void lights(const Frame &f,const Light *values,unsigned count) {
    if(!wantsLights(f))return;
    Snapshot shot{};
    shot.serial=f.serial;shot.time=f.time;shot.count=std::min(count,maxTraceLights);
    std::copy_n(values,shot.count,shot.values.begin());
    session->report.snapshots.push(shot);
    session->lastLights=f.time;
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
        finishIncident(*session);
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
