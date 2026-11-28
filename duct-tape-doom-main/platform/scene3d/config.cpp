/* Reading and writing graphics.cfg; the options and their keys are listed
   once, in settings_panel.cpp. */
#include "internal.h"
#include "../settings_panel.h"
#include "../performance_trace.h"
#include <cerrno>
#include <fstream>
#include <sstream>
extern "C" { extern int screenblocks,detailLevel; }
extern "C" void AM_SetEnhancedAutomap(int enabled);
extern "C" {
#include "d_main.h"
}

namespace doom3d {
const char *graphicsConfig="graphics.cfg";
void loadSettings() {
    std::ifstream file(graphicsConfig,std::ios::binary);if(!file)return;
    std::string line;bool first=true;
    while(std::getline(file,line)) {
        if(first&&line.compare(0,3,"\xef\xbb\xbf")==0)line.erase(0,3);
        first=false;
        auto comment=line.find_first_of("#;");
        if(comment!=std::string::npos)line.erase(comment);
        std::istringstream input(line);std::string key,extra;double value;
        if(!(input>>key>>value)||key.size()>63||!std::isfinite(value)||(input>>extra))continue;
        readOption(settings,key.c_str(),value);
    }
    finishReading(settings);
    AM_SetEnhancedAutomap(settings.enhancedAutomap);
}
#ifdef DOOM_TEST_DRIVER
static bool quietSaveError=false;
#endif
static bool saveFailure(const std::string &reason) {
    std::string message="Your settings were applied, but could not be saved to:\n"+std::string(graphicsConfig)+"\n\n"+reason+"\nAny existing settings file has been preserved.";
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"%s",message.c_str());
#ifdef DOOM_TEST_DRIVER
    if(!quietSaveError)
#endif
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Settings not saved",message.c_str(),SDL_GetKeyboardFocus());
    return false;
}
static bool saveSettings() {
    static std::atomic<unsigned> sequence{0};
    std::string temporary=std::string(graphicsConfig)+".saving-"+std::to_string(SDL_GetPerformanceCounter())+"-"+std::to_string(++sequence)+".tmp";
    FILE *file=fopen(temporary.c_str(),"wb");
    if(!file)return saveFailure(std::strerror(errno));
    writeOptions(file,settings);
    bool complete=ferror(file)==0;
    if(fflush(file)!=0)complete=false;
    if(fclose(file)!=0)complete=false;
    if(complete&&SDL_RenamePath(temporary.c_str(),graphicsConfig))return true;
    std::string reason=complete?SDL_GetError():"The settings file could not be completely written.";
    SDL_RemovePath(temporary.c_str());
    return saveFailure(reason);
}
void applyPerformanceTraceSettings() {
    static bool wasEnabled=false;
    const bool enabled=settings.accelerated&&settings.performanceTrace;
    if(!enabled&&!wasEnabled)return;
    const performance::Config config{settings.performanceTraceTarget,settings.performanceTraceMode,
        settings.performanceTraceDuration,settings.performanceTraceDetail!=0,settings.performanceTraceAutoExport!=0};
    performance::configure(enabled,config,settingsSerial,serializeOptions(settings));
    if(enabled&&!wasEnabled) {
        auto basename=[](const char *path) {
            if(!path)return std::string{};
            std::string name=path;
            return name.substr(name.find_last_of("/\\")+1);
        };
        std::string addons;
        for(int i=1;i<MAXWADFILES&&wadfiles[i];++i){
            if(!addons.empty())addons+='\n';
            addons+=basename(wadfiles[i]);
        }
        performance::files(basename(wadfiles[0]).c_str(),std::move(addons));
    }
    wasEnabled=enabled;
}
void settingsChanged() {AM_SetEnhancedAutomap(settings.enhancedAutomap);applySettings();syncWallAmbienceSettings();R_SetViewSize(screenblocks,detailLevel);saveSettings();tickTime=0;++settingsSerial;applyPerformanceTraceSettings();}
} // namespace doom3d

#ifdef DOOM_TEST_DRIVER
extern "C" int I_TestSaveSettings(const char *path) {
    const char *previous=doom3d::graphicsConfig;doom3d::graphicsConfig=path;
    doom3d::quietSaveError=true;bool saved=doom3d::saveSettings();doom3d::quietSaveError=false;
    doom3d::graphicsConfig=previous;return saved;
}
#endif
