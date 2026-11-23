#include <fstream>
#include <sstream>
extern "C" int I_TestSettingsPersistence(int);
extern "C" int I_TestSaveSettings(const char*);
static std::string settingsCase;
static std::string fileContents(const char *name){std::ifstream f(name,std::ios::binary);std::ostringstream s;s<<f.rdbuf();return s.str();}
static void settingsPersistenceFrame(){
    // Dummy video has no GPU device, so invoke the normal scene initialization explicitly.
    sceneInit(SDL_GetKeyboardFocus());
    if(settingsCase=="--settings-oil-write"||settingsCase=="--settings-oil-read"||settingsCase=="--settings-oil-disable") {
        const bool enable=settingsCase=="--settings-oil-write";
        check(settings.incendiaryBarrels!=enable,"oil profile starts with the expected persisted choice");
        if(settingsCase!="--settings-oil-read")
            check(I_TestSettingsPersistence(12),"real F4 Combat checkbox applies and saves");
        const bool want=settingsCase!="--settings-oil-disable";
        check((settings.incendiaryBarrels!=0)==want&&(P_IncendiaryEnabled()!=0)==want,
              "oil preference reaches the live engine and survives process restart");
    }else if(settingsCase=="--settings-immersion-write"||settingsCase=="--settings-immersion-read") {
        if(settingsCase=="--settings-immersion-write") {
            check(!settings.livingSurfaces&&settings.livingIntensity==2,"new profile starts Off and Medium");
            check(I_TestSettingsPersistence(11),"F4 applies and saves every immersion control");
        }
        check(settings.livingSurfaces&&settings.livingIntensity==3&&!settings.livingMorph&&!settings.livingPulse&&
              !settings.livingWetness,"all immersion choices match the persisted profile");
    }else if(settingsCase=="--settings-speedrun-split-write"||settingsCase=="--settings-speedrun-split-read"){
        if(settingsCase=="--settings-speedrun-split-write"){
            settings.accelerated=settings.speedrunHUD=settings.speedrunOnly=1;settings.speedrunLayout=2;
            settings.speedrunSecrets=settings.speedrunItems=settings.speedrunHealth=settings.speedrunArmor=settings.speedrunAmmo=1;
            settings.speedrunPlayerAnchor=3;settings.speedrunPlayerX=-12;settings.speedrunPlayerY=9;
            settings.speedrunX=25;settings.speedrunY=-6;settings.speedrunMapName=0;
            check(I_TestSaveSettings(graphicsConfig),"split HUD saves through the normal F4 writer");
        }
        check(settings.speedrunOnly&&settings.speedrunLayout==2&&settings.speedrunSecrets&&settings.speedrunItems&&
              settings.speedrunHealth&&settings.speedrunArmor&&settings.speedrunAmmo&&!settings.speedrunMapName&&
              settings.speedrunPlayerAnchor==3&&settings.speedrunPlayerX==-12&&settings.speedrunPlayerY==9&&
              settings.speedrunX==25&&settings.speedrunY==-6,"split HUD restores independent placement and metrics");
        settings.accelerated=1;
        bool pending=worldPending;worldPending=true;
        constexpr int layer=SCREENWIDTH*SCREENHEIGHT;
        std::vector<unsigned> source(layer,0xffffffffu);
        auto atlas=hudPixels(source.data());
        check(std::none_of(atlas.begin(),atlas.begin()+2*layer,[](unsigned pixel){return pixel>>24;}),"HUD-only skips both standard HUD layers");
        const auto left=speedrunAtlasPanels[0],right=speedrunAtlasPanels[1];
        check(left.width>0&&right.width>0&&left.x+left.width<=right.x,"split HUD packs two disjoint atlas regions");
        auto map=hudScreenPosition(1920,1080,1440,left.width,left.height,0,100,0,false,true);
        auto player=hudScreenPosition(1920,1080,1440,right.width,right.height,3,-100,5,false,true);
        check(map.x+map.w<=960&&player.x>=960&&player.y>map.y,"extreme offsets keep the columns on separate sides");
        settings.speedrunHealth=settings.speedrunArmor=settings.speedrunAmmo=0;
        hudPixels(source.data());
        check(speedrunAtlasPanels[1].width==0,"an empty player column leaves no panel behind");
        worldPending=pending;
    }else if(settingsCase=="--settings-layout-write"){
        check(settings.speedrunLayout==0&&settings.speedrunX==0&&settings.speedrunY==0,"existing profiles retain column layout and zero adjustments");
        char sample[8][48]={"MAP04","ATT 1","DTH 0","KS 0","TIME 00:00.00","RUN 00:00:00","DEALT 0","TAKEN 0"};int visible[8]={1,1,1,1,1,1,1,1};
        for(int layout:{0,1})for(float offset:{-100.f,0.f,100.f}){
            settings.speedrunLayout=layout;settings.speedrunX=settings.speedrunY=offset;auto p=speedrunPanel(sample,visible);
            check(p.x>=3&&p.y>=3&&p.x-3+p.width<=SCREENWIDTH&&p.y-3+p.height<=SCREENHEIGHT,"both layouts clamp the entire panel within the HUD viewport");
        }
        settings.speedrunLayout=1;auto row=speedrunPanel(sample,visible);check(row.cells[0].y==row.cells[7].y,"normal metrics fit side by side in a single info row");
        {bool pending=worldPending;worldPending=true;settings.speedrunHUD=1;settings.speedrunX=settings.speedrunY=0;
            std::vector<unsigned> pixels(SCREENWIDTH*SCREENHEIGHT,0);combatWidgets(pixels.data());int top=SCREENHEIGHT,bottom=-1;
            for(int y=0;y<SCREENHEIGHT;++y)for(int x=0;x<SCREENWIDTH;++x)if(pixels[y*SCREENWIDTH+x]>>24){top=std::min(top,y);bottom=std::max(bottom,y);}
            check(bottom>=top&&bottom-top<16,"actual row painting forms a single compact info bar");worldPending=pending;}
        for(int i=1;i<8;++i)std::snprintf(sample[i],48,"VERY LARGE SESSION VALUE 12345678901234567890");auto wrap=speedrunPanel(sample,visible);
        check(wrap.cells[7].y>wrap.cells[1].y&&wrap.width<=SCREENWIDTH,"long row values wrap safely rather than clipping off screen");
        visible[2]=0;check(speedrunPanel(sample,visible).count==7,"hidden metrics consume no layout cells");
        settings.speedrunX=15;settings.speedrunY=-20;settings.speedrunLayout=1;
        check(I_TestSaveSettings(graphicsConfig),"position and layout save through the normal F4 writer");
    }else if(settingsCase=="--settings-layout-read"){
        check(settings.speedrunLayout==1&&settings.speedrunX==15&&settings.speedrunY==-20,"row layout and independent X/Y adjustments persist after restart");
    }else if(settingsCase=="--settings-opacity-write"){
        check(settings.hudBackgroundOpacity==75&&settings.speedrunBackgroundOpacity==80,"HUD opacity defaults retain dark readable panels");
        bool pending=worldPending;worldPending=true;settings.hudStyle=1;settings.hudFace=0;settings.hudLayout=0;settings.hudScale=100;settings.speedrunHUD=1;settings.speedrunAnchor=0;
        for(float percent:{0.f,25.f,100.f}){
            settings.hudBackgroundOpacity=settings.speedrunBackgroundOpacity=percent;
            std::vector<unsigned> hud(SCREENWIDTH*SCREENHEIGHT,0),speed(SCREENWIDTH*SCREENHEIGHT,0);modernHud(hud.data());combatWidgets(speed.data());
            unsigned expected=(unsigned)std::lround(percent*2.55f);
            check((hud[165*SCREENWIDTH+9]>>24)==expected&&(hud[SCREENWIDTH+222]>>24)==expected,"modern panel and stats alpha follow the slider");
            check((speed[53*SCREENWIDTH+5]>>24)==expected,"Speedrun panel alpha follows its independent slider");
            check((hud[167*SCREENWIDTH+12]>>24)==255&&(speed[56*SCREENWIDTH+8]>>24)==255,"HUD and Speedrun text stay opaque including at zero backdrop");
        }
        worldPending=pending;settings.hudBackgroundOpacity=0;settings.speedrunBackgroundOpacity=25;
        check(I_TestSaveSettings(graphicsConfig),"opacity controls save through the normal config writer");
    }else if(settingsCase=="--settings-opacity-read"){
        check(settings.hudBackgroundOpacity==0&&settings.speedrunBackgroundOpacity==25,"independent opacity choices persist across process restart");
        readOption(settings,"hud_background_opacity",-10);readOption(settings,"speedrun_background_opacity",150);
        check(settings.hudBackgroundOpacity==0&&settings.speedrunBackgroundOpacity==100,"opacity values clamp safely to zero through one hundred");
    }else if(settingsCase=="--settings-elemental2-write"){
        check(!settings.elementalDot&&!settings.combatMessages&&!settings.speedrunHUD,"three new features default off");
        check(I_TestSettingsPersistence(9),"actual Combat and SPEEDRUN tabs apply new controls through F4");
        check(p_wp_dot&&settings.speedrunHUD&&settings.combatMessages&&!settings.speedrunDeaths&&settings.speedrunAnchor==3,"metric visibility and anchor apply independently");
    }else if(settingsCase=="--settings-elemental2-read"){
        check(settings.elementalDot&&settings.combatMessages&&settings.speedrunHUD&&!settings.speedrunDeaths&&settings.speedrunAnchor==3,"all new settings restore from separate-process profile");
        settings.accelerated=1;applySettings();check(p_wp_dot,"restored damaging option reaches gameplay");
        auto before=fileContents(graphicsConfig);check(I_TestSettingsPersistence(10),"F10 discards new UI drafts");check(fileContents(graphicsConfig)==before&&p_wp_dot&&settings.speedrunHUD,"discard preserves file and active controls");
    }else if(settingsCase=="--settings-elemental-write"){
        check(!settings.elementalCombatFX&&!settings.advancedMonsterReactions,"new elemental controls default off");
        settings.elementalCombatFX=settings.advancedMonsterReactions=1;settings.accelerated=1;applySettings();
        check(p_wp_elemental&&p_wp_reactions,"both controls reach gameplay");
        check(I_TestSaveSettings(graphicsConfig),"normal writer persists new controls");
    }else if(settingsCase=="--settings-elemental-read"){
        check(settings.elementalCombatFX&&settings.advancedMonsterReactions,"new controls survive separate-process restart");
        settings.accelerated=1;applySettings();check(p_wp_elemental&&p_wp_reactions,"restored controls reach runtime");
        settings.elementalCombatFX=settings.advancedMonsterReactions=0;applySettings();check(!p_wp_elemental&&!p_wp_reactions,"disable applies immediately");
    }else if(settingsCase=="--settings-eye-default"){
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
