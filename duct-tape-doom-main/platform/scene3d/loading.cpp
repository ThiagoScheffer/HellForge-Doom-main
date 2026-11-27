/* The loading screen. A level's first frame builds the map and runs the
   bakes, which on a big map takes seconds; the backend runs that frame's
   work in a loading pass first (runLoading), and the stages report here so
   the screen shows how far it got instead of a frozen window. Settings
   changes that rebake show it too, once the work takes long enough. */
#include "internal.h"
extern "C" {
#include "hu_stuff.h"
}

namespace doom3d {
namespace {
constexpr int stageCount=(int)LoadStage::Count;
const char *const stageNames[stageCount]={"Preparing world","Preparing textures","Sunlight","Static lighting",
    "Sprite lighting","Bounce lighting","Ambient lighting","Sector light flow"};
// Each stage's share of the bar, in milliseconds: the last load's times,
// first these (Freedoom's E1M1 with the default options).
double stageMs[stageCount]={60,400,100,2000,30,120,250,25};
constexpr uint64_t showAfter=250'000'000,redrawEvery=33'000'000; // ns

struct Pass {
    LoadingShow show=nullptr;
    std::thread::id thread;
    bool planned[stageCount]={},level=false;
    int stage=-1,loop=0,shownStage=-1;
    const char *label=nullptr;
    float part=0,loopPart=0;
    uint64_t start=0,stageStart=0,shown=0;
    double took[stageCount]={};
} pass;

float fraction() {
    double done=0,total=0;
    for(int s=0;s<stageCount;++s)if(pass.planned[s]) {
        total+=stageMs[s];
        if(s<pass.stage)done+=stageMs[s];
        else if(s==pass.stage)done+=stageMs[s]*pass.part;
    }
    return total>0?(float)std::clamp(done/total,0.0,1.0):0;
}
void endStage(uint64_t now) {
    if(pass.stage>=0)pass.took[pass.stage]+=(now-pass.stageStart)/1e6;
}
} // namespace

void planLoading(LoadStage stage,bool planned) {pass.planned[(int)stage]=planned;}
void loadingStage(LoadStage stage) {
    if(!pass.show)return;
    uint64_t now=SDL_GetTicksNS();
    endStage(now);
    pass.label=(int)stage<(int)LoadStage::Sun?stageNames[(int)stage]:"Preparing lighting";
    fprintf(stderr,"3D loading stage: %s\n",pass.label);fflush(stderr);
    pass.stage=(int)stage;pass.part=0;pass.loop=0;pass.loopPart=0;pass.stageStart=now;
    if(stage==LoadStage::Map)pass.level=true;
    if(stage==LoadStage::Map||stage==LoadStage::Textures)loadingTick();
}
void loadingLabel(const char *label) {
    if(!loadingOnThisThread())return;
    pass.label=label;
}
bool loadingOnThisThread() {return pass.show&&std::this_thread::get_id()==pass.thread;}
static const char *title() {
    const char *level=pass.level?HU_LevelTitle():nullptr;
    return level&&*level?level:pass.level?"Loading":"Updating the lighting";
}
void loadingTick(float part) {
    if(!loadingOnThisThread())return;
    uint64_t now=SDL_GetTicksNS();
    // Loops that know how far they are say so: a stage's first loop fills
    // half of it, the next half the rest, and so on. Otherwise the stage
    // fills toward 90% over the time it took last.
    if(part>=0) {
        if(part<pass.loopPart)++pass.loop;
        pass.loopPart=part;
        float base=1-std::ldexp(1.0f,-pass.loop);
        pass.part=std::max(pass.part,base+(1-base)*0.5f*std::min(part,1.0f));
    }
    else if(pass.stage>=0)pass.part=std::max(pass.part,(float)std::min(0.9,(now-pass.stageStart)/1e6/stageMs[pass.stage]));
    // Each bake stage gets one standalone update at most, as before.
    // The actual calculation keeps its normal worker progress updates.
    if(part<0&&pass.stage>=(int)LoadStage::Sun&&pass.shownStage==pass.stage)return;
    if(now-pass.start<showAfter||now-pass.shown<redrawEvery)return;
    pass.shown=now;pass.shownStage=pass.stage;
    pass.show(title(),pass.label?pass.label:"",fraction());
}
void runLoading(int width,int height,LoadingShow show) {
    pass=Pass{};
    pass.show=show;pass.thread=std::this_thread::get_id();pass.start=SDL_GetTicksNS();
    prepareFrame(width,height);
    uint64_t now=SDL_GetTicksNS();
    endStage(now);
    endBakeCacheSession(pass.start,pass.took[(int)LoadStage::Map]);
    // A full bar to finish, if the screen was up, so it does not vanish part way.
    if(pass.shown)pass.show(title(),"",1);
    std::string times;
    for(int s=0;s<stageCount;++s)if(pass.took[s]>0) {
        stageMs[s]=std::max(1.0,pass.took[s]);
        char entry[64];snprintf(entry,sizeof(entry),"%s%s %.0f ms",times.empty()?"":", ",stageNames[s],pass.took[s]);
        times+=entry;
    }
    if(!times.empty())fprintf(stderr,"3D loading: %s; %.0f ms in all.\n",times.c_str(),(now-pass.start)/1e6);
    pass.show=nullptr;
}
} // namespace doom3d
