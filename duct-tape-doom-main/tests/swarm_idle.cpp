#include <cstdio>
#include <cstdlib>
extern "C" {
#include "doomdef.h"
#include "doomstat.h"
#include "p_local.h"
#include "p_swarm.h"
#include "m_argv.h"
void A_Look(mobj_t*);
}
static void check(bool ok,const char *name){if(!ok){std::fprintf(stderr,"FAIL %s\n",name);std::exit(1);}std::printf("PASS %s\n",name);}
int main(){
    char name[]="swarm-test";char *args[]={name,nullptr};myargc=1;myargv=args;
    sector_t sector={};subsector_t sub={};sub.sector=&sector;
    state_t idle={};idle.action.acp1=(actionf_p1)A_Look;
    mobj_t actor={},player={};actor.subsector=player.subsector=&sub;actor.state=&idle;
    actor.flags=MF_COUNTKILL|MF_SHOOTABLE;actor.health=player.health=100;actor.height=player.height=56*FRACUNIT;
    players[0].mo=&player;players[0].health=100;players[0].playerstate=PST_LIVE;playeringame[0]=true;
    P_SwarmBeginLevel();P_SwarmConfigure(1,0);
    check(!P_SwarmIdleSkip(&actor),"cold entries run normal AI");
    P_SwarmIdleNegative(&actor,1);check(!P_SwarmIdleSkip(&actor),"cursor-only negative cannot hibernate");
    P_SwarmIdleNegative(&actor,0);check(P_SwarmIdleSkip(&actor),"unchanged confirmed idle search is reused");
    ++player.x;check(!P_SwarmIdleSkip(&actor),"player movement wakes the search");
    P_SwarmIdleNegative(&actor,0);++actor.angle;check(!P_SwarmIdleSkip(&actor),"monster turn wakes the search");
    P_SwarmIdleNegative(&actor,0);sector.soundtarget=&player;check(!P_SwarmIdleSkip(&actor),"sound alert bypasses hibernation");sector.soundtarget=nullptr;
    P_SwarmIdleNegative(&actor,0);actor.target=&player;check(!P_SwarmIdleSkip(&actor),"engaged monsters stay active");actor.target=nullptr;
    P_SwarmIdleNegative(&actor,0);actor.momx=1;check(!P_SwarmIdleSkip(&actor),"moving monsters stay active");actor.momx=0;
    P_SwarmIdleNegative(&actor,0);P_SwarmWorldChanged();check(!P_SwarmIdleSkip(&actor),"moving world geometry invalidates all entries");
    P_SwarmIdleNegative(&actor,0);--actor.health;check(!P_SwarmIdleSkip(&actor),"damage invalidates the negative result");
    P_SwarmIdleNegative(&actor,0);P_SwarmForget(&actor);check(!P_SwarmIdleSkip(&actor),"removed actors leave no reusable entry");
    P_SwarmIdleNegative(&actor,0);netgame=true;check(!P_SwarmIdleSkip(&actor),"network exclusion");netgame=false;
    demoplayback=true;check(!P_SwarmIdleSkip(&actor),"playback exclusion");demoplayback=false;
    demorecording=true;check(!P_SwarmIdleSkip(&actor),"recording exclusion");demorecording=false;
    respawnmonsters=true;check(!P_SwarmIdleSkip(&actor),"nightmare respawn exclusion");respawnmonsters=false;
    P_SwarmConfigure(0,0);check(!P_SwarmIdleSkip(&actor),"off restores normal polling");
    P_SwarmConfigure(1,0);check(!P_SwarmIdleSkip(&actor),"reenabling starts with an empty logical cache");
    P_SwarmIdleNegative(&actor,0);P_SwarmBeginLevel();check(!P_SwarmIdleSkip(&actor),"reload discards runtime history");
    char mode[]="-solidmaps";char *solidArgs[]={name,mode,nullptr};myargc=2;myargv=solidArgs;P_SwarmBeginLevel();
    P_SwarmIdleNegative(&actor,0);check(!P_SwarmIdleSkip(&actor),"solid geometry excluded");
    myargc=1;myargv=args;P_SwarmBeginLevel();P_SwarmPatchedDefinitions();P_SwarmIdleNegative(&actor,0);
    check(!P_SwarmIdleSkip(&actor),"modified gameplay definitions fall back to normal AI");
}
