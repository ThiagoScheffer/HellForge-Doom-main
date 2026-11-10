#include <fstream>
#include <sstream>
extern "C" int I_TestSettingsPersistence(int);
extern "C" int I_TestSaveSettings(const char*);
static std::string settingsCase;
static std::string fileContents(const char *name){std::ifstream f(name,std::ios::binary);std::ostringstream s;s<<f.rdbuf();return s.str();}
static void settingsPersistenceFrame(){
    // Dummy video has no GPU device, so invoke the normal scene initialization explicitly.
    sceneInit(SDL_GetKeyboardFocus());
    if(settingsCase=="--settings-write"||settingsCase=="--settings-write-f4"||settingsCase=="--settings-write-button") {
        p_wp_balance.bleedDamage3=7;
        int mode=settingsCase=="--settings-write-button"?2:settingsCase=="--settings-write-f4"?1:0;
        check(I_TestSettingsPersistence(mode)!=0,"F4 control events and Apply/close save the current choices");
    }else if(settingsCase=="--settings-read"||settingsCase=="--settings-parse") {
        check(settings.criticalParts&&settings.enhancedWeakPoints&&settings.weaponVulnerabilities&&settings.localizedInjuries,"combat options reload after process restart");
        check(settings.monsterBleeding&&settings.headshotFrenzy&&settings.monsterFeeding&&settings.monsterMetalSparks,"new combat switches reload after process restart");
        check(settings.headshotFeedback==3&&!settings.headshotSound,"choice and disabled checkbox reload");
        check(settings.fov==95&&settings.scale==100&&settings.emissive&&settings.fog,"slider and explicitly re-enabled Low-FX graphics persist");
        check(settings.cornerPeek&&settings.peekLeft==SDL_SCANCODE_Z,"F4 key binding survives restart");
        check(p_wp_balance.bleedDamage3==7,"numeric balance tuning reloads");
    }else if(settingsCase=="--settings-legacy") {
        check(settings.bloodLevel==0&&settings.hitReactions==1,"legacy keys still feed combined choices across comments");
        check(settings.criticalParts==1,"valid lines after malformed legacy lines still load");
    }else if(settingsCase=="--settings-discard"||settingsCase=="--settings-unapplied") {
        auto before=fileContents(graphicsConfig);int critical=settings.criticalParts;float fov=settings.fov;
        check(I_TestSettingsPersistence(settingsCase=="--settings-discard"?3:4)!=0,"F4 draft can be explicitly discarded or left unapplied");
        check(settings.criticalParts==critical&&settings.fov==fov&&fileContents(graphicsConfig)==before,"discard and unapplied drafts do not write the profile");
    }else if(settingsCase=="--settings-save-errors") {
        auto before=fileContents(graphicsConfig);
#ifdef _WIN32
        FILE *locked=std::fopen(graphicsConfig,"rb");
        check(locked!=nullptr,"isolated config locked against replacement");
        int prior=settings.criticalParts;settings.criticalParts=!prior;
        check(!I_TestSaveSettings(graphicsConfig),"failed atomic replacement is reported without terminating the game");
        std::fclose(locked);settings.criticalParts=prior;
        check(fileContents(graphicsConfig)==before,"failed replacement preserves the old settings byte-for-byte");
#endif
        auto missing=output+"/missing-parent/graphics.cfg";
        check(!I_TestSaveSettings(missing.c_str()),"missing/unwritable destination reports a nonfatal save failure");
        check(fileContents(graphicsConfig)==before,"other save failures preserve the real config");
    }else I_Error((char*)"Unknown settings fixture mode");
    std::printf("PASS %d settings persistence checks %s\n",checks,settingsCase.c_str());I_Quit();
}
