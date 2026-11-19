// Real Doom simulation + stock sprite patches, isolated configs and no GPU.
// Usage: doom-critical-smoke <DOOM2.WAD> <ragdoll-arena.wad> <output-dir>
#include "../platform/scene3d/internal.h"
#include "../platform/scene3d/modern_hud.h"
#include "../platform/death_joints.h"
#include "../platform/monster_regions_stock.h"
#include "../platform/monster_regions.h"
#include "../platform/settings_panel.h"
extern "C" {
#include "p_local.h"
#include "d_main.h"
#include "i_system.h"
#include "m_argv.h"
extern int p_player_surface_damage,p_monster_surface_damage;
}
#include <SDL3/SDL_main.h>
#include <cstdlib>
namespace doom3d { int criticalThing(mobj_t*,critical_hit_t*); }
using namespace doom3d;
static std::string output;
static bool visual=false;
static bool visualEnhanced=false;
static bool visualSevere=false;
static bool visualFeedback=false;
static bool visualSurvival=false;
static bool solidMode=false;
static bool eyeMode=false;
static bool elementalMode=false,visualElemental=false,elemental2Mode=false,visualElemental2=false;
static bool visualEyes=false,e1m8Mode=false,e1m8Exit=false,e1m8Snap=false;
static std::string visualCapture;
static int checks=0;
static void check(bool ok,const char *message) {
    if(!ok)I_Error((char*)"Critical smoke: %s",message);
    ++checks;
}
static fixed_t fx(float n){return (fixed_t)std::lround(n*FRACUNIT);}
static Image &patch(int lump) {
    int key=-1-lump;auto found=images.find(key);if(found!=images.end())return found->second;
    const byte *data=(const byte*)W_CacheLumpNum(lump,PU_CACHE);
    const patch_t *p=(const patch_t*)data;Image im;
    im.width=SHORT(p->width);im.height=SHORT(p->height);im.left=SHORT(p->leftoffset);im.top=SHORT(p->topoffset);
    im.pixels.resize((size_t)im.width*im.height*2);
    for(int x=0;x<im.width;++x) {
        size_t pos=(uint32_t)LONG(p->columnofs[x]);int previous=-1;
        while(data[pos]!=255) {
            int top=data[pos];if(top<=previous)top+=previous;previous=top;
            for(int y=0;y<data[pos+1];++y)if(top+y>=0&&top+y<im.height) {
                size_t at=((size_t)(top+y)*im.width+x)*2;im.pixels[at]=data[pos+3+y];im.pixels[at+1]=255;
            }
            pos+=data[pos+1]+4;
        }
    }
    return images.emplace(key,std::move(im)).first->second;
}
static bool solid(const Image &im,int x,int y){return x>=0&&y>=0&&x<im.width&&y<im.height&&im.pixels[((size_t)y*im.width+x)*2+1]!=0;}
static int beginCalls=0;
static int begin(mobj_t *s,fixed_t x,fixed_t y,fixed_t z,fixed_t dx,fixed_t dy,fixed_t slope,fixed_t range) {
    ++beginCalls;return criticalBegin(s,x,y,z,dx,dy,slope,range);
}
static void shoot(mobj_t *shooter,float x,float y,float z,bool bullet=true) {
    angle_t angle=R_PointToAngle2(shooter->x,shooter->y,fx(x),fx(y));
    float distance=std::hypot(x-units(shooter->x),y-units(shooter->y));
    fixed_t slope=fx((z-units(shooter->player->viewz))/distance);
    la_freeaim=true;
    if(bullet)P_BulletAttack(shooter,angle,2048*FRACUNIT,slope,5);
    else P_LineAttack(shooter,angle,2048*FRACUNIT,slope,5);
}
static void run() {
    if(solidMode)p_player_surface_damage=p_monster_surface_damage=0;
    auto *shooter=players[consoleplayer].mo;
    check(P_TeleportMove(shooter,0,fx(-32))!=0,"fixture player placement");
    shooter->angle=ANG90;shooter->z=shooter->floorz;players[consoleplayer].viewz=shooter->z+41*FRACUNIT;
    settings.accelerated=1;settings.criticalParts=1;settings.wounds=0;settings.hitFlinch=0;
    settings.voxelModels=0;settings.blood=0;
    settings.killShotsCarry=0;settings.shootableLights=0;
    P_CriticalBegin=begin;P_CriticalThing=criticalThing;
    prepareCriticalSurface=criticalSurface;criticalVoxelModel=nullptr;
    R_BloodHit=bloodHit;
    int headFrames=0,bodyFrames=0,fallbackFrames=0;
    for(auto type:{MT_POSSESSED,MT_SHOTGUY,MT_CHAINGUY,MT_WOLFSS}) {
        auto *monster=P_SpawnMobj(0,fx(128),ONFLOORZ,type);monster->angle=ANG270;
        auto &info=mobjinfo[type];
        for(auto state:{info.spawnstate,info.seestate,info.missilestate,info.painstate}) {
            if(!state)continue;
            for(unsigned rotation=0;rotation<8;++rotation) {
                P_SetMobjState(monster,(statenum_t)state);monster->health=10000;
                auto &sf=sprites[monster->sprite].spriteframes[monster->frame&FF_FRAMEMASK];
                auto &im=patch(firstspritelump+sf.lump[rotation]);
                // Cache all angles for the fallback geometry's frame selection.
                for(int r=0;r<8;++r)patch(firstspritelump+sf.lump[r]);
                bool flip=sf.flip[rotation];
                auto capture=[&](){forgetWounds(monster);woundView(monster,shooter,im,flip,0,128,0,(float)im.top,(float)(im.top-im.height),1,0);};
                doom_joints::Head h{};bool has=doom_joints::findHead(im.pixels.data(),im.width,im.height,im.left,im.top,h);
                int hx=(int)std::floor(h.middle+im.left),hy=-1;
                if(has)for(int y=0;y<im.height;++y)if(im.top-y-0.5f>=h.neck&&solid(im,hx,y)){hy=y;break;}
                if(hy>=0) {
                    capture();int before=monster->health;
                    float across=flip?im.width-im.left-hx-0.5f:hx+0.5f-im.left;
                    shoot(shooter,across,128,im.top-hy-0.5f);
                    check(before-monster->health==10,"real sprite head pixel must deal exactly double damage");++headFrames;
                } else ++fallbackFrames;
                P_SetMobjState(monster,(statenum_t)state);monster->health=10000;capture();
                int bx=-1,by=std::min(im.height-1,im.top-24);
                for(int x=0;x<im.width;++x)if(solid(im,x,by)){bx=x;break;}
                if(bx>=0) {
                    float across=flip?im.width-im.left-bx-0.5f:bx+0.5f-im.left;
                    shoot(shooter,across,128,im.top-by-0.5f);
                    check(monster->health==9995,"real sprite body pixel must keep normal damage");++bodyFrames;
                }
            }
        }
        P_RemoveMobj(monster);forgetWounds(monster);
    }
    check(headFrames>=64&&bodyFrames>=64,"real walking, attack and pain frames must be covered");
    std::printf("Stock sprite coverage: %d head, %d body, %d safe head-detection fallbacks\n",headFrames,bodyFrames,fallbackFrames);

    // Use a synthetic narrow head for reproducible collision edge cases,
    // while retaining the actual game's actor, traversal and damage code.
    auto *near=P_SpawnMobj(0,fx(128),ONFLOORZ,MT_POSSESSED);
    auto *far=P_SpawnMobj(0,fx(152),ONFLOORZ,MT_POSSESSED);
    Image art;art.width=48;art.height=72;art.left=24;art.top=72;art.pixels.resize(48*72*2);
    for(int y=0;y<72;++y)for(int x=0;x<48;++x)if(y>=14||(x>=20&&x<28))art.pixels[((size_t)y*48+x)*2+1]=255;
    auto reset=[&](float farZ=0) {
        near->health=far->health=1000;near->flags|=MF_SHOOTABLE;far->flags|=MF_SHOOTABLE;
        forgetWounds(near);forgetWounds(far);near->z=0;far->z=fx(farZ);
        woundView(near,shooter,art,false,0,128,0,72,0,1,0);
        woundView(far,shooter,art,false,0,152,farZ,72+farZ,farZ,1,0);
    };
    reset();shoot(shooter,0.5f,128,67.5f);
    check(near->health==990&&far->health==1000,"visible head above vanilla height hits before farther target");
    reset();shoot(shooter,21.5f,128,35.5f);
    check(near->health==995,"visible shoulders beyond vanilla radius are candidates");
    reset(32);shoot(shooter,12.5f,128,67.5f);
    check(near->health==1000&&far->health==995,"transparent head gap continues to the opaque target behind");
    reset();shoot(shooter,0.5f,128,35.5f);
    check(near->health==995&&far->health==1000,"near opaque body stops the bullet");
    reset();far->flags&=~MF_SHOOTABLE;near->z=fx(280);forgetWounds(near);
    woundView(near,shooter,art,false,0,128,280,352,280,1,0);
    shoot(shooter,0.5f,128,347.5f);
    check(near->health==1000,"ceiling occludes an elevated sprite");
    near->z=fx(-150);forgetWounds(near);
    woundView(near,shooter,art,false,0,128,-150,-78,-150,1,0);
    shoot(shooter,0.5f,128,-82.5f);
    check(near->health==1000,"floor occludes a lower sprite");
    near->z=0;
    reset();far->flags&=~MF_SHOOTABLE;
    P_UnsetThingPosition(near);near->y=fx(220);P_SetThingPosition(near);forgetWounds(near);
    woundView(near,shooter,art,false,0,220,0,72,0,1,0);
    shoot(shooter,0.5f,220,35.5f);
    check(near->health==1000,"wall occludes a sprite behind it");
    P_UnsetThingPosition(near);near->y=fx(128);P_SetThingPosition(near);
    reset();far->flags&=~MF_SHOOTABLE;
    beginCalls=0;shoot(shooter,0.5f,128,35.5f,false);
    check(beginCalls==0&&near->health==995,"ordinary melee/BFG/enemy line attacks never opt in");
    for(int mode=0;mode<3;++mode) {
        reset();far->flags&=~MF_SHOOTABLE;beginCalls=0;
        if(mode==0)netgame=true;else if(mode==1)demoplayback=true;else demorecording=true;
        shoot(shooter,0.5f,128,35.5f);
        netgame=demoplayback=demorecording=false;
        check(beginCalls==0&&near->health==995,"network and demo modes bypass sprite damage");
    }
    reset();far->flags&=~MF_SHOOTABLE;settings.criticalParts=0;
    shoot(shooter,0.5f,128,35.5f);check(near->health==995,"disabled setting retains ordinary damage");
    settings.criticalParts=1;
    // Validate config round-trip through the same table that creates F4.
    Settings saved;check(saved.criticalParts==0,"critical parts defaults off");readOption(saved,"critical_parts",1);
    check(saved.criticalParts==1,"critical parts loads from graphics config");
    FILE *file=std::tmpfile();check(file!=nullptr,"temporary option output");writeOptions(file,saved);std::rewind(file);
    char line[256];bool persisted=false;while(std::fgets(line,sizeof(line),file))if(std::strstr(line,"critical_parts 1"))persisted=true;
    std::fclose(file);check(persisted,"critical parts persists through F4 option serialization");
    reset();far->flags&=~MF_SHOOTABLE;near->health=10;settings.wounds=1;
    shoot(shooter,0.5f,128,67.5f);
    check(near->health<=0,"lethal critical damage is applied once through normal death handling");
    WoundSpot spot[2];check(woundSpots(near,spot,2)>0,"lethal headshot captures its wound before the death state");
    P_RemoveMobj(near);P_RemoveMobj(far);
    std::printf("PASS %d critical hit integration checks\n",checks);std::fflush(stdout);
}
#include "weakpoints_integration.inc"
#include "feedback_integration.inc"
#include "survival_integration.inc"
#include "survival_visual.inc"
static void visualFrame() {
    static std::vector<mobj_t*> victims;
    static int step=0,lastTic=-1,wait=0;
    if(leveltime==lastTic)return;lastTic=leveltime;
    if(R_BeforeTic)R_BeforeTic();
    auto *shooter=players[consoleplayer].mo;
    if(step==0) {
        check(I_Render3DIsAccelerated()!=0,"visual renderer must be accelerated");
        check(P_TeleportMove(shooter,0,fx(-32))!=0,"visual player placement");
        shooter->angle=ANG90;shooter->z=shooter->floorz;players[consoleplayer].viewz=shooter->z+41*FRACUNIT;
        int index=0;
        for(auto type:{MT_POSSESSED,MT_SHOTGUY,MT_CHAINGUY,MT_WOLFSS}) {
            auto *m=P_SpawnMobj(fx((float)(-102+index++*68)),fx(128),ONFLOORZ,type);
            m->angle=ANG270;m->tics=-1;m->health=1000;victims.push_back(m);
        }
        wait=leveltime;step=1;return;
    }
    // Keep the fixture stationary after its pain state, while preserving
    // ordinary damage and wound creation on the actual shot.
    for(auto *m:victims){m->momx=m->momy=m->momz=0;m->tics=-1;}
    if(step==1&&leveltime-wait>=4) {
        visualCapture=output+"/critical-before.png";I_Render3DScreenshotTo(visualCapture.c_str());step=2;return;
    }
    if(step==2) {
        for(auto *m:victims) {
            const auto &sf=sprites[m->sprite].spriteframes[m->frame&FF_FRAMEMASK];
            angle_t bearing=(angle_t)(int64_t)(std::atan2(units(m->y-shooter->y),units(m->x-shooter->x))*(4294967296.0/(2*doomPi)));
            unsigned rotation=sf.rotate?(bearing-m->angle+(unsigned)(ANG45/2)*9)>>29:0;
            const Image &im=lumpImage(firstspritelump+sf.lump[rotation],false);
            doom_joints::Head head{};
            check(doom_joints::findHead(im.pixels.data(),im.width,im.height,im.left,im.top,head),"visual fixture head detection");
            int x=(int)std::floor(head.middle+im.left),y=0;
            while(y<im.height&&!solid(im,x,y))++y;
            check(y<im.height,"visual fixture opaque head");
            // Compute the ray for the displayed camera-facing sprite plane.
            float across=sf.flip[rotation]?im.width-im.left-x-0.5f:x+0.5f-im.left;
            int before=m->health;
            shoot(shooter,units(m->x)+across,units(m->y),im.top-y-0.5f);
            check(before-m->health==10,"visual headshot damage");
            m->momx=m->momy=m->momz=0;m->tics=-1;
        }
        visualCapture=output+"/critical-after.png";I_Render3DScreenshotTo(visualCapture.c_str());step=3;return;
    }
    if(step==3) {
        for(const char *name:{"critical-before.png","critical-after.png"}) {
            std::string path=output+"/"+name;FILE *f=std::fopen(path.c_str(),"rb");
            check(f!=nullptr,"visual screenshot exists");std::fclose(f);
        }
        std::puts("PASS four real sprite headshots and before/after GPU captures");I_Quit();
    }
}
#include "settings_persistence.inc"
#include "eye_injury_smoke.inc"
#include "elemental_smoke.inc"
#include "elemental_visual.inc"
#include "elemental2_smoke.inc"
#include "elemental2_visual.inc"
#include "eye_injury_visual.inc"
#include "e1m8_regression.inc"
extern "C" int I_TestSevereWallMarks();
extern "C" int I_TestSeverePools();
static void severeVisualFrame() {
    static int shot=-1;
    static bool captured=false;
    static int wallShot=-1;
    static bool wallCaptured=false;
    static bool falloutCaptured=false;
    static std::string falloutPath;
    auto *shooter=players[consoleplayer].mo;
    if(shot<0) {
        if(levelSerial!=r_levelserial||floors.empty())return;
        check(I_Render3DIsAccelerated()!=0,"severe blood visual requires GPU renderer");
        for(auto type:{MT_TROOP,MT_SERGEANT,MT_VILE,MT_FATSO,MT_BRUISER,MT_KNIGHT,MT_HEAD}) {
            mobj_t actor{};
            actor.type=type;
            const int expected=type==MT_HEAD?1:(type==MT_BRUISER||type==MT_KNIGHT)?2:0;
            check(monsterBloodTint(&actor)==expected,"stock severe monster uses its expected blood tint");
        }
        check(P_TeleportMove(shooter,0,fx(-64))!=0,"severe blood camera placement");
        shooter->angle=ANG90;
        shooter->z=shooter->floorz;
        players[consoleplayer].viewz=shooter->z+41*FRACUNIT;
        int n=0;
        for(auto type:{MT_TROOP,MT_KNIGHT,MT_HEAD}) {
            auto *target=P_SpawnMobj(fx(-92+92*n++),fx(128),ONFLOORZ,type);
            target->angle=ANG270;
            P_WeakpointBeginShot(shooter,wp_pistol);
            wp_damage_event_t e{};
            e.target=target;e.source=e.inflictor=shooter;e.kind=WP_BULLET;
            e.baseDamage=1;
            e.impact={WP_BODY,100,
                      units(target->x),units(target->y-target->radius),
                      units(target->z)+units(target->height)*.80f};
            target->health=target->info->spawnhealth/4+1;
            P_WeakpointApply(&e);P_WeakpointEndShot();
            check(target->health>0&&int64_t(target->health)*4<=target->info->spawnhealth,
                  "ordinary 1 HP body hit crosses severe health threshold");
            target->momx=target->momy=target->momz=0;
            target->tics=-1;
        }
        combat_notice_t notices[3]{};
        check(P_HudNotices(notices)==3&&notices[0].kind==HUD_SEVERE_CRITICAL,
              "three severe hits reached the HUD queue");
        shot=leveltime;
    } else if(!captured&&leveltime-shot>=5) {
        visualCapture=output+"/severe-bloodspill.png";
        I_Render3DScreenshotTo(visualCapture.c_str());
        captured=true;
    } else if(captured&&!falloutCaptured&&leveltime-shot>=26) {
        check(I_TestSeverePools()>0&&I_TestSeverePools()<=32,"ballistic fallout collects in bounded pools");
        falloutPath=output+"/severe-fallout.png";
        I_Render3DScreenshotTo(falloutPath.c_str());
        falloutCaptured=true;
    } else if(falloutCaptured&&leveltime-shot>=30&&wallShot<0) {
        FILE *f=std::fopen(visualCapture.c_str(),"rb");
        check(f!=nullptr,"three-tint rupture screenshot saved");
        std::fclose(f);
        f=std::fopen(falloutPath.c_str(),"rb");
        check(f!=nullptr,"fallout pool screenshot saved");
        std::fclose(f);
        check(P_TeleportMove(shooter,fx(342),fx(135))!=0,"severe wall camera placement");
        shooter->angle=0;shooter->z=shooter->floorz;
        players[consoleplayer].viewz=shooter->z+41*FRACUNIT;
        wp_severe_burst_t wall{};
        wall.impact.x=428;wall.impact.y=128;wall.impact.z=units(shooter->z)+42;
        wall.directionX=-1;wall.seed=123;
        severeBloodBurst(&wall);
        wallShot=leveltime;
    } else if(wallShot>=0&&!wallCaptured&&leveltime-wallShot>=11) {
        check(I_TestSevereWallMarks()>0&&I_TestSevereWallMarks()<=4,
              "pressurized streams reach the solid east wall and deposit bounded marks");
        visualCapture=output+"/severe-wall-impact.png";
        I_Render3DScreenshotTo(visualCapture.c_str());
        wallCaptured=true;
    } else if(wallCaptured&&leveltime-wallShot>=14) {
        FILE *f=std::fopen(visualCapture.c_str(),"rb");
        check(f!=nullptr,"wall impact screenshot saved");
        std::fclose(f);
        std::puts("PASS severe critical damage, monster tints, jets, wall marks and fallout pools");
        I_Quit();
    }
}
extern "C" void I_TestFrame() {
    if(gamestate!=GS_LEVEL||!players[consoleplayer].mo)return;
    if(e1m8Mode){e1m8Frame();return;}
    if(!settingsCase.empty()){settingsPersistenceFrame();return;}
    if(eyeMode){eyeIntegration();I_Quit();return;}
    if(elemental2Mode){elemental2Integration();I_Quit();return;}
    if(visualElemental2){elemental2VisualFrame();return;}
    if(elementalMode){elementalIntegration();I_Quit();return;}
    if(visualElemental){elementalVisualFrame();return;}
    if(visualEyes){eyeVisualFrame();return;}
    if(visualSurvival){survivalVisualFrame();return;}
    if(visualFeedback){feedbackVisualFrame();return;}
    if(visualEnhanced){enhancedVisualFrame();return;}
    if(visualSevere){severeVisualFrame();return;}
    if(visual){visualFrame();return;}
    run();featureSwitchConfig();enhancedCoverage();cachedHitBenchmark();balanceHitCounts();realProjectiles();localizedRockets();realBFGAndMovement();realShotgunDischarges();realInjuriesAndSave();feedbackAndStatusIntegration();survivalIntegration();std::printf("PASS %d total engine integration checks\n",checks);I_Quit();
}
int main(int argc,char **argv) {
    if(argc!=4&&argc!=5)return 2;output=argv[3];solidMode=argc==5&&!std::strcmp(argv[4],"--solid");visualEnhanced=argc==5&&!std::strcmp(argv[4],"--visual-enhanced");
    visualFeedback=argc==5&&!std::strcmp(argv[4],"--visual-feedback");
    visualSevere=argc==5&&!std::strcmp(argv[4],"--visual-severe");
    visualSurvival=argc==5&&!std::strcmp(argv[4],"--visual-survival");
    visualEyes=argc==5&&!std::strcmp(argv[4],"--visual-eye-injuries");
    visualElemental=argc==5&&!std::strcmp(argv[4],"--visual-elemental");
    elemental2Mode=argc==5&&!std::strcmp(argv[4],"--elemental2");
    visualElemental2=argc==5&&!std::strcmp(argv[4],"--visual-elemental2");
    e1m8Mode=argc==5&&(std::string(argv[4])=="--e1m8-floor"||std::string(argv[4])=="--e1m8-exit"||std::string(argv[4])=="--e1m8-exit-snap");e1m8Exit=e1m8Mode&&std::string(argv[4]).find("--e1m8-exit")==0;e1m8Snap=e1m8Mode&&std::string(argv[4])=="--e1m8-exit-snap";
    visual=e1m8Mode||visualElemental2||visualElemental||visualEyes||visualSurvival||visualFeedback||visualEnhanced||visualSevere||(argc==5&&!std::strcmp(argv[4],"--visual"));
    elementalMode=argc==5&&!std::strcmp(argv[4],"--elemental");
    eyeMode=argc==5&&!std::strcmp(argv[4],"--eye-injuries");
    if(argc==5&&std::string(argv[4]).find("--settings-")==0)settingsCase=argv[4];
    std::string config=output+"/doom.cfg",graphics=output+"/graphics.cfg";
    FILE *f=nullptr;
    if(e1m8Mode){std::ifstream input(argv[2],std::ios::binary);std::ofstream copy(graphics,std::ios::binary);copy<<input.rdbuf();}
    else if(settingsCase.empty()) {
    f=std::fopen(graphics.c_str(),"w");if(!f)return 2;
    std::fputs(visual?"accelerated 1\ncritical_parts 1\nwounds 1\nvoxel_models 0\nhit_flinch 0\neye_adaptation 0\n":"accelerated 0\n",f);std::fclose(f);
    if(visualEnhanced) {f=std::fopen(graphics.c_str(),"a");std::fputs("enhanced_weak_points 1\nmonster_weapon_vulnerabilities 1\nlocalized_injury_effects 1\n",f);std::fclose(f);}
    if(visualSevere) {
        f=std::fopen(graphics.c_str(),"a");if(!f)return 2;
        std::fputs("enhanced_weak_points 1\ncombat_event_messages 1\nsevere_critical_bloodspill 1\nblood 1\nblood_spray 1\n",f);
        std::fclose(f);
    }
    }
    f=std::fopen(config.c_str(),"w");if(!f)return 2;std::fputs("show_messages 0\n",f);std::fclose(f);
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER,"dummy");if(!visual)SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
    if(!SDL_Init(SDL_INIT_VIDEO))return 3;
    std::vector<std::string> args={"critical-smoke","-iwad",argv[1],"-file",argv[2],"-config",config,"-graphics",graphics,
        "-nomonsters","-nomusic","-nosound","-width","640","-height","400","-warp",visualSurvival?"6":visual?"1":"4"};
    if(e1m8Mode){args={"e1m8-regression","-profilespikes","-iwad",argv[1],"-file","launcher/DiggingMaterials.wad","-config",config,"-graphics",graphics,"-nomusic","-nosound","-width","960","-height","540","-warp","1","8","-solidmaps","-uzmovement","-fluidmonsters","-fluidweapons","launcher/mods/FluidWeapons.pk3","-modsounds","launcher/mods/FluidWeapons.pk3","launcher/mods/FluidWeapons-SFX.pk3","-fluid-sounds","-weaponinterpolation"};}
    if(!settingsCase.empty())args.emplace_back("-classic");
    std::vector<char*> pointers;for(auto &s:args)pointers.push_back(s.data());
    if(solidMode){args.emplace_back("-solidmaps");pointers.clear();for(auto &s:args)pointers.push_back(s.data());}
    myargc=(int)pointers.size();myargv=pointers.data();singletics=true;D_DoomMain();return 0;
}
