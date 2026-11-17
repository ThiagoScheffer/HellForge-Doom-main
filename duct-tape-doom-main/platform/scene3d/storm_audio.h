/* HELLFORGE thunder sound playback for the Windows launcher build.
   WAVs are embedded as Win32 RCDATA by CMake, so the launcher can extract a
   standalone executable without looking for loose sound files. This is a
   separate SDL3 logical playback stream. The initial milestone does not pass
   through HELLFORGE's positional/reverb mixer; SFX volume is applied here.
   Failure to initialize audio never blocks weather or gameplay. */
#ifndef HELLFORGE_SCENE3D_STORM_AUDIO_H
#define HELLFORGE_SCENE3D_STORM_AUDIO_H
#include "../storm_rules.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
namespace doom3d { namespace storm_audio {
struct Clip {SDL_AudioSpec spec{};std::vector<Uint8> pcm;};
struct Player {
    SDL_AudioStream *stream=nullptr;
    Clip clips[3];
    bool attempted=false,usable=false;
};
inline Player &player() {static Player p;return p;}
bool readResource(int number,Clip &clip);
inline bool prepare() {
    Player &p=player();
    if(p.attempted)return p.usable;
    p.attempted=true;
    if(!(SDL_WasInit(SDL_INIT_AUDIO)&SDL_INIT_AUDIO))return false;
    for(int i=0;i<3;++i)if(!readResource(22001+i,p.clips[i]))return false;
    p.stream=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&p.clips[0].spec,nullptr,nullptr);
    if(!p.stream)return false;
    if(!SDL_ResumeAudioStreamDevice(p.stream)) {
        SDL_DestroyAudioStream(p.stream);p.stream=nullptr;return false;
    }
    p.usable=true;
    return true;
}
inline void stop() {
    Player &p=player();
    if(p.stream)SDL_DestroyAudioStream(p.stream);
    p=Player{};
}
inline void muteQueued() {
    Player &p=player();
    if(p.stream)SDL_ClearAudioStream(p.stream);
}
inline void play(doom_storm::Kind type,float volume,int sfxVolume) {
    if(type==doom_storm::Kind::None||volume<=0||sfxVolume<=0||!prepare())return;
    Player &p=player();
    // One thunder tail at a time; no unbounded backlog on long samples.
    if(SDL_GetAudioStreamQueued(p.stream)>0)return;
    int index=type==doom_storm::Kind::Close?0:type==doom_storm::Kind::Medium?1:2;
    float distanceGain=index==0?.65f:index==1?.45f:.25f;
    float gain=std::clamp(volume,0.f,1.f)*std::clamp(sfxVolume/15.f,0.f,1.f)*distanceGain;
    const auto &sample=p.clips[index].pcm;
    if(sample.empty()||gain<=0)return;
    std::vector<Uint8> scaled(sample.size());
    for(size_t i=0;i+1<sample.size();i+=2) {
        std::int16_t original;
        std::memcpy(&original,sample.data()+i,sizeof(original));
        int value=(int)std::lround(original*gain);
        std::int16_t result=(std::int16_t)std::clamp(value,-32768,32767);
        std::memcpy(scaled.data()+i,&result,sizeof(result));
    }
    if(SDL_PutAudioStreamData(p.stream,scaled.data(),(int)scaled.size()))SDL_FlushAudioStream(p.stream);
}
}} // doom3d::storm_audio
#endif
