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
 puts("PASS F4 nested director navigation and repaint boundaries");fflush(stdout);std::exit(0);
}
int main(int argc,char**argv){if(argc!=2)return 2;char app[]="settings-navigation",iwad[]="-iwad",warp[]="-warp",one[]="1",checkmap[]="-checkbigmap",nomusic[]="-nomusic",nosound[]="-nosound",cfg[]="-config",path[]="navigation-doom.cfg";char*args[]={app,iwad,argv[1],warp,one,checkmap,nomusic,nosound,cfg,path};myargc=10;myargv=args;R_BeforeTic=run;D_DoomMain();return 0;}
