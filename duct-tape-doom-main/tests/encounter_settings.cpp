#include "../platform/settings_panel.h"
#include "../platform/game_profile.h"
#include <cstdlib>
#include <set>
#include <string>
extern "C" {extern int netgame;int Game_HasEffect(game_effect){return 1;}}
int gpuMaxSamples(){return 8;}
namespace doom3d {bool voxelDetected(){return false;}}
using namespace doom3d;
static void check(bool okay,const char* text){if(!okay){std::fprintf(stderr,"FAIL director settings: %s\n",text);std::exit(1);}std::printf("PASS director settings: %s\n",text);}
int main(){
 Settings a;check(!a.encounter.enabled&&!a.encounter.monsters&&!a.encounter.reinforcements&&!a.encounter.items,"disabled defaults");
 for(const char* k:{"encounter_director_enabled","encounter_randomize_monsters","encounter_reinforcements_enabled","encounter_randomize_items","encounter_randomize_ammo","encounter_randomize_health","encounter_randomize_weapons"})readOption(a,k,1);
 readOption(a,"encounter_random_seed",16777215);readOption(a,"encounter_min_spawn_distance",9000);readOption(a,"encounter_max_spawn_distance",65);readOption(a,"encounter_min_group_size",8);readOption(a,"encounter_max_group_size",1);finishReading(a);
 check(a.encounter.minDistance<a.encounter.maxDistance&&a.encounter.minGroup<=a.encounter.maxGroup,"paired limits validated");
 FILE* f=std::tmpfile();check(f!=nullptr,"isolated settings stream");writeOptions(f,a);std::rewind(f);Settings b;char key[256];double value;std::set<std::string> seen;int count=0;
 while(std::fscanf(f,"%255s %lf",key,&value)==2){if(std::string(key).find("encounter_")==0){check(seen.insert(key).second,"director keys are unique");++count;}readOption(b,key,value);}std::fclose(f);finishReading(b);
 check(count>=30&&!std::memcmp(&a.encounter,&b.encounter,sizeof(a.encounter)),"all director values and full-precision seed round-trip");
 netgame=1;bool found=false;for(auto& tab:optionTabs(b))if(std::string(tab.title).find("ED ")==0||!strcmp(tab.title,"ENCOUNTER DIRECTOR")){found=true;for(auto& section:tab.sections)for(auto& o:section.options)check(!o.isEnabled(b),"network director controls read-only");}netgame=0;
 check(found&&b.encounter.enabled&&b.encounter.monsters&&b.encounter.items&&b.encounter.reinforcements,"network lock preserves independent preferences");
 std::puts("PASS Encounter Director option registry and persistence");
}
