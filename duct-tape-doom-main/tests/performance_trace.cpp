#include "../platform/performance_trace.h"
#include "../platform/surface_lighting.h"
#include <SDL3/SDL.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace doom3d::performance;
namespace fs=std::filesystem;

static void require(bool pass,const char *message) {
    if(!pass)throw std::runtime_error(message);
}
static std::string read(const fs::path &path) {
    std::ifstream file(path,std::ios::binary);
    require(bool(file),"Missing report file");
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}

int main() {
    const auto original=fs::current_path();
    try {
        SDL_Time utc=0;
        SDL_GetCurrentTime(&utc);
        const auto output=original/"performance-trace-test"/std::to_string(utc);
        fs::create_directories(output);
        fs::current_path(output);
        Config config;
        config.autoExport=false;config.duration=500;
        configure(false,config,0,{});
        require(!exportReport(),"Default disabled tracing allocated a session");
        configure(true,config,1,"caustics 1\nemissive 1\n");
        identify("synthetic","test \"GPU\"","fifo",1);
        files("DOOM2.WAD","example,quoted\".wad");
        uint64_t time=1000000000;
        Frame f;
        f.hasView=true;f.level=1;f.width=1280;f.height=800;f.eye[0]=128;
        std::strcpy(f.map,"MAP01");
        std::strcpy(f.floor,"NUKAGE1");
        auto run=[&](int count,float duration,bool save=false) {
            for(int i=0;i<count;++i) {
                time+=uint64_t(duration*1000000);
                f.time=time;f.interval=duration;f.outside=2;f.acquire=1;f.prepare=5;
                Evidence evidence;
                evidence.count=1;
                evidence.lights[0].category=6;
                evidence.lights[0].strength=duration;
                evidence.lights[0].baked=1;
                std::strcpy(evidence.lights[0].material,"NUKAGE3");
                submit(f,save,&evidence);
            }
        };
        run(2100,15);
        run(8,25);run(60,16.667f); // Candidate shorter than 500 ms.
        run(1,45);                 // Single non-severe spike.
        run(280,15);               // 500 ms hysteresis and three seconds post-recovery.
        run(50,25);                // Sustained, with no frame above the spike threshold.
        f.eye[0]=320;f.pitch=-65;run(50,25);
        f.pitch=-89;run(20,25);run(12,17);run(1,55);run(20,25); // Interrupted recovery and new peak.
        run(300,16.667f); // Confirm recovery and post-capture.
        f.level=2;run(1,15,true);   // Save load in the same map must close the incident.
        require(exportReport(),"Detection report could not be queued");
        shutdown();
        const auto captured=fs::directory_iterator(output/"Diagnostics"/"Performance")->path();
        const auto detection=read(captured/"summary.txt");
        const auto timeline=read(captured/"frames.csv");
        const auto hotspots=read(captured/"hotspots.json");
        require(detection.find("Incidents: 2")!=std::string::npos,"Slowdown, spike or hotspot grouping failed");
        require(detection.find("flags 2 (")!=std::string::npos,"Isolated spike was misclassified");
        require(detection.find("flags 7 (")!=std::string::npos,"Spike did not attach to the sustained parent");
        const auto incidents=read(captured/"incidents.json");
        require(incidents.find("\"termination\":\"recovered\"")!=std::string::npos,"Recovery did not close parent");
        require(incidents.find("\"pitch_bin\":-6")!=std::string::npos,"Downward segment missing");
        require(incidents.find("\"incident_frames_lost\":0")!=std::string::npos,"Ordinary replacement counted as incident loss");
        const auto shots=read(captured/"lights.csv");
        require(shots.find(",55,1,0,0,,2,incident_peak,\"NUKAGE3\"")!=std::string::npos,"Peak light payload belongs to wrong frame");
        MaskWork a,b,mirror;
        const unsigned surface[]{0u,1u},decoration[]{1u<<31,0u};
        const unsigned no[]{0,0},one[]{0,1},three[]{1u<<31,3};
        a.account(no,surface,decoration);a.account(one,surface,decoration);
        b.account(three,surface,decoration);
        a.add(b);mirror.add(b);
        require(a.processed==3&&a.lit==2&&a.pairs==4&&a.maximum==3&&a.surface==2&&a.decoration==1,
                "Mask pair reduction did not count both words or categories");
        require(mirror.processed==1,"Mirror population mixed with main counts");
        require(timeline.find(",pre,")!=std::string::npos&&timeline.find(",recovery,")!=std::string::npos,
            "Incident history or recovery was not retained");
        require(hotspots.find("\"position_bin\":[2,0,0]")!=std::string::npos&&
                hotspots.find("\"position_bin\":[5,0,0]")!=std::string::npos,"Distinct hotspots were merged");
        require(read(captured/"events.jsonl").find("\"event\":\"savegame_load\"")!=std::string::npos,"Save load event missing");

        SurfaceOrigin fixture{4,8,11,0,1};
        fixture.merge({4,8,12,1,1});
        require(fixture.material==4&&fixture.sector==8&&fixture.line==-1&&fixture.contributors==2,
            "Merged sources must keep only common provenance");
        fixture.merge({5,9,11,0,1});
        require(fixture.material==SurfaceOrigin::unknown&&fixture.sector==-1&&fixture.contributors==3,
            "Conflicting contributors were attributed to a single source");

        const auto downsample=output/"downsample";
        fs::create_directories(downsample);
        fs::current_path(downsample);
        configure(true,config,1,"emissive 1\n");
        run(340,15);
        run(1,55);
        run(1000,25);
        require(exportReport(),"Cannot export long active incident");
        const auto traceRoot=downsample/"Diagnostics"/"Performance";
        fs::path firstExport;
        for(int attempt=0;attempt<400;++attempt) {
            if(fs::exists(traceRoot)) {
                for(const auto &entry:fs::directory_iterator(traceRoot))firstExport=entry.path();
                if(!firstExport.empty()&&exportReport())break;
            }
            SDL_Delay(5);
        }
        shutdown();
        require(!firstExport.empty(),"Long incident report missing");
        auto duplicate=firstExport;
        duplicate.replace_filename(firstExport.filename().u8string().substr(0,firstExport.filename().u8string().size()-1)+"2");
        require(read(firstExport/"incidents.json")==read(duplicate/"incidents.json"),"Duplicate export changed incident identity");
        const auto full=read(firstExport/"incidents.json");
        require(full.find("\"active_frames_observed\":1001")!=std::string::npos&&
                full.find("\"active_max_ms\":55")!=std::string::npos&&
                full.find("\"active_ms\":25055")!=std::string::npos&&
                full.find("\"retained_sample_p95_ms\"")!=std::string::npos,
                "Intentional downsampling changed exact aggregate populations");
        require(read(firstExport/"lights.csv").find(",55,1,0,0,,1,incident_peak,")!=std::string::npos,
                "Peak from the start of a long incident was replaced");

        const auto retention=output/"retention";
        fs::create_directories(retention);
        fs::current_path(retention);
        configure(true,config,1,"caustics 1\nemissive 1\n");
        identify("synthetic","test \"GPU\"","fifo",1);
        files("DOOM2.WAD","example,quoted\".wad");
        run(2100,15);
        configure(true,config,2,"caustics 0\nemissive 1\n");
        for(unsigned level=3;level<38;++level) {
            f.level=level;run(2,15);run(1,40); // Exceed the incident cap.
        }
        configure(false,config,3,"caustics 0\nemissive 1\n");
        const auto last=f.serial;
        run(10,60);
        require(f.serial==last,"Disabled tracer accepted new samples");
        require(exportReport(),"Manual export could not be queued");
        shutdown();
        const auto folder=fs::directory_iterator(retention/"Diagnostics"/"Performance")->path();
        const auto summary=read(folder/"summary.txt");
        const auto events=read(folder/"events.jsonl");
        const auto session=read(folder/"session.json");
        const auto frames=read(folder/"frames.csv");
        const auto groups=read(folder/"hotspots.json");
        require(summary.find("Incidents: 32")!=std::string::npos,"Incident retention did not cap at 32");
        require(session.find("\"dropped_incidents\":0")==std::string::npos,"Dropped incidents were not recorded");
        require(session.find("\"history_overwritten\":0")==std::string::npos,"Frame history never wrapped");
        require(session.find("\"gpu_execution_ms\":null")!=std::string::npos,"Unavailable GPU time was not explicit");
        require(session.find("test \\\"GPU\\\"")!=std::string::npos,"GPU metadata was not JSON escaped");
        require(events.find("\"key\":\"caustics\",\"previous\":\"1\",\"value\":\"0\"")!=std::string::npos,"Settings diff missing");
        require(events.find("\"event\":\"tracer_disabled\"")!=std::string::npos,"Disable event missing");
        require(frames.find("\"MAP01\"")!=std::string::npos&&frames.find("\"NUKAGE1\"")!=std::string::npos,"Frame pose/material missing");
        require(groups.find("\"position_bin\":[5,0,0]")!=std::string::npos,"Hotspot key missing");
        const auto automatic=output/"automatic";
        fs::create_directories(automatic);
        fs::current_path(automatic);
        config.autoExport=true;
        configure(true,config,1,"emissive 1\n");
        run(2,15);run(1,60);
        f.level++;run(1,15);
        shutdown();
        const auto autoFolder=fs::directory_iterator(automatic/"Diagnostics"/"Performance")->path();
        require(read(autoFolder/"summary.txt").find("Incidents: 1")!=std::string::npos,"Automatic export lost the incident");
        fs::current_path(original);
        std::cout<<"Performance trace contracts passed. Report "<<folder.u8string()<<'\n';
        return 0;
    } catch(const std::exception &e) {
        shutdown();
        fs::current_path(original);
        std::cerr<<e.what()<<'\n';
        return 1;
    }
}
