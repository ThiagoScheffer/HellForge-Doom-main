#include "../platform/performance_trace.h"
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
        config.autoExport=false;config.duration=250;
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
                submit(f,save);
                if(wantsLights(f)) { Light l;l.category=6;lights(f,&l,1); }
            }
        };
        run(2100,15);
        run(1,60);                 // Single severe spike.
        run(280,15);               // 500 ms hysteresis and three seconds post-recovery.
        run(50,25);                // Sustained, with no frame above the spike threshold.
        f.eye[0]=320;run(50,25);    // Same map, distinct camera location.
        f.level=2;run(1,15,true);   // Save load in the same map must close the incident.
        require(exportReport(),"Detection report could not be queued");
        shutdown();
        const auto captured=fs::directory_iterator(output/"Diagnostics"/"Performance")->path();
        const auto detection=read(captured/"summary.txt");
        const auto timeline=read(captured/"frames.csv");
        const auto hotspots=read(captured/"hotspots.json");
        require(detection.find("Incidents: 3")!=std::string::npos,"Slowdown, spike or hotspot grouping failed");
        require(detection.find("flags 6")!=std::string::npos,"Severe spike was not captured");
        require(detection.find("flags 1")!=std::string::npos,"Sustained slowdown was not captured");
        require(timeline.find(",pre,")!=std::string::npos&&timeline.find(",recovery,")!=std::string::npos,
            "Incident history or recovery was not retained");
        require(hotspots.find("\"position_bin\":[2,0,0]")!=std::string::npos&&
                hotspots.find("\"position_bin\":[5,0,0]")!=std::string::npos,"Distinct hotspots were merged");
        require(read(captured/"events.jsonl").find("\"event\":\"savegame_load\"")!=std::string::npos,"Save load event missing");

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
