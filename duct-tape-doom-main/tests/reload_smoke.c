/* Real-engine regression: original DOOM2, actual death/Use and save/load paths.
   Run in an isolated working directory; never use a user's profile. */
#include "doomdef.h"
#include "doomstat.h"
#include "d_main.h"
#include "g_game.h"
#include "p_local.h"
#include "p_spec.h"
#include "m_argv.h"
#include "d_event.h"
#include "i_system.h"
#include "../platform/i_render3d.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int key_use;
extern unsigned r_levelserial;
extern void I_RunEngine(void (*entry)(void));
extern int I_TestBakeCacheStorage(void);
extern int I_TestLightingModelTransition(int frame);
extern int I_TestRelightRunning(void);
extern void I_TestNativeCrash(void);
static int storageMode,crashMode,autoLoadMode,startupMode,relightQuitMode;
static int lightingModelsMode;
extern int g_death_load_last_save;
static int stage,begin,initialSerial,frames,cycles,goal=10,saveMode;
static void effects(void) {
    mobj_t *mo=players[0].mo;
    P_SpawnBlood(mo->x,mo->y,mo->z+32*FRACUNIT,10);
    P_SpawnPuff(mo->x+20*FRACUNIT,mo->y,mo->z+32*FRACUNIT);
    mobj_t *rocket=P_SpawnMobj(mo->x+48*FRACUNIT,mo->y,mo->z+32*FRACUNIT,MT_ROCKET);
    rocket->target=mo;P_ExplodeMissile(rocket);
    mobj_t *plasma=P_SpawnMobj(mo->x+48*FRACUNIT,mo->y+8*FRACUNIT,mo->z+32*FRACUNIT,MT_PLASMA);
    plasma->target=mo;P_ExplodeMissile(plasma);
    // Exercise moving-sector relighting; open a normal local door.
    for(int i=0;i<numlines;++i)if(lines[i].special==1&&lines[i].backsector) {
        EV_VerticalDoor(&lines[i],mo);break;
    }
}
void I_TestFrame(void) {
    if(gamestate!=GS_LEVEL||!players[0].mo)return;
    if(++frames>30000){fprintf(stderr,"RELOAD FAIL timeout stage %d\n",stage);exit(3);}
    if(lightingModelsMode) {
        if(frames>=4&&I_TestLightingModelTransition(frames-4)) {
            fprintf(stderr,"LIGHTING MODE TRANSITIONS PASS\n");
            I_Quit();
        }
        return;
    }
    if(relightQuitMode) {
        // Simulate a nearby sector moving even on maps without a type-1 door.
        if(frames==4)players[0].mo->subsector->sector->ceilingheight+=8*FRACUNIT;
        if(frames>=4&&I_TestRelightRunning()) {
            fprintf(stderr,"RELIGHT QUIT: active worker\n");
            I_Quit();
        }
        if(frames>300){fprintf(stderr,"RELIGHT QUIT FAIL: no active worker\n");exit(6);}
        return;
    }
    if(startupMode) {
        if(frames>=4){fprintf(stderr,"STARTUP PASS 4 frames; normal quit\n");I_Quit();}
        return;
    }
    if(stage==0) {
        begin=gametic;initialSerial=r_levelserial;stage=1;
        players[0].cheats|=CF_GODMODE;
    }
    if(stage==1&&gametic>begin+12) {
        if(crashMode)I_TestNativeCrash();
        if(cycles==0)I_Render3DScreenshotTo("initial.png");
        if(storageMode){if(!I_TestBakeCacheStorage())exit(5);I_Quit();}
        effects();begin=gametic;stage=2;
        if((saveMode||autoLoadMode)&&cycles==0){
            players[0].armorpoints=77;
            G_SaveGame(7,"RELOAD REGRESSION");
        }
    }
    if(stage==2&&gametic>begin+35) {
        if(saveMode) {
            FILE *f=fopen("doomsav7.dsg","rb");if(!f){fprintf(stderr,"RELOAD FAIL no save\n");exit(4);}fclose(f);
            for(int i=0;i<numlines;++i)if(lines[i].special==1&&lines[i].backsector){EV_VerticalDoor(&lines[i],players[0].mo);break;}
            stage=6;
        } else {
            if(autoLoadMode){
                int enabled=g_death_load_last_save;
                if(!enabled){fprintf(stderr,"AUTOLOAD FAIL option not applied\n");exit(8);}
                g_death_load_last_save=0;
                if(G_TryDeathLoadLastSave())exit(9);
                g_death_load_last_save=enabled;
                netgame=true;if(G_TryDeathLoadLastSave())exit(9);netgame=false;
                demoplayback=true;if(G_TryDeathLoadLastSave())exit(9);demoplayback=false;
                demorecording=true;if(G_TryDeathLoadLastSave())exit(9);demorecording=false;
                if(rename("doomsav7.dsg","recovery-hidden.dsg")!=0)exit(9);
                if(G_TryDeathLoadLastSave())exit(9);
                if(rename("recovery-hidden.dsg","doomsav7.dsg")!=0)exit(9);
                players[0].armorpoints=0;
            }
            players[0].cheats&=~CF_GODMODE;players[0].powers[pw_invulnerability]=0;
            P_DamageMobj(players[0].mo,NULL,NULL,10000);stage=3;
        }
        begin=gametic;
    }
    if(stage==6) {
        if(I_TestRelightRunning()) {
            fprintf(stderr,"RELOAD ACTIVE RELIGHT before save load\n");G_LoadGame("doomsav7.dsg");stage=4;
        } else if(gametic>begin+30) {
            fprintf(stderr,"RELOAD FAIL no active relight before save load\n");exit(6);
        }
    }
    if(stage==3&&gametic>begin+15) {
        event_t key={ev_keydown,key_use,0,0};D_PostEvent(&key);stage=4;
    }
    if(stage==4&&r_levelserial!=initialSerial) {
        event_t key={ev_keyup,key_use,0,0};D_PostEvent(&key);stage=5;begin=gametic;
    }
    if(stage==5&&gametic>begin+15) {
        if(autoLoadMode && (players[0].armorpoints!=77 || players[0].playerstate!=PST_LIVE)){
            fprintf(stderr,"AUTOLOAD FAIL save was not restored\n");exit(7);
        }
        if(cycles==0)I_Render3DScreenshotTo("reloaded.png");
        ++cycles;fprintf(stderr,"RELOAD CYCLE %s %d/%d\n",saveMode?"save":"restart",cycles,goal);

        if(cycles>=goal){fprintf(stderr,"RELOAD PASS %s %d cycles\n",saveMode?"save":"restart",cycles);I_Quit();}
        stage=0;
    }
}
int main(int argc,char **argv) {
    if(argc<5)return 2;
    autoLoadMode=!strcmp(argv[3],"autoload");
    startupMode=!strcmp(argv[3],"startup");
    lightingModelsMode=!strcmp(argv[3],"lighting-models");
    relightQuitMode=!strcmp(argv[3],"relight-quit");
    crashMode=!strcmp(argv[3],"crash");storageMode=!strcmp(argv[3],"storage");saveMode=!strcmp(argv[3],"save");goal=atoi(argv[4]);if(goal<1||goal>50)return 2;
    char *args[128]={argv[0],"-iwad",argv[1],"-warp","1","-skill","1","-graphics",argv[2],
        "-config","reload-doom.cfg","-nosound","-nomusic","-width","960","-height","540"};
    int count=17;
    for(int i=5;i<argc&&count<127;++i)args[count++]=argv[i];
    myargc=count;myargv=args;I_RunEngine(D_DoomMain);return 0;
}

