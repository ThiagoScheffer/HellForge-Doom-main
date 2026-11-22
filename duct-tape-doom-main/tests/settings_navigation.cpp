// Real F4 handlers and CPU painter. No GPU windows or image captures.
extern "C" {
#include "p_local.h"
#include "d_main.h"
#include "m_argv.h"
#include "sounds.h"
}
#include "../platform/settings_menu.cpp"
#include <map>
#include <set>
static int applied=0,checks=0;
namespace doom3d {Settings settings;void settingsChanged(){++applied;}bool voxelDetected(){return false;}}
int gpuMaxSamples(){return 8;}
extern "C" {
int Game_HasEffect(game_effect){return 1;}
int Game_FontLump(int c){char name[16];snprintf(name,sizeof(name),"STCFN%03d",c);return W_CheckNumForName(name);}
int Game_FontSpacing(){return 1;}
int Game_MenuSound(game_menu_sound){return sfx_swtchn;}
}
static void check(bool ok,const char *message){++checks;if(!ok){fprintf(stderr,"FAIL F4: %s\n",message);std::exit(1);}}
static void choose(const char *key){check(selectSetting(key),"setting reachable");}
static void type(const char *s){for(;*s;++s)I_Render3DSettingsKey(*s==' '?SDL_SCANCODE_SPACE:SDL_SCANCODE_A+*s-'a',0);}
static void paint(){std::vector<unsigned> pixels((size_t)width*height);I_Render3DSettingsDraw(pixels.data());check(pixels.front()==backdrop&&pixels.back()==backdrop,"plain painter borders");}
static void run(){
 R_BeforeTic=nullptr;I_Render3DSettingsMenu();check(panel.tabs.size()==16&&panel.tabs.size()<=(size_t)visibleLines,"all categories fit sidebar");
 std::map<std::string,Option> registry;std::set<std::string> seen;
 for(auto &t:optionTabs(panel.draft))for(auto &s:t.sections)for(auto &o:s.options)if(o.key&&*o.key)check(registry.emplace(o.key,o).second,"unique registry keys");
 for(size_t t=0;t<panel.tabs.size();++t){selectTab((int)t);int n=(int)currentPages().size();check(n>0,"category has sections");
  for(int p=0;p<n;++p){selectPage(p);check(hasSelection(),"page has controls");
   for(auto &s:currentPage().sections)for(auto &o:s.options)if(o.key&&*o.key){check(seen.insert(o.key).second,"setting appears once");const auto &a=registry.at(o.key);check(o.kind==a.kind&&o.flag==a.flag&&o.number==a.number&&o.enabled==a.enabled&&o.values==a.values&&o.low==a.low&&o.high==a.high&&o.step==a.step,"binding, dependencies and ranges retained");}
   loadFont();check(pageTabEdge(0)==leftColumn*cellWidth&&pageTabEdge(n)==rightColumn*cellWidth,"sub-tabs fit pane");paint();
   I_Render3DSettingsKey(SDL_SCANCODE_E,0);check(panel.page==(p+1)%n,"E wraps sections");I_Render3DSettingsKey(SDL_SCANCODE_Q,0);check(panel.page==p,"Q reverses sections");
   int next=(p+1)%n;I_Render3DSettingsClick((pageTabEdge(next)+pageTabEdge(next+1))/2,cellHeight+8,1);check(panel.page==next,"mouse sub-tab selection");
   if(n>1){check(selectedOption().flag==&panel.page,"controller-accessible page choice");I_Render3DSettingsKey(SDL_SCANCODE_RIGHT,0);check(panel.page==(next+1)%n,"choice changes page");}
  }
 }
 check(seen.size()==registry.size(),"all settings reachable exactly once");
 auto original=panel.tabs;for(int i=0;i<25;++i){auto t=panel.tabs[0];t.title="Extra category";panel.tabs.push_back(std::move(t));}selectTab((int)panel.tabs.size()-1);check(panel.tab-panel.tabScroll<visibleLines,"sidebar overflow scrolls");I_Render3DSettingsClick(24,listRow*cellHeight+8,1);check(panel.tab==panel.tabScroll,"sidebar click after scroll");panel.tabs=std::move(original);selectTab(0);paint();
 choose("window_strength");check(!strcmp(panel.tabs[panel.tab].title,"Lighting")&&!strcmp(currentPage().title,"Windows"),"window settings located correctly");panel.draft.bakedLights=0;panel.draft.windowLight=1;float strength=panel.draft.windowStrength;check(!selectedOption().isEnabled(panel.draft)&&dependencyHint(selectedOption()).find("Lighting / Baked")!=std::string::npos,"dependency points to correct sub-tab");I_Render3DSettingsKey(SDL_SCANCODE_RIGHT,0);check(panel.draft.windowStrength==strength,"disabled controls locked");
 choose("fov");I_Render3DSettingsKey(SDL_SCANCODE_RIGHT,0);I_Render3DSettingsKey(SDL_SCANCODE_TAB,0);check(panel.draft.fov==95&&settings.fov==90,"navigation preserves draft only");choose("fov");I_Render3DSettingsKey(SDL_SCANCODE_R,0);check(panel.draft.fov==90,"reset default");I_Render3DSettingsKey(SDL_SCANCODE_F10,0);check(!panel.open&&applied==0&&settings.bakedLights,"discard draft");
 check(I_TestSettingsControls()==1,"real key binding and dependency controls");
 I_Render3DSettingsMenu();I_Render3DSettingsKey(SDL_SCANCODE_F,0);type("baked light");check(panel.finding&&hasSelection(),"word search");bool found=false;for(size_t i=0;i<panel.lines.size();++i)if(!strcmp(panel.lines[i].option->key,"baked_lights")){panel.selected=(int)i;found=true;break;}check(found&&panel.lines[panel.selected].location=="Lighting / Baked","search location breadcrumb");I_Render3DSettingsKey(SDL_SCANCODE_RETURN,0);int before=panel.draft.bakedLights;I_Render3DSettingsKey(SDL_SCANCODE_RETURN,0);check(panel.draft.bakedLights==!before&&settings.bakedLights==before,"search edits real draft");I_Render3DSettingsKey(SDL_SCANCODE_R,0);check(panel.draft.bakedLights==before,"search reset");I_Render3DSettingsKey(SDL_SCANCODE_ESCAPE,0);check(panel.open&&!searching(),"Escape exits find without apply");
 I_Render3DSettingsKey(SDL_SCANCODE_F,0);type("window");I_Render3DSettingsKey(SDL_SCANCODE_MINUS,1);type("strength");check(panel.lines.size()==1&&!strcmp(panel.lines[0].option->key,"window_strength"),"config key search supports underscore");I_Render3DSettingsKey(SDL_SCANCODE_ESCAPE,0);
 I_Render3DSettingsKey(SDL_SCANCODE_F,0);type("qerzxy");check(panel.lines.empty(),"empty search state");paint();I_Render3DSettingsKey(SDL_SCANCODE_RETURN,0);for(int key:{SDL_SCANCODE_UP,SDL_SCANCODE_DOWN,SDL_SCANCODE_HOME,SDL_SCANCODE_END,SDL_SCANCODE_R,SDL_SCANCODE_RIGHT})I_Render3DSettingsKey(key,0);check(!hasSelection()&&panel.open,"empty search navigation safe");I_Render3DSettingsKey(SDL_SCANCODE_F10,0);check(applied==0,"discard from find");
 I_Render3DSettingsMenu();choose("immersive_sky");for(int i=0;i<6;++i)I_Render3DSettingsKey(SDL_SCANCODE_RIGHT,0);check(panel.draft.immersiveSky==0,"six sky profiles cycle");choose("hell_blood_eclipse");check(!panel.draft.hellBloodEclipse&&!selectedOption().isEnabled(panel.draft),"eclipse defaults off and needs immersive sky");choose("immersive_sky");I_Render3DSettingsKey(SDL_SCANCODE_RIGHT,0);choose("hell_blood_eclipse");check(selectedOption().isEnabled(panel.draft),"eclipse supports Auto");I_Render3DSettingsKey(SDL_SCANCODE_RETURN,0);
 choose("living_surfaces");I_Render3DSettingsKey(SDL_SCANCODE_RETURN,0);choose("living_intensity");I_Render3DSettingsKey(SDL_SCANCODE_RIGHT,0);
 choose("speedrun_hud");I_Render3DSettingsKey(SDL_SCANCODE_RETURN,0);choose("speedrun_layout");I_Render3DSettingsKey(SDL_SCANCODE_RIGHT,0);I_Render3DSettingsKey(SDL_SCANCODE_RIGHT,0);
 choose("monster_weapon_drops");I_Render3DSettingsKey(SDL_SCANCODE_RIGHT,0);I_Render3DSettingsKey(SDL_SCANCODE_RIGHT,0);I_Render3DSettingsKey(SDL_SCANCODE_F4,0);check(!panel.open&&applied==1&&settings.hellBloodEclipse&&settings.livingSurfaces&&settings.livingIntensity==3&&settings.speedrunLayout==2&&settings.monsterWeaponDrops==MONSTER_DROPS_SCARCE,"F4 applies cross-category edits");
 FILE *file=tmpfile();check(file!=nullptr,"isolated profile");writeOptions(file,settings);rewind(file);Settings restored;char line[512],key[128];double value;std::set<std::string> written;while(fgets(line,sizeof(line),file))if(sscanf(line,"%127s %lf",key,&value)==2){check(written.insert(key).second,"writer keys unique");readOption(restored,key,value);}fclose(file);finishReading(restored);check(restored.hellBloodEclipse&&restored.livingSurfaces&&restored.livingIntensity==3&&restored.speedrunLayout==2&&restored.monsterWeaponDrops==MONSTER_DROPS_SCARCE,"profile values round-trip");
 I_Render3DSettingsMenu();choose("encounter_director_enabled");I_Render3DSettingsKey(SDL_SCANCODE_RETURN,0);choose("encounter_debug");bool reset=false;for(size_t i=0;i<panel.lines.size();++i)if(panel.lines[i].option&&panel.lines[i].option->action==resetDirectorDraft){panel.selected=(int)i;reset=true;break;}check(reset,"director reset retained");panel.draft.fov=100;I_Render3DSettingsKey(SDL_SCANCODE_RETURN,0);check(!panel.draft.encounter.enabled&&panel.draft.fov==100,"director reset isolated");I_Render3DSettingsKey(SDL_SCANCODE_F10,0);
 printf("PASS F4 rework: %d checks, %zu settings, %zu categories\n",checks,registry.size(),panel.tabs.size());fflush(stdout);std::exit(0);
}
int main(int argc,char**argv){if(argc!=2)return 2;char app[]="settings-navigation",iwad[]="-iwad",warp[]="-warp",one[]="1",checkmap[]="-checkbigmap",nomusic[]="-nomusic",nosound[]="-nosound",cfg[]="-config",path[]="navigation-doom.cfg";char*args[]={app,iwad,argv[1],warp,one,checkmap,nomusic,nosound,cfg,path};myargc=10;myargv=args;R_BeforeTic=run;D_DoomMain();return 0;}
