/* Common C11 system interface for the portable and legacy Linux targets. */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* The engine supplies fixed-width limits with these historical names. */
#undef MAXCHAR
#undef MAXSHORT
#undef MAXINT
#undef MAXLONG
#undef MINCHAR
#undef MINSHORT
#undef MININT
#undef MINLONG
#else
#include <errno.h>
#include <time.h>
#endif

#include "doomdef.h"
#include "i_system.h"
#include "i_sound.h"
#include "i_video.h"
#include "m_misc.h"
#include "m_argv.h"
#include <errno.h>
#include "d_net.h"
#include "g_game.h"
#include "game_profile.h"
#ifdef DOOM_NATIVE
#include <SDL3/SDL.h>
#endif

#if defined(DOOM_NATIVE) && UINTPTR_MAX > UINT32_MAX
int mb_used = 512;
#else
int mb_used = 128;
#endif
static ticcmd_t emptycmd;

ticcmd_t *I_BaseTiccmd(void) { return &emptycmd; }
int I_GetHeapSize(void) { return mb_used * 1024 * 1024; }

byte *I_ZoneBase(int *size)
{
    byte *memory;
    int arg=M_CheckParm("-heapsize");
    if(arg){
        char *end;errno=0;
        if(arg+1>=myargc)I_Error("-heapsize requires MiB (16..2047)");
        long value=strtol(myargv[arg+1],&end,10);
        if(errno || *end || value<16 || value>2047)I_Error("Invalid -heapsize; expected 16..2047 MiB");
        mb_used=(int)value;
    }
    if (mb_used < 16 || mb_used > INT_MAX / (1024 * 1024))
        I_Error("Invalid zone size");
    *size = I_GetHeapSize();
    printf("Zone capacity: %d MiB\n",mb_used);
#if defined(_WIN32) && defined(DOOM_MONSTER_BENCHMARK)
    {
        int fixed=M_CheckParm("-bench-zone-base");
        if(fixed&&fixed+1<myargc){
            void *address=(void*)(uintptr_t)strtoull(myargv[fixed+1],NULL,0);
            memory=(byte*)VirtualAlloc(address,(size_t)*size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
            if(!memory||memory!=address)I_Error("Benchmark fixed zone address unavailable");
            printf("Benchmark fixed zone: %p, %d bytes\n",memory,*size);
            return memory;
        }
    }
#endif
    memory = malloc((size_t)*size);
    if (!memory)
        I_Error("Could not allocate the DOOM memory zone");
    return memory;
}

int I_GetTime(void)
{
    uint64_t ticks;
#ifdef _WIN32
    static LARGE_INTEGER frequency, base;
    LARGE_INTEGER now;
    if (!frequency.QuadPart)
    {
        if (!QueryPerformanceFrequency(&frequency) || !QueryPerformanceCounter(&base))
            I_Error("Could not initialize the monotonic clock");
    }
    if (!QueryPerformanceCounter(&now))
        I_Error("Could not read the monotonic clock");
    uint64_t elapsed = (uint64_t)(now.QuadPart - base.QuadPart);
    ticks = (elapsed / frequency.QuadPart) * TICRATE
          + (elapsed % frequency.QuadPart) * TICRATE / frequency.QuadPart;
#else
    static struct timespec base;
    static boolean initialized;
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now))
        I_Error("Could not read the monotonic clock");
    if (!initialized)
    {
        base = now;
        initialized = true;
    }
    int64_t seconds = (int64_t)now.tv_sec - base.tv_sec;
    int64_t nanoseconds = (int64_t)now.tv_nsec - base.tv_nsec;
    if (nanoseconds < 0)
    {
        --seconds;
        nanoseconds += INT64_C(1000000000);
    }
    ticks = (uint64_t)seconds * TICRATE
          + (uint64_t)nanoseconds * TICRATE / UINT64_C(1000000000);
#endif
    return (int)(ticks & INT32_MAX);
}

void I_WaitVBL(int count)
{
    if (count <= 0)
        return;
#ifdef _WIN32
    Sleep((DWORD)((uint64_t)count * 1000 / 70));
#else
    struct timespec delay = {
        count / 70,
        (long)((int64_t)(count % 70) * INT64_C(1000000000) / 70)
    };
    while (nanosleep(&delay, &delay) && errno == EINTR) { }
#endif
}

void I_Init(void) { I_InitSound(); }
void I_BeginRead(void) { }
void I_EndRead(void) { }
void I_Tactile(int on, int off, int total)
{
    (void)on; (void)off; (void)total;
}

byte *I_AllocLow(int length)
{
    byte *memory;
    if (length < 0)
        I_Error("Invalid allocation size");
    memory = calloc((size_t)length, 1);
    if (!memory)
        I_Error("Could not allocate memory");
    return memory;
}

void I_Quit(void)
{
    D_QuitNetGame();
    M_SaveDefaults();
    /* Renderer owns the storm audio stream. Release it while SDL audio is
       still alive, then unregister music before closing the mixer stream. */
    I_ShutdownGraphics();
    I_ShutdownMusic();
    I_ShutdownSound();
    exit(EXIT_SUCCESS);
}

void I_Error(char *error, ...)
{
    va_list args;
    va_start(args, error);
#ifdef DOOM_NATIVE
    char message[1024];
    va_list display_args;
    va_copy(display_args, args);
    vsnprintf(message, sizeof(message), error, display_args);
    va_end(display_args);
#endif
    fputs("Error: ", stderr);
    vfprintf(stderr, error, args);
    fputc('\n', stderr);
    va_end(args);
#ifdef DOOM_NATIVE
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, Game_Title(), message, NULL);
#endif
    I_ShutdownGraphics();
    I_ShutdownMusic();
    I_ShutdownSound();
    exit(EXIT_FAILURE);
}
