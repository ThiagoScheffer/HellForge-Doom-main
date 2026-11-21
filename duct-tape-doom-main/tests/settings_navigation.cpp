// Exercise the real menu handlers and CPU painter; no GPU/window is required.
extern "C" {
#include "p_local.h"
#include "d_main.h"
#include "m_argv.h"
#include "sounds.h"
}
#include "../platform/settings_menu.cpp"
static int applied=0;
namespace doom3d {Settings settings;void settingsChanged(){++applied;}bool voxelDetected(){return false;}}
int gpuMaxSamples(){return 8;}
extern "C" {
int Game_HasEffect(game_effect){return 1;}
int Game_FontLump(int c){char name[16];snprintf(name,sizeof(name),"STCFN%03d",c);return W_CheckNumForName(name);}
int Game_FontSpacing(){return 1;}
int Game_MenuSound(game_menu_sound){return sfx_swtchn;}
}
static void check(bool okay,const char* label){if(!okay){fprintf(stderr,"FAIL F4 navigation: %s\n",label);std::exit(1);}printf("PASS F4 navigation: %s\n",label);}
static int directorIndex(){for(size_t i=0;i<panel.tabs.size();++i)if(!strcmp(panel.tabs[i].title,"ENCOUNTER DIRECTOR"))return int(i);return -1;}
static void choose(const char* key){for(size_t i=0;i<panel.lines.size();++i)if(panel.lines[i].option&&!strcmp(panel.lines[i].option->key,key)){panel.selected=int(i);keepSelectionVisible();return;}check(false,"option exists");}
static void capture(const char* path,const std::vector<unsigned>& pixels){FILE*f=fopen(path,"wb");check(f!=nullptr,"capture file");fprintf(f,"P6\n%d %d\n255\n",width,height);for(unsigned p:pixels){unsigned char rgb[]={static_cast<unsigned char>(p>>16),static_cast<unsigned char>(p>>8),static_cast<unsigned char>(p)};fwrite(rgb,1,3,f);}fclose(f);}
static void run(){
 R_BeforeTic=nullptr;I_Render3DSettingsMenu();int root=directorIndex();check(root>=0&&panel.directorPages.size()==5,"one parent and five nested director tabs");
 for(const auto& tab:panel.tabs)check(strncmp(tab.title,"ED ",3)!=0,"ED pages absent from main sidebar");
 int atmosphere=-1;for(size_t i=0;i<panel.tabs.size();++i)if(!strcmp(panel.tabs[i].title,"Atmosphere"))atmosphere=int(i);
 check(atmosphere>=0,"Atmosphere category is present");selectTab(atmosphere);choose("immersive_sky");
 auto& sky=selectedOption();check(sky.kind==OptionKind::Choice&&sky.flag==&panel.draft.immersiveSky&&sky.items.size()==6&&sky.values==std::vector<int>({0,1,2,3,4,5}),"Immersive sky exposes all six profiles in the actual F4 menu");
 check(panel.selected>=panel.scroll&&panel.selected<panel.scroll+visibleLines,"Immersive sky row is visible");
 for(int profile=1;profile<=6;++profile){I_Render3DSettingsKey(SDL_SCANCODE_RIGHT,0);check(panel.draft.immersiveSky==profile%6,"F4 cycles every sky profile and wraps to Off");}
 std::vector<unsigned> skyCapture(size_t(width)*height);I_Render3DSettingsDraw(skyCapture.data());capture("atmosphere-immersive-sky.ppm",skyCapture);
 selectTab(root);size_t roots=panel.tabs.size();check(panel.directorPage==-1&&selectedOption().flag==&panel.directorPage,"parent opens on Overview with page selector");
 const char* labels[]={"ED Monsters","ED Reinforcements","ED Items","ED Population","ED Advanced"};
 for(int i=0;i<5;++i){I_Render3DSettingsKey(SDL_SCANCODE_E,0);check(panel.directorPage==i&&!strcmp(currentPage().title,labels[i])&&panel.tabs.size()==roots,"Q/E selects nested page");}
 I_Render3DSettingsKey(SDL_SCANCODE_E,0);check(panel.directorPage==-1,"nested navigation wraps to Overview");
 I_Render3DSettingsKey(SDL_SCANCODE_RIGHT,0);check(panel.directorPage==0,"controller-compatible choice opens child page");
 loadFont();int x=(directorTabEdge(1)+directorTabEdge(2))/2;
 I_Render3DSettingsClick(x,cellHeight+8,1);check(panel.directorPage==1,"mouse selects internal tab");
 I_Render3DSettingsKey(SDL_SCANCODE_END,0);check(panel.lines[(size_t)panel.selected].option!=nullptr,"End skips trailing information headings");
 std::vector<unsigned> overview(size_t(width)*height),reinforce(overview.size()),extra(overview.size());
 selectDirectorPage(-1);I_Render3DSettingsDraw(overview.data());capture("director-overview.ppm",overview);
 auto original=panel.tabs;for(int i=0;i<25;++i){auto tab=panel.tabs[0];tab.title="Extra category";panel.tabs.push_back(std::move(tab));}
 I_Render3DSettingsDraw(extra.data());
 check(std::equal(overview.begin()+descriptionRow*cellHeight*width,overview.begin()+helpRow*cellHeight*width,extra.begin()+descriptionRow*cellHeight*width),"overflowing category list cannot paint over descriptions");
 selectTab(int(panel.tabs.size())-1);check(panel.tab-panel.tabScroll<visibleLines,"main sidebar scroll keeps selected category visible");
 I_Render3DSettingsClick(24,listRow*cellHeight+8,1);check(panel.tab==panel.tabScroll,"scrolled sidebar clicks use visible category index");
 panel.tabs=std::move(original);selectTab(root);selectDirectorPage(1);I_Render3DSettingsDraw(reinforce.data());capture("director-reinforcements.ppm",reinforce);
 selectDirectorPage(3);I_Render3DSettingsDraw(extra.data());capture("director-population.ppm",extra);
 selectDirectorPage(4);I_Render3DSettingsKey(SDL_SCANCODE_END,0);I_Render3DSettingsDraw(extra.data());capture("director-advanced.ppm",extra);
 selectDirectorPage(-1);choose("encounter_director_enabled");I_Render3DSettingsKey(SDL_SCANCODE_RETURN,0);check(panel.draft.encounter.enabled&&!settings.encounter.enabled,"nested controls edit draft only");
 I_Render3DSettingsKey(SDL_SCANCODE_F10,0);check(!panel.open&&!settings.encounter.enabled,"F10 discards nested edits");
 I_Render3DSettingsMenu();selectTab(directorIndex());choose("encounter_director_enabled");I_Render3DSettingsKey(SDL_SCANCODE_RETURN,0);I_Render3DSettingsKey(SDL_SCANCODE_F4,0);check(!panel.open&&settings.encounter.enabled&&applied==1,"F4 applies nested settings through existing handler");
 I_Render3DSettingsMenu();selectTab(directorIndex());selectDirectorPage(4);panel.draft.fov=100;
 for(size_t i=0;i<panel.lines.size();++i)if(panel.lines[i].option&&panel.lines[i].option->action==resetDirectorDraft){panel.selected=int(i);break;}
 I_Render3DSettingsKey(SDL_SCANCODE_RETURN,0);check(!panel.draft.encounter.enabled&&panel.draft.fov==100,"nested reset affects director draft only");
 I_Render3DSettingsKey(SDL_SCANCODE_F10,0);check(settings.encounter.enabled,"reset remains discardable");
 I_Render3DSettingsMenu();
 int textures=-1;
 for(size_t i=0;i<panel.tabs.size();++i)if(!strcmp(panel.tabs[i].title,"Textures"))textures=int(i);
 check(textures>=0,"Textures category is present");selectTab(textures);choose("living_surfaces");
 check(!panel.draft.livingSurfaces&&panel.draft.livingIntensity==2,"immersion defaults to Off and Medium");
 I_Render3DSettingsKey(SDL_SCANCODE_RETURN,0);choose("living_intensity");
 I_Render3DSettingsKey(SDL_SCANCODE_RIGHT,0);check(panel.draft.livingIntensity==3,"intensity can reach High");
 I_Render3DSettingsKey(SDL_SCANCODE_RIGHT,0);check(panel.draft.livingIntensity==1,"intensity wraps to Low");
 for(const char *key:{"living_morph","living_pulse","living_wetness"}) {
     choose(key);I_Render3DSettingsKey(SDL_SCANCODE_RETURN,0);
 }
 I_Render3DSettingsKey(SDL_SCANCODE_F4,0);
 check(!panel.open&&applied==2&&settings.livingSurfaces&&settings.livingIntensity==1&&
       !settings.livingMorph&&!settings.livingPulse&&!settings.livingWetness,
       "F4 applies the master, intensity and three independent immersion toggles");
 FILE *profile=tmpfile();check(profile!=nullptr,"immersion settings profile stream");
 writeOptions(profile,settings);rewind(profile);
 Settings restored;char line[256],key[64];double value;
 while(fgets(line,sizeof(line),profile))if(sscanf(line,"%63s %lf",key,&value)==2)readOption(restored,key,value);
 fclose(profile);finishReading(restored);
 check(restored.livingSurfaces&&restored.livingIntensity==1&&!restored.livingMorph&&!restored.livingPulse&&
       !restored.livingWetness,"the real settings writer and reader round-trip all five values");
 I_Render3DSettingsMenu();
 int gameplay=-1;
 for(size_t i=0;i<panel.tabs.size();++i)if(!strcmp(panel.tabs[i].title,"Gameplay"))gameplay=int(i);
 check(gameplay>=0,"Gameplay category is present");selectTab(gameplay);choose("monster_weapon_drops");
 check(panel.draft.monsterWeaponDrops==MONSTER_DROPS_CLASSIC,"Classic drop mode is the default");
 I_Render3DSettingsKey(SDL_SCANCODE_RIGHT,0);
 check(panel.draft.monsterWeaponDrops==MONSTER_DROPS_AMMO,"F4 selects Ammo Instead");
 I_Render3DSettingsKey(SDL_SCANCODE_RIGHT,0);
 I_Render3DSettingsKey(SDL_SCANCODE_F4,0);
 check(settings.monsterWeaponDrops==MONSTER_DROPS_SCARCE&&applied==3,"F4 applies Scarce Ammo");
 profile=tmpfile();check(profile!=nullptr,"drop mode profile stream");
 writeOptions(profile,settings);rewind(profile);
 Settings dropProfile;
 while(fgets(line,sizeof(line),profile))if(sscanf(line,"%63s %lf",key,&value)==2)readOption(dropProfile,key,value);
 fclose(profile);finishReading(dropProfile);
 check(dropProfile.monsterWeaponDrops==MONSTER_DROPS_SCARCE,"drop mode survives settings round-trip");
 readOption(dropProfile,"monster_weapon_drops",99);
 check(dropProfile.monsterWeaponDrops==MONSTER_DROPS_SCARCE,"invalid drop mode ignored");
 puts("PASS F4 nested director navigation and repaint boundaries");fflush(stdout);std::exit(0);
}
int main(int argc,char**argv){if(argc!=2)return 2;char app[]="settings-navigation",iwad[]="-iwad",warp[]="-warp",one[]="1",checkmap[]="-checkbigmap",nomusic[]="-nomusic",nosound[]="-nosound",cfg[]="-config",path[]="navigation-doom.cfg";char*args[]={app,iwad,argv[1],warp,one,checkmap,nomusic,nosound,cfg,path};myargc=10;myargv=args;R_BeforeTic=run;D_DoomMain();return 0;}
