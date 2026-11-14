// Fixed-tic native replay; same runner/input sequence as the headless driver.
#include <stdio.h>
#include "doomdef.h"
#include "doomstat.h"
#include "d_main.h"
#include "m_argv.h"
#include "i_video.h"
#include "../platform/i_render3d.h"
#include "../platform/sprite_visibility.h"
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
extern void MP_BenchmarkRun(void);
extern void (*MP_BenchmarkRender)(double*);
extern void (*MP_BenchmarkDone)(void);
extern void D_Display(void);
extern void D_ProcessEvents(void);
extern void I_RunEngine(void(*entry)(void));
static int running;
static int menuTest,menuObserved,frameIndex;
static SDL_AtomicInt menuInjected;
static Uint64 menuPosted;
static FILE *visibility;
static const char *imagePath;
static int imageFrame=35;
static int filterFixture;
extern void I_TestSpriteFilterScene(int frame);
static void escapeEvent(void){
    SDL_Event e;SDL_zero(e);e.type=SDL_EVENT_USER;e.user.code=0x48464d50;SDL_PushEvent(&e);
}
static Uint32 SDLCALL menuTimer(void *unused,SDL_TimerID id,Uint32 interval){
    (void)unused;(void)id;(void)interval;menuPosted=SDL_GetTicksNS();SDL_SetAtomicInt(&menuInjected,1);escapeEvent();return 0;
}
static void finish(void){
    if(visibility){fclose(visibility);visibility=NULL;}
    if(menuTest&&!menuObserved)fprintf(stderr,"BENCH_MENU FAIL Escape was not observed\n");
    I_ShutdownGraphics();
}
static void render(double *output){
    int currentFrame=frameIndex++;
    if(filterFixture)I_TestSpriteFilterScene(currentFrame);
    if(menuTest){
        I_StartTic();D_ProcessEvents();
        if(SDL_GetAtomicInt(&menuInjected)&&menuactive&&!menuObserved){
            double ms=(SDL_GetTicksNS()-menuPosted)/1e6;menuObserved=1;
            printf("BENCH_MENU %s latency_ms %.3f target_ms 250 (tagged SDL event to Escape; test-driver keyboard isolation retained)\n",ms<=250?"PASS":"FAIL",ms);fflush(stdout);
            escapeEvent();I_StartTic();D_ProcessEvents();
        }
        if(currentFrame==5)SDL_AddTimer(20,menuTimer,NULL);
    }
    if(imagePath&&currentFrame==imageFrame)I_Render3DScreenshotTo(imagePath);
    D_Display();
    if(visibility){
        SpriteVisibilityStats s=I_SpriteVisibilityStats();
        fprintf(visibility,"%d,%llu,%llu,%llu,%llu,%llu",currentFrame,(unsigned long long)s.examined,(unsigned long long)s.rejected,(unsigned long long)s.retained,(unsigned long long)s.eligible,(unsigned long long)s.filter_ns);
        for(int i=0;i<SVF_REASONS;++i)fprintf(visibility,",%llu",(unsigned long long)s.fallback[i]);
        fputc('\n',visibility);
    }
    const I_Render3DFrameProfile *frame=I_Render3DLastFrame();
    if(frame){output[0]=frame->cpuMs;output[1]=frame->gpuMs;output[2]=frame->spritesMs;}
}
void I_TestFrame(void){
    if(running||gamestate!=GS_LEVEL||!players[0].mo)return;
    running=1;
    menuTest=M_CheckParm("-bench-menu-test")!=0;
    filterFixture=M_CheckParm("-bench-filter-fixture")!=0;
    int at=M_CheckParm("-bench-image");if(at&&at+1<myargc)imagePath=myargv[at+1];
    at=M_CheckParm("-bench-image-frame");if(at&&at+1<myargc)imageFrame=atoi(myargv[at+1]);
    at=M_CheckParm("-monsterbench");
    if(at&&at+1<myargc){char path[4096];snprintf(path,sizeof(path),"%s/visibility.csv",myargv[at+1]);visibility=fopen(path,"w");
        if(visibility)fputs("frame,examined,rejected,retained,eligible,filter_ns,disabled,secondary,other,voxel,pose,wound,liquid,artwork,invalid\n",visibility);
    }
    if(M_CheckParm("-bench-fenced"))I_Render3DProfile(1,0);
    MP_BenchmarkRender=render;MP_BenchmarkDone=finish;MP_BenchmarkRun();
}
int main(int argc,char **argv){myargc=argc;myargv=argv;I_RunEngine(D_DoomMain);return 0;}
