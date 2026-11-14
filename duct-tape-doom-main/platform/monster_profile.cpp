// Diagnostic sidecar only: no actor fields, RNG or gameplay decisions.
#include "monster_profile.h"
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <vector>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <atomic>
#include <string>
#include <cctype>
extern "C" {
#include "doomdef.h"
#include "doomstat.h"
#include "p_local.h"
#include "m_argv.h"
}
#ifdef DOOM_MONSTER_PROFILING
namespace {
std::atomic<int> enabled{0};
std::atomic<unsigned> generation{0};
struct Metric {uint64_t calls=0,inclusive=0,exclusive=0,maximum=0;};
struct Frame {uint64_t start=0,children=0;unsigned category=0;};
struct Thread {
    unsigned generation=0;
    std::array<Frame,256> stack{};unsigned depth=0;
    std::array<Metric,MP_CATEGORIES> current{},total{};
    std::array<std::array<uint64_t,8>,MP_CATEGORIES> histogram{};
    std::array<Metric,4> searchSight{};
    std::array<uint64_t,MP_COUNTERS> counters{},counterTotal{};
    std::array<Metric,NUMMOBJTYPES> types{};
};
std::mutex registryMutex;
std::vector<std::unique_ptr<Thread>> threads;
thread_local Thread *local=nullptr;
Thread &thread() {
    if(!local){std::lock_guard<std::mutex> lock(registryMutex);threads.emplace_back(new Thread);local=threads.back().get();}
    const auto epoch=generation.load(std::memory_order_acquire);
    if(!local->depth&&local->generation!=epoch){
        local->current={};local->total={};local->counters={};local->counterTotal={};local->types={};local->histogram={};local->searchSight={};local->generation=epoch;
    }
    return *local;
}
struct Sample {int tic;std::array<Metric,MP_CATEGORIES> metrics;std::array<uint64_t,MP_COUNTERS> counters;int mobjs,eligible,targeted,moving,unknown;std::array<int,4> bins;bool population,work;uint64_t populationNs;};
std::vector<Sample> samples;
std::filesystem::path folder;
bool requested=false,population=false,externalTics=false;
uint64_t startupClock=0;
int on=1500,off=1200,workMoving=128,workQueries=1500;
double distances[3]={512,1024,2048};
uint64_t dropped=0;
uint64_t windows=0;
bool lastFlushSucceeded=false,flushErrorReported=false;
bool rotating=false;
std::atomic<uint64_t> overflows{0};
std::unique_ptr<std::unordered_map<mobj_t*,std::array<fixed_t,3>>> previousPositions;
unsigned positionLevel=~0u;
std::array<bool,NUMMOBJTYPES> patchedTypes{};
constexpr uint64_t queryBounds[]={1000,4000,16000,64000,256000,1000000,4000000,UINT64_MAX};
const char *names[]={"tic","thinkers","mobj","look","chase","player_search","position","try_move","block_things","block_lines","sight","solid_sight","fluid_chase","fluid_tick","support","tactics","feeding","frenzy","geometry","render","sprite_light","shadow","subsector","dig_position","dig_height","gpu_upload","gpu_encode","gpu_submit","tactics_move"
#ifdef DOOM_MONSTER_PROFILE_ARGUMENTS
,"argument_search"
#endif
};
const char *counterNames[]={"thing_visits","line_visits","line_duplicates","thing_callbacks","line_callbacks","iterator_stops","sight_reject","sight_bsp","support_hit","support_miss","support_clear","support_invalidate","fluid_overlap","fluid_steps","fluid_failed","feeding_candidates","frenzy_candidates","position_failed","move_failed","support_peak_entries","players_acquired","look_activations","sight_visible","sight_blocked","tactics_candidates"};
static_assert(sizeof(names)/sizeof(*names)==MP_CATEGORIES,"profile category names");
static_assert(sizeof(counterNames)/sizeof(*counterNames)==MP_COUNTERS,"profile counter names");
int argument(const char *key,int fallback){int n=M_CheckParm((char*)key);return n&&n+1<myargc?atoi(myargv[n+1]):fallback;}
bool sameOption(const char *a,const char *b){
    while(*a&&*b&&std::tolower((unsigned char)*a)==std::tolower((unsigned char)*b)){++a;++b;}
    return !*a&&!*b;
}
}
int MP_Enabled(void){return enabled.load(std::memory_order_acquire);}
void MP_SetEnabled(int value){enabled.store(value,std::memory_order_release);}
void MP_PatchedType(int type){if(mp_enabled&&type>=0&&type<NUMMOBJTYPES)patchedTypes[type]=true;}
void MP_ForgetActor(const void *actor){if(mp_enabled&&previousPositions)previousPositions->erase((mobj_t*)actor);}
uint64_t MP_Now(void){return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
void MP_Startup(void){
    if(myargc<1||!myargv)return;
    int at=M_CheckParm((char*)"-profilemonsters");
    if(at){
        if(at+1<myargc&&myargv[at+1][0]!='-')MP_Init(myargv[at+1]);
        else fprintf(stderr,"Monster profiler disabled: -profilemonsters requires an output directory\n");
    }
    // These controls belong to the observer, not the game. Remove them after
    // parsing so hot M_CheckParm scans and gameplay argument indices match an
    // otherwise identical launch without profiling.
    const char *controls[]={"-profilemonsters","-profile-on","-profile-off","-profile-moving","-profile-queries","-profile-distances"};
    int output=1;
    for(int i=1;i<myargc;++i){
        int kind=-1;for(int k=0;k<6;++k)if(sameOption(myargv[i],controls[k])){kind=k;break;}
        if(kind<0){myargv[output++]=myargv[i];continue;}
        if(i+1<myargc&&(myargv[i+1][0]!='-'||(kind>0&&std::isdigit((unsigned char)myargv[i+1][1]))))++i;
    }
    if(output<myargc)myargv[output]=nullptr;
    myargc=output;
}
MP_Scope MP_Begin(unsigned category) {
    MP_Scope result={};if(!mp_enabled)return result;
    auto &t=thread();
    if(t.depth>=t.stack.size()){++overflows;return result;}
    result.active=1;result.depth=t.depth;result.category=category;result.start=MP_Now();
    t.stack[t.depth++]={result.start,0,category};return result;
}
void MP_End(MP_Scope scope) {
    if(!scope.active)return;
    uint64_t end=MP_Now();auto &t=thread();
    if(!t.depth||t.depth!=scope.depth+1){++overflows;return;}
    auto frame=t.stack[--t.depth];uint64_t elapsed=end-frame.start;
    auto &m=t.current[frame.category];++m.calls;m.inclusive+=elapsed;m.exclusive+=elapsed>=frame.children?elapsed-frame.children:0;m.maximum=std::max(m.maximum,elapsed);
    auto &total=t.total[frame.category];++total.calls;total.inclusive+=elapsed;total.exclusive+=elapsed>=frame.children?elapsed-frame.children:0;total.maximum=std::max(total.maximum,elapsed);
    size_t bucket=std::lower_bound(std::begin(queryBounds),std::end(queryBounds),elapsed)-std::begin(queryBounds);++t.histogram[frame.category][bucket];
    if(frame.category==MP_SIGHT)for(unsigned n=t.depth;n>0;--n){
        unsigned category=t.stack[n-1].category;
        int role=category==MP_PLAYERS?0:category==MP_TACTIC_MOVE||category==MP_TACTICS?1:category==MP_FEED?2:category==MP_FRENZY?3:-1;
        if(role<0)continue;auto &s=t.searchSight[role];++s.calls;s.inclusive+=elapsed;s.exclusive+=elapsed>=frame.children?elapsed-frame.children:0;s.maximum=std::max(s.maximum,elapsed);break;
    }
    if(frame.category==MP_MOBJ&&scope.actorType>=0&&scope.actorType<NUMMOBJTYPES){auto &a=t.types[scope.actorType];++a.calls;a.inclusive+=elapsed;a.exclusive+=elapsed>=frame.children?elapsed-frame.children:0;a.maximum=std::max(a.maximum,elapsed);}
    if(t.depth)t.stack[t.depth-1].children+=elapsed;
}
void MP_Count(unsigned counter,uint64_t amount){auto &t=thread();t.counters[counter]+=amount;t.counterTotal[counter]+=amount;}
void MP_Max(unsigned counter,uint64_t amount){auto &t=thread();t.counters[counter]=std::max(t.counters[counter],amount);t.counterTotal[counter]=std::max(t.counterTotal[counter],amount);}
void MP_Stage(const char *stage){
    if(!requested||!startupClock)return;
    fprintf(stderr,"MONSTER_STAGE elapsed_ns %llu %s\n",(unsigned long long)(MP_Now()-startupClock),stage);
    fflush(stderr);
}
void MP_Init(const char *path) {
    if(requested)return;requested=true;startupClock=MP_Now();folder=std::filesystem::u8path(path);
    std::error_code error;std::filesystem::create_directories(folder,error);
    if(error){fprintf(stderr,"Monster profiler disabled: %s\n",error.message().c_str());requested=false;return;}
    previousPositions=std::make_unique<std::unordered_map<mobj_t*,std::array<fixed_t,3>>>();
    samples.reserve(8192);
    on=std::max(1,argument("-profile-on",1500));off=std::clamp(argument("-profile-off",1200),0,on);
    workMoving=std::max(1,argument("-profile-moving",128));workQueries=std::max(1,argument("-profile-queries",1500));
    int at=M_CheckParm((char*)"-profile-distances");
    if(at&&at+1<myargc){double a,b,c;if(sscanf(myargv[at+1],"%lf,%lf,%lf",&a,&b,&c)==3&&std::isfinite(a)&&std::isfinite(b)&&std::isfinite(c)&&a>0&&b>a&&c>b){distances[0]=a;distances[1]=b;distances[2]=c;}}
    MP_SetEnabled(1);thread();
    auto start=MP_Now();for(int i=0;i<100000;++i){auto s=MP_Begin(MP_LOOK);MP_End(s);}auto cost=MP_Now()-start;
    auto timerStart=MP_Now();for(int i=0;i<100000;++i)(void)MP_Now();auto timerCost=MP_Now()-timerStart;
    auto counterStart=MP_Now();for(int i=0;i<100000;++i)MP_Count(MP_THING_VISITS,1);auto counterCost=MP_Now()-counterStart;
    std::ofstream meta(folder/"calibration.txt");
    meta<<"empty_scope_ns "<<cost/100000.0<<"\nclock_read_ns "<<timerCost/100000.0<<"\ncounter_direct_ns "<<counterCost/100000.0<<"\nclock_period "<<std::chrono::steady_clock::period::num<<"/"<<std::chrono::steady_clock::period::den<<"\non "<<on<<"\noff "<<off<<"\nwork_moving "<<workMoving<<"\nwork_queries "<<workQueries<<"\ndistances "<<distances[0]<<","<<distances[1]<<","<<distances[2]<<"\n";
    for(auto &p:threads){p->current={};p->total={};p->counters={};p->counterTotal={};p->types={};p->histogram={};p->searchSight={};}
    atexit(MP_Flush);
}
void MP_Window(int enabled) {
    // Only the owning thread resets its records. A background job crossing a
    // boundary remains in its previous generation and is omitted from totals.
    // Never write a worker's TLS data from the main thread.
    if(enabled)generation.fetch_add(1,std::memory_order_acq_rel);
    MP_SetEnabled(requested&&enabled);
    if(local){auto &main=thread();main.current={};main.counters={};}
}
void MP_Tic(int tic) {
    if(!mp_enabled)return;
    auto &t=thread();Sample s={};s.tic=tic;s.metrics=t.current;s.counters=t.counters;t.current={};t.counters={};
    uint64_t begin=MP_Now();
    if(positionLevel!=r_levelserial){previousPositions->clear();population=false;positionLevel=r_levelserial;}
    for(auto *th=thinkercap.next;th!=&thinkercap;th=th->next){
        if(th->function.acp1!=(actionf_p1)P_MobjThinker)continue;
        auto *mo=(mobj_t*)th;++s.mobjs;
        if(mo->player||mo->health<=0||!(mo->flags&MF_SHOOTABLE)||(!(mo->flags&MF_COUNTKILL)&&mo->type!=MT_SKULL)){previousPositions->erase(mo);continue;}
        ++s.eligible;
        if(mo->type>=NUMMOBJTYPES_VANILLA||(mo->type>=0&&mo->type<NUMMOBJTYPES&&patchedTypes[mo->type]))++s.unknown;
        if(mo->target&&mo->target->health>0)++s.targeted;
        std::array<fixed_t,3> position{mo->x,mo->y,mo->z};
        auto before=previousPositions->find(mo);
        if(before!=previousPositions->end()&&before->second!=position)++s.moving;
        (*previousPositions)[mo]=position;
        double distance=INFINITY;
        for(int i=0;i<MAXPLAYERS;++i)if(playeringame[i]&&players[i].mo&&players[i].health>0&&players[i].mo->health>0&&players[i].playerstate==PST_LIVE){
            double dx=((double)mo->x-players[i].mo->x)/FRACUNIT,dy=((double)mo->y-players[i].mo->y)/FRACUNIT;distance=std::min(distance,dx*dx+dy*dy);
        }
        int bin=0;while(bin<3&&distance>distances[bin]*distances[bin])++bin;++s.bins[bin];
    }
    if(!population&&s.eligible>=on)population=true;else if(population&&s.eligible<off)population=false;
    s.population=population;s.work=population&&(s.moving>=workMoving||s.metrics[MP_POSITION].calls>=(unsigned)workQueries);s.populationNs=MP_Now()-begin;
    if(samples.size()==8192){
        // Rotate bounded diagnostic windows rather than retaining unbounded data
        // or silently losing every sample after a few minutes of normal play.
        auto base=folder;auto destination=base/("window-"+std::to_string(windows));
        std::error_code error;std::filesystem::create_directories(destination,error);
        if(!error){folder=destination;rotating=true;MP_Flush();rotating=false;folder=base;if(lastFlushSucceeded){samples.clear();++windows;}}
    }
    if(samples.size()<8192)samples.push_back(s);else ++dropped;
}
void MP_ExternalTics(void){externalTics=true;}
void MP_AutoTic(int tic){if(!externalTics)MP_Tic(tic);}
void MP_Flush(void) {
    if(!requested)return;
    std::ofstream timings(folder/"scopes.csv"),counts(folder/"counters.csv"),pop(folder/"population.csv"),totals(folder/"threads.csv");
    timings<<"tic,category,calls,inclusive_ns,exclusive_ns,max_query_ns\n";counts<<"tic,counter,value\n";
    pop<<"tic,mobjs,eligible,targeted,moving,patched,near512,near1024,near2048,far,population_trigger,work_trigger,population_ns\n";
    for(const auto &s:samples){
        for(int c=0;c<MP_CATEGORIES;++c){const auto &m=s.metrics[c];timings<<s.tic<<','<<names[c]<<','<<m.calls<<','<<m.inclusive<<','<<m.exclusive<<','<<m.maximum<<'\n';}
        for(int c=0;c<MP_COUNTERS;++c)counts<<s.tic<<','<<counterNames[c]<<','<<s.counters[c]<<'\n';
        pop<<s.tic<<','<<s.mobjs<<','<<s.eligible<<','<<s.targeted<<','<<s.moving<<','<<s.unknown;for(int n:s.bins)pop<<','<<n;pop<<','<<s.population<<','<<s.work<<','<<s.populationNs<<'\n';
    }
    totals<<"thread,category,calls,inclusive_ns,exclusive_ns,max_query_ns\n";
    std::ofstream histogram(folder/"query_histogram.csv");histogram<<"thread,category,upper_inclusive_ns,calls\n";
    std::ofstream searches(folder/"search_visibility.csv");searches<<"thread,search,calls,inclusive_ns,exclusive_ns,max_query_ns\n";
    const char *roles[]={"player_acquisition","tactics","feeding","frenzy"};
    std::vector<Thread*> snapshot;
    {std::lock_guard<std::mutex> lock(registryMutex);for(auto &t:threads)snapshot.push_back(t.get());}
    // Rolling writes occur during play and read only main-thread records.
    // Final explicit/exit flushing must follow renderer worker shutdown.
    for(size_t i=0;i<snapshot.size();++i){auto *t=snapshot[i];if(rotating&&t!=local)continue;
        if(t->generation!=generation.load(std::memory_order_relaxed))continue;
        for(int c=0;c<MP_CATEGORIES;++c){const auto &m=t->total[c];totals<<i<<','<<names[c]<<','<<m.calls<<','<<m.inclusive<<','<<m.exclusive<<','<<m.maximum<<'\n';
            for(int b=0;b<8;++b)histogram<<i<<','<<names[c]<<','<<queryBounds[b]<<','<<t->histogram[c][b]<<'\n';}
        for(int role=0;role<4;++role){const auto &s=t->searchSight[role];searches<<i<<','<<roles[role]<<','<<s.calls<<','<<s.inclusive<<','<<s.exclusive<<','<<s.maximum<<'\n';}}
    std::ofstream actors(folder/"actor_types.csv");actors<<"thread,type,calls,inclusive_ns,exclusive_ns,max_query_ns\n";
    for(size_t i=0;i<snapshot.size();++i){auto *t=snapshot[i];if(rotating&&t!=local)continue;
        if(t->generation!=generation.load(std::memory_order_relaxed))continue;
        for(int c=0;c<NUMMOBJTYPES;++c){const auto &m=t->types[c];if(m.calls)actors<<i<<','<<c<<','<<m.calls<<','<<m.inclusive<<','<<m.exclusive<<','<<m.maximum<<'\n';}}
    std::ofstream status(folder/"status.txt");status<<"dropped "<<dropped<<"\nstack_errors "<<overflows.load()<<"\nrotated_windows "<<windows
        <<"\nregistered_threads "<<snapshot.size()<<"\nsample_capacity_bytes "<<samples.capacity()*sizeof(Sample)
        <<"\nthread_records_bytes "<<snapshot.size()*sizeof(Thread)<<"\npopulation_entries "<<(previousPositions?previousPositions->size():0)<<"\n";
    timings.flush();counts.flush();pop.flush();totals.flush();actors.flush();status.flush();histogram.flush();searches.flush();
    lastFlushSucceeded=timings.good()&&counts.good()&&pop.good()&&totals.good()&&actors.good()&&status.good()&&histogram.good()&&searches.good();
    if(!lastFlushSucceeded&&!flushErrorReported){fprintf(stderr,"Monster profiler: diagnostic output write failed; gameplay continues\n");flushErrorReported=true;}
}
#endif
