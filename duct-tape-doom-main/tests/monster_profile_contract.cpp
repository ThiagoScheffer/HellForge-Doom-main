#include "../platform/monster_profile.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <stdexcept>
#include <vector>
#include <atomic>
extern "C" {
#include "p_local.h"
#include "m_argv.h"
#include "doomstat.h"
}
static void require(bool value,const char *label){if(!value)throw std::runtime_error(label);std::cout<<"PASS "<<label<<"\n";}
int main(int argc,char **argv){
    auto root=std::filesystem::absolute("monster-profile-contract");std::filesystem::create_directories(root);
    P_InitThinkers();
    if(argc>1&&std::string(argv[1])=="bad-output"){
        std::ofstream(root/"blocker")<<"keep";
        MP_Init((root/"blocker"/"child").string().c_str());require(!mp_enabled,"unavailable output disables profiling");return 0;
    }
    require(!mp_enabled,"profiling defaults off");
    MP_Init(root.string().c_str());MP_Window(1);
    auto outer=MP_Begin(MP_TIC);auto child=MP_Begin(MP_SIGHT);
    MP_Count(MP_THING_VISITS,7);MP_End(child);
    std::thread worker([]{for(int i=0;i<1000;++i){auto scope=MP_Begin(MP_SPRITE_LIGHT);MP_End(scope);}});
    worker.join();MP_End(outer);MP_Tic(0);MP_Flush();
    std::ifstream file(root/"scopes.csv");std::string line;std::getline(file,line);
    uint64_t parentInclusive=0,parentExclusive=0,childInclusive=0;
    while(std::getline(file,line)){
        std::istringstream stream(line);std::string tic,name,calls,inclusive,exclusive;
        std::getline(stream,tic,',');std::getline(stream,name,',');std::getline(stream,calls,',');std::getline(stream,inclusive,',');std::getline(stream,exclusive,',');
        auto in=std::stoull(inclusive),ex=std::stoull(exclusive);require(in>=ex,"inclusive bounds exclusive");
        if(name=="tic"){parentInclusive=in;parentExclusive=ex;}
        if(name=="sight")childInclusive=in;
    }
    require(parentInclusive==parentExclusive+childInclusive,"nested child time subtracted once");
    std::ifstream threads(root/"threads.csv");std::string text((std::istreambuf_iterator<char>(threads)),{});
    require(text.find("1,sprite_light,1000,")!=std::string::npos,"worker scopes separated from main thread");
    std::ifstream buckets(root/"query_histogram.csv");std::getline(buckets,line);uint64_t bucketCalls=0;
    while(std::getline(buckets,line)){
        std::istringstream input(line);std::string id,category,bound,calls;
        std::getline(input,id,',');std::getline(input,category,',');std::getline(input,bound,',');std::getline(input,calls,',');
        if(id=="1"&&category=="sprite_light")bucketCalls+=std::stoull(calls);
    }
    require(bucketCalls==1000,"query-duration histogram accounts for every worker scope");
    auto feeding=MP_Begin(MP_FEED);auto visibility=MP_Begin(MP_SIGHT);MP_End(visibility);MP_End(feeding);MP_Flush();
    std::ifstream roleFile(root/"search_visibility.csv");std::string roleText((std::istreambuf_iterator<char>(roleFile)),{});
    require(roleText.find("0,feeding,1,")!=std::string::npos,"visibility is attributed to its nearest search scope");
    std::vector<mobj_t> mobjs(1500);
    for(auto &mo:mobjs){mo.thinker.function.acp1=(actionf_p1)P_MobjThinker;mo.health=100;mo.flags=MF_COUNTKILL|MF_SHOOTABLE;mo.type=MT_TROOP;mo.x=3000*FRACUNIT;P_AddThinker(&mo.thinker);}
    MP_PatchedType(MT_TROOP);MP_Tic(1);for(int i=0;i<300;++i)mobjs[i].health=0;MP_Tic(2);
    mobjs[300].health=0;MP_Tic(3);
    for(auto &mo:mobjs)mo.health=100;MP_Tic(4);
    MP_ForgetActor(&mobjs[0]);mobjs[0].x+=FRACUNIT;MP_Tic(5);
    mobj_t player={};player.health=100;player.x=2900*FRACUNIT;player.z=1000*FRACUNIT;
    playeringame[0]=true;players[0].mo=&player;players[0].health=100;players[0].playerstate=PST_LIVE;MP_Tic(6);
    players[0].playerstate=PST_DEAD;MP_Tic(7);playeringame[0]=false;players[0].mo=nullptr;MP_Flush();
    std::ifstream population(root/"population.csv");std::getline(population,line);
    std::vector<std::vector<std::string>> records;
    while(std::getline(population,line)){std::vector<std::string> values;std::istringstream input(line);std::string value;while(std::getline(input,value,','))values.push_back(value);records.push_back(values);}
    require(records[1][2]=="1500"&&records[1][10]=="1","population threshold activates at 1500");
    require(records[1][5]=="1500","patched stock actor types are reported as uncertain");
    require(records[2][2]=="1200"&&records[2][10]=="1","population hysteresis remains on at 1200");
    require(records[3][2]=="1199"&&records[3][10]=="0","population hysteresis switches off below 1200");
    require(records[4][10]=="1"&&records[4][11]=="0","resurrection recount and workload trigger separated");
    require(records[5][4]=="0","removed actor history cannot mark a reused address as moving");
    require(records[6][6]=="1500","distance bins use horizontal distance to a living player");
    require(records[7][9]=="1500","dead players are excluded from nearest-player bins");
    std::atomic<bool> ready{false},release{false};
    std::thread crossing([&]{
        auto old=MP_Begin(MP_LOOK);ready.store(true);
        while(!release.load())std::this_thread::yield();
        MP_End(old);auto fresh=MP_Begin(MP_LOOK);MP_End(fresh);
    });
    while(!ready.load())std::this_thread::yield();
    MP_Window(1);release.store(true);crossing.join();MP_Flush();
    std::ifstream epochs(root/"threads.csv");std::string epochText((std::istreambuf_iterator<char>(epochs)),{});
    require(epochText.find("2,look,1,")!=std::string::npos,"worker resets its own generation and excludes crossing scope");
    P_InitThinkers();
    MP_Window(0);auto disabled=MP_Begin(MP_CHASE);require(!disabled.active,"runtime-off scope does not start");
    MP_Window(1);for(int i=5;i<8200;++i)MP_Tic(i);MP_Window(0);MP_Flush();
    std::ifstream status(root/"status.txt");std::string dropped;uint64_t count;status>>dropped>>count;
    require(count==0,"bounded buffer rotates without dropping samples");status>>dropped>>count;require(count==0,"scope stack balanced");
    status>>dropped>>count;require(count==1&&std::filesystem::exists(root/"window-0"/"scopes.csv"),"full diagnostic window flushed separately");
    char program[]="contract",profile[]="-PROFILEMONSTERS",path[]="ignored-existing-session",queries[]="-profile-queries",number[]="1500",iwad[]="-iwad",game[]="keep.wad",skill[]="-skill",hard[]="4";
    char *arguments[]={program,profile,path,queries,number,iwad,game,skill,hard,nullptr};myargc=9;myargv=arguments;MP_Startup();
    require(myargc==5&&std::string(myargv[1])=="-iwad"&&std::string(myargv[3])=="-skill","diagnostic controls are removed without changing game argument order");
    myargc=0;myargv=nullptr;
}
