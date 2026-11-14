#ifndef HELLFORGE_MONSTER_WAKE_AUDIT_H
#define HELLFORGE_MONSTER_WAKE_AUDIT_H
enum { WA_AMBUSH, WA_PLAYERS, WA_MISSILE, WA_MELEE, WA_SOLID, WA_SITES };
#ifdef DOOM_MONSTER_BENCHMARK
#ifdef __cplusplus
extern "C" {
#endif
struct mobj_s; struct line_s;
void WA_Init(void);
void WA_MeasureBegin(void);
void WA_Map(void);
void WA_Spawn(struct mobj_s*);
void WA_Remove(struct mobj_s*);
void WA_LookBegin(struct mobj_s*);
void WA_LookEnd(void);
void WA_Transition(struct mobj_s*,int sound);
int WA_Sight(struct mobj_s*,struct mobj_s*,int site,int reused);
void WA_SolidTime(unsigned long long ns);
unsigned long long WA_SolidStart(void);
void WA_SolidEnd(unsigned long long start);
void WA_Shot(void);
void WA_Teleport(struct mobj_s*,struct line_s*,int success);
void WA_Flush(const char *folder);
#ifdef __cplusplus
}
struct WA_SolidScope {
    unsigned long long start;
    WA_SolidScope():start(WA_SolidStart()){}
    ~WA_SolidScope(){WA_SolidEnd(start);}
};
#endif
#else
#define WA_Map() ((void)0)
#define WA_Spawn(a) ((void)0)
#define WA_Remove(a) ((void)0)
#define WA_LookBegin(a) ((void)0)
#define WA_LookEnd() ((void)0)
#define WA_Transition(a,s) ((void)0)
#define WA_Teleport(a,l,s) ((void)0)
#endif
#endif
