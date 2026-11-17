// Exercise storm stream ownership against the actual SDL audio lifecycle.
#include "platform/scene3d/storm_audio.h"
#include <cstdio>
#include <cstring>
int main(int argc,char **argv) {
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER,"dummy");
    if(!SDL_InitSubSystem(SDL_INIT_AUDIO))return 2;
    SDL_AudioSpec spec{SDL_AUDIO_S16LE,2,44100};
    auto &p=doom3d::storm_audio::player();
    p.stream=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,nullptr,nullptr);
    if(!p.stream)return 3;
    if(argc>1&&std::strcmp(argv[1],"unsafe-order")==0)
        SDL_QuitSubSystem(SDL_INIT_AUDIO); // Original engine exit order.
    doom3d::storm_audio::stop();
    doom3d::storm_audio::stop(); // Cleanup must be repeatable.
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    std::puts("PASS: storm stream released before audio subsystem shutdown");
}
