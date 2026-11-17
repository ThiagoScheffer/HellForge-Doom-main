/* Isolate Windows resource APIs from the Doom engine header macros. */
#include "storm_audio.h"
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace doom3d { namespace storm_audio {
bool readResource(int number,Clip &clip) {
#ifdef _WIN32
    HMODULE module=GetModuleHandleA(nullptr);
    if(!module)return false;
    HRSRC found=FindResourceA(module,MAKEINTRESOURCEA(number),RT_RCDATA);
    if(!found)return false;
    DWORD bytes=SizeofResource(module,found);
    HGLOBAL handle=LoadResource(module,found);
    const void *data=handle?LockResource(handle):nullptr;
    if(!data||!bytes)return false;
    SDL_IOStream *source=SDL_IOFromConstMem(data,(size_t)bytes);
    if(!source)return false;
    Uint8 *decoded=nullptr;Uint32 length=0;
    bool ok=SDL_LoadWAV_IO(source,true,&clip.spec,&decoded,&length);
    if(!ok)return false;
    if(clip.spec.format==SDL_AUDIO_S16LE&&clip.spec.channels==2&&clip.spec.freq==44100&&
       length>0&&length%2==0)clip.pcm.assign(decoded,decoded+length);
    SDL_free(decoded);
    return !clip.pcm.empty();
#else
    (void)number;(void)clip;return false;
#endif
}
}} // doom3d::storm_audio
