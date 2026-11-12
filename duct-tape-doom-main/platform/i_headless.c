/* Test/demo backend: no window, input, audio device, or network access. */
#include <stdlib.h>
#include <string.h>
#include "doomdef.h"
#include "doomstat.h"
#include "i_system.h"
#include "i_sound.h"
#include "i_video.h"
#include "i_net.h"
#include "m_argv.h"
#include "v_video.h"
#include "w_wad.h"
#include "p_local.h"
#include "p_tick.h"
#include "z_zone.h"
#include "r_main.h"
#include "r_things.h"
#include "g_game.h"
#include <stdio.h>

#include "../tests/boom_checks.inc"

void I_InitGraphics(void) {}
void I_ShutdownGraphics(void) {}
void I_StartFrame(void)
{
    if (gamestate == GS_LEVEL && players[0].mo)
    {
        if (M_CheckParm("-checkbigmap"))
        {
            if (M_CheckParm("-checkboom"))
            {
                B_Run();
                fflush(stdout);
                exit(0);
            }
            extern void R_ValidateTextureComposites(void);
            if (M_CheckParm("-checkloaded") && leveltime < 35)
                I_Error("Bigmap save did not restore level time");
            for (thinker_t *t = thinkercap.next; t != &thinkercap; t = t->next)
                if (t->function.acp1 == (actionf_p1)P_MobjThinker)
                {
                    mobj_t *m = (mobj_t *)t;
                    if (m->sprite == NUMSPRITES_VANILLA)
                        continue; // TNT1 is deliberately invisible
                    if (m->sprite < 0 || m->sprite >= numsprites || (m->frame & FF_FRAMEMASK) >= sprites[m->sprite].numframes)
                        I_Error("Bigmap has missing sprite artwork for actor %d frame %d", m->type, m->frame & FF_FRAMEMASK);
                }
            R_ValidateTextureComposites();
            if (M_CheckParm("-checkrender"))
                R_RenderPlayerView(&players[0]);
            for (int i = 0; i < 35; i++)
                P_Ticker();
            Z_CheckHeap();
            size_t used, purgeable, available;
            Z_Usage(&used, &purgeable, &available);
            printf("Zone bytes: used=%zu cache=%zu free=%zu\n", used, purgeable, available);
            if (M_CheckParm("-checksave"))
            {
                extern void G_DoSaveGame(void);
                G_SaveGame(0, "Bigmap verification");
                G_DoSaveGame();
            }

            printf("PASS bigmap: MAP%02d vertices=%d segs=%d subsectors=%d nodes=%d sectors=%d 35 tics\n",
                   gamemap, numvertexes, numsegs, numsubsectors, numnodes, numsectors);
            fflush(stdout);
            exit(0);
        }
        if (getenv("DERP_CHECK_MAP_SOURCE"))
        {
            extern int SV_CheckMapSource(void);
            exit(SV_CheckMapSource() ? 0 : 1);
        }
        const char *path = getenv("DERP_EXPORT_VOLUME");
        if (path)
        {
            extern void P_ExportSolidFixture(const char *);
            P_ExportSolidFixture(path);
            exit(0);
        }
    }
}
void I_StartTic(void) {}
void I_UpdateNoBlit(void) {}
void I_FinishUpdate(void) {}
void I_SetPalette(byte *palette) { (void)palette; }
void I_ReadScreen(byte *screen)
{
    memcpy(screen, screens[0], SCREENWIDTH * SCREENHEIGHT);
}

void I_InitNetwork(void)
{
    if (M_CheckParm("-net"))
        I_Error("The headless backend supports single-player demos only");
    doomcom = calloc(1, sizeof(*doomcom));
    if (!doomcom)
        I_Error("Could not allocate network state");
    doomcom->id = DOOMCOM_ID;
    doomcom->ticdup = 1;
    doomcom->numplayers = doomcom->numnodes = 1;
    netgame = false;
}
void I_NetCmd(void) { I_Error("Network commands are unavailable in the headless backend"); }
#include "i_lan_unavailable.h"

void I_InitSound(void) {}
void I_ShutdownSound(void) {}
void I_UpdateSound(void) {}
void I_SubmitSound(void) {}
void I_SetChannels(void) {}
int I_GetSfxLumpNum(sfxinfo_t *sfx)
{
    char name[9];
    snprintf(name, sizeof(name), "ds%s", sfx->name);
    return W_GetNumForName(name);
}
int I_StartSound(int id, int vol, int sep, int pitch, int priority)
{
    (void)id;
    (void)vol;
    (void)sep;
    (void)pitch;
    (void)priority;
    return 0;
}
void I_StopSound(int handle) { (void)handle; }
int I_SoundIsPlaying(int handle)
{
    (void)handle;
    return 0;
}
void I_UpdateSoundParams(int handle, int vol, int sep, int pitch)
{
    (void)handle;
    (void)vol;
    (void)sep;
    (void)pitch;
}
void I_SoundAt(int handle, fixed_t x, fixed_t y, fixed_t z)
{
    (void)handle;
    (void)x;
    (void)y;
    (void)z;
}
fixed_t I_SoundReach(int id)
{
    (void)id;
    return FRACUNIT;
}
void I_InitMusic(void) {}
void I_ShutdownMusic(void) {}
void I_SetMusicVolume(int volume) { (void)volume; }
void I_PauseSong(int handle) { (void)handle; }
void I_ResumeSong(int handle) { (void)handle; }
int I_RegisterSong(void *data)
{
    (void)data;
    return 0;
}
void I_PlaySong(int handle, int looping)
{
    (void)handle;
    (void)looping;
}
void I_StopSong(int handle) { (void)handle; }
void I_UnRegisterSong(int handle) { (void)handle; }

/* Camera placement is unavailable without the 3D renderer. */
static float director_pitch;
float I_DirectorPitch(void) { return director_pitch; }
void I_DirectorSetPitch(float pitch) { director_pitch = pitch; }
int I_DirectorPlace(float *out)
{
    (void)out;
    return 0;
}
int I_TestDirectorClosePlacement(void) { return 0; }
int I_TestDirectorNoBob(void) { return 0; }
