#include <fstream>
#include <sstream>
extern "C" int I_TestSettingsPersistence(int);
extern "C" int I_TestSaveSettings(const char*);
static std::string settingsCase;
static std::string fileContents(const char *name){std::ifstream f(name,std::ios::binary);std::ostringstream s;s<<f.rdbuf();return s.str();}
static void settingsPersistenceFrame(){
    // Dummy video has no GPU device, so invoke the normal scene initialization explicitly.
    sceneInit(SDL_GetKeyboardFocus());
    if(settingsCase=="--settings-eye-default"){
        check(!settings.eyeInjuries&&!p_wp_eyes,"eye injuries default off");
    }else if(settingsCase=="--settings-eye-write"){
        check(!settings.eyeInjuries,"fresh eye setting starts off");
        check(I_TestSettingsPersistence(7),"real Combat checkbox applies through F4");
        check(settings.eyeInjuries&&p_wp_eyes&&p_wp_enhanced,"eye switch reaches gameplay with its required control");
    }else if(settingsCase=="--settings-eye-read"||settingsCase=="--settings-eye-discard"){
        check(settings.eyeInjuries&&settings.enhancedWeakPoints,"eye and required feature persist after restart");
        settings.accelerated=1;applySettings();check(p_wp_eyes&&p_wp_enhanced,"restored eye preference reaches simulation");
        if(settingsCase=="--settings-eye-discard"){
            auto before=fileContents(graphicsConfig);check(I_TestSettingsPersistence(8),"F10 discards eye checkbox draft");
            check(settings.eyeInjuries&&p_wp_eyes&&fileContents(graphicsConfig)==before,"eye discard leaves current behavior and file unchanged");
        }
    }else if(settingsCase=="--settings-ambush-ui-write"||settingsCase=="--settings-ambush-ui-disable"){
        bool enable=settingsCase=="--settings-ambush-ui-write";
        check(settings.ambushSightReuse!=enable,"isolated ambush profile starts in the expected state");
        check(I_TestSettingsPersistence(5),"Performance checkbox toggles through actual F4 apply handler");
        check(P_AmbushSightReuseActive()==enable,"F4 applies ambush sight choice to the running simulation");
        check(fileContents(graphicsConfig).find(std::string("ambush_sight_reuse ")+(enable?"1":"0"))!=std::string::npos,"F4 persists the ambush sight choice");
    }else if(settingsCase=="--settings-ambush-ui-read"||settingsCase=="--settings-ambush-ui-off"){
        bool expected=settingsCase=="--settings-ambush-ui-read";
        check(settings.ambushSightReuse==expected&&P_AmbushSightReuseActive()==expected,"ambush menu and simulation choice restore in a separate process");
    }else if(settingsCase=="--settings-ambush-ui-discard"){
        auto before=fileContents(graphicsConfig);int value=settings.ambushSightReuse;
        check(I_TestSettingsPersistence(6),"F10 discards the Performance checkbox draft");
        check(settings.ambushSightReuse==value&&P_AmbushSightReuseActive()==(value!=0)&&fileContents(graphicsConfig)==before,"discard preserves live behavior and the saved profile");
    }else if(settingsCase=="--settings-swarm-write"){
          settings.monsterHibernation=1;settings.swarmDebug=1;settings.spriteFiltering=1;settings.ambushSightReuse=1;applySettings();
        check(P_AmbushSightReuseActive(),"ambush sight switch applies immediately");
        check(I_TestSaveSettings(graphicsConfig)!=0,"swarm controls use the normal persistent settings writer");
    }else if(settingsCase=="--settings-swarm-read"){
          check(settings.monsterHibernation==1&&settings.swarmDebug==1&&settings.spriteFiltering==1&&settings.ambushSightReuse==1,"performance switches reload in a separate process");
          check(P_AmbushSightReuseActive(),"restored ambush switch reaches simulation");
          settings.ambushSightReuse=0;applySettings();check(!P_AmbushSightReuseActive(),"ambush sight switch disables immediately");
          settings.ambushSightReuse=1;applySettings();check(P_AmbushSightReuseActive(),"ambush sight switch re-enables immediately");
    }else if(settingsCase=="--settings-swarm-default"){
          check(!settings.monsterHibernation&&!settings.swarmDebug&&!settings.spriteFiltering&&!settings.ambushSightReuse,"performance controls default off");
          check(!P_AmbushSightReuseActive(),"ambush sight runtime defaults off");
    }else if(settingsCase=="--settings-write"||settingsCase=="--settings-write-f4"||settingsCase=="--settings-write-button") {
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
