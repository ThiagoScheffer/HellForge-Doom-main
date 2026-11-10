/* Actual activation/movement/archive functions on a tiny isolated map graph.
 * Included only by the headless verification backend. */
#include "p_boom.h"
#include "p_saveg.h"
#include "m_bbox.h"
static int boomChecks;
static void B_Check(int ok,const char *what){if(!ok)I_Error("Boom regression: %s",what);++boomChecks;}
static void B_Clear(void){
    thinker_t *t=thinkercap.next;
    while(t!=&thinkercap){thinker_t *next=t->next;Z_Free(t);t=next;}
    P_InitThinkers();
    memset(activeceilings,0,sizeof(activeceilings));memset(activeplats,0,sizeof(activeplats));
}
static void B_Reset(sector_t *s){
    B_Clear();
    for(int i=0;i<3;i++){
        s[i].floorheight=i*64*FRACUNIT;s[i].ceilingheight=(128+i*64)*FRACUNIT;
        s[i].floordata=s[i].specialdata=NULL;s[i].floorpic=0;s[i].special=0;
    }
    P_BoomSpawn();p_boom_compat=1;
}
static void B_Run(void){
    sector_t *savedSectors=sectors;line_t *savedLines=lines;side_t *savedSides=sides;
    int savedNS=numsectors,savedNL=numlines,savedND=numsides;
    thinker_t savedThinkers=thinkercap;player_t *savedView=viewplayer;
    int savedCompat=p_boom_compat;
    ceiling_t *savedCeilings[MAXCEILINGS];plat_t *savedPlats[MAXPLATS];
    memcpy(savedCeilings,activeceilings,sizeof(savedCeilings));memcpy(savedPlats,activeplats,sizeof(savedPlats));
    P_InitThinkers();
    sector_t testSectors[3]={0};line_t testLines[2]={0};side_t testSides[4]={0};
    sector_t *s=testSectors;line_t *l=testLines;side_t *sd=testSides;
    line_t *edges0[]={&l[0]},*edges1[]={&l[0],&l[1]},*edges2[]={&l[1]};
    sectors=s;numsectors=3;lines=l;numlines=2;sides=sd;numsides=4;
    for(int i=0;i<3;i++){s[i].blockbox[BOXTOP]=-1;s[i].blockbox[BOXRIGHT]=-1;}
    s[0].lines=edges0;s[0].linecount=1;s[1].lines=edges1;s[1].linecount=2;s[2].lines=edges2;s[2].linecount=1;s[1].tag=7;
    for(int i=0;i<2;i++){
        l[i].frontsector=&s[i];l[i].backsector=&s[i+1];l[i].flags=ML_TWOSIDED;
        l[i].sidenum[0]=i*2;l[i].sidenum[1]=i*2+1;sd[i*2].sector=&s[i];sd[i*2+1].sector=&s[i+1];
    }
    player_t player={0};mobj_t actor={0};subsector_t sub={0};sub.sector=&s[1];
    actor.player=&player;actor.subsector=&sub;player.mo=&actor;viewplayer=&player;
    line_t trigger=l[0];trigger.tag=7;
    /* 32 native floor movers: every speed, direction and change mode. */
    for(int speed=0;speed<4;speed++)for(int direction=0;direction<2;direction++)for(int change=0;change<4;change++){
        B_Reset(s);s[0].floorpic=1;s[0].special=7;
        trigger.special=0x6000|(6<<7)|(direction<<6)|(speed<<3)|(change<<10);
        B_Check(P_BoomActivate(&trigger,&actor,0,0)==1,"walk floor activation");
        B_Check(trigger.special==0,"walk-once cleared");
        floormove_t *f=s[1].floordata;
        B_Check(f&&f->speed==(FRACUNIT<<speed),"floor speed");
        for(int tic=0;tic<220&&s[1].floordata;tic++)T_MoveFloor(f);
        B_Check(s[1].floorheight==(direction?88:40)*FRACUNIT,"floor destination reached");
        B_Check(!s[1].floordata,"floor released");
        B_Check(s[1].floorpic==(change?1:0),"floor texture transfer");
        B_Check(s[1].special==(change==3?7:0),"floor damage-special transfer");
    }
    for(int speed=0;speed<4;speed++)for(int direction=0;direction<2;direction++){
        B_Reset(s);trigger.special=0x4000|(6<<7)|(direction<<6)|(speed<<3)|1;
        B_Check(P_BoomActivate(&trigger,&actor,0,0)==1,"ceiling activation");
        ceiling_t *c=s[1].specialdata;
        B_Check(c&&c->speed==(FRACUNIT<<speed),"ceiling speed");
        for(int tic=0;tic<220&&s[1].specialdata;tic++)T_MoveCeiling(c);
        B_Check(s[1].ceilingheight==(direction?216:168)*FRACUNIT,"ceiling destination");
        B_Check(!s[1].specialdata,"ceiling released");
    }
    for(int speed=0;speed<4;speed++)for(int target=0;target<4;target++){
        B_Reset(s);trigger.special=0x3400|(target<<8)|(speed<<3)|1;
        B_Check(P_BoomActivate(&trigger,&actor,0,0)==1,"lift activation");
        plat_t *plat=s[1].floordata;
        B_Check(plat&&plat->speed==(2*FRACUNIT<<speed),"lift speed");
        B_Check(plat->low==(target==2?64:0)*FRACUNIT&&plat->high==(target==3?128:64)*FRACUNIT,"lift endpoints");
        B_Check(plat->wait==35,"lift delay");
    }
    for(int speed=0;speed<4;speed++)for(int direction=0;direction<2;direction++){
        B_Reset(s);trigger.special=0x3000|(direction<<8)|(1<<6)|(speed<<3)|1;
        B_Check(P_BoomActivate(&trigger,&actor,0,0)==1,"stairs activation");
        floormove_t *first=s[1].floordata,*second=s[2].floordata;
        B_Check(first&&second&&first->floordestheight==(direction?72:56)*FRACUNIT,"stair first step");
        B_Check(second->floordestheight==(direction?80:48)*FRACUNIT,"stair chain height");
        B_Check((trigger.special&256)!=(direction<<8),"repeat stairs reverse direction");
        static const int rates[]={FRACUNIT/4,FRACUNIT/2,FRACUNIT*2,FRACUNIT*4};
        B_Check(first->speed==rates[speed],"stair speed calibration");
        s[2].floorheight=s[1].floorheight;
        for(int tic=0;tic<600&&s[1].floordata;tic++)T_MoveFloor(first);
        B_Check(s[1].stairlock!=0,"completed early stair remains locked");
        B_Check(P_BoomActivate(&trigger,&actor,0,0)==0,"stairs cannot retrigger mid-chain");
        for(int tic=0;tic<600&&s[2].floordata;tic++)T_MoveFloor(second);
        B_Check(!s[1].stairlock&&!s[2].stairlock,"last completed stair unlocks chain");
    }
    B_Reset(s);trigger.special=0x6000|(6<<7)|64|1;
    B_Check(P_BoomActivate(&trigger,&actor,1,1)==0,"walk trigger rejects use/backside");
    B_Check(P_BoomActivate(&trigger,&actor,0,0)==1,"floor starts");
    trigger.special=0x4000|(6<<7)|64|1;
    B_Check(P_BoomActivate(&trigger,&actor,0,0)==1,"ceiling can move with floor");
    B_Check(s[1].floordata&&s[1].specialdata,"independent movers");
    B_Reset(s);trigger.special=0x3800|(1<<6)|6;
    B_Check(!P_BoomActivate(&trigger,&actor,0,1),"locked door rejects missing red card");
    player.cards[it_redskull]=1;
    B_Check(!P_BoomActivate(&trigger,&actor,0,1),"exact card rejects skull");
    trigger.special|=512;
    B_Check(P_BoomActivate(&trigger,&actor,0,1)==1,"interchangeable red key accepts skull");
    B_Reset(s);trigger.special=233;
    B_Check(P_BoomActivate(&trigger,&actor,0,1)==1,"elevator switch");
    floormove_t *elevator=s[1].floordata;
    B_Check(elevator&&s[1].specialdata==elevator,"elevator owns both planes");
    byte *archive=calloc(1,65536);save_p=archive;P_ArchiveSpecials();B_Clear();
    s[1].specialdata=s[1].floordata=NULL;save_p=archive;P_UnArchiveSpecials();
    elevator=s[1].floordata;
    B_Check(elevator&&elevator->thinker.function.acp1==(actionf_p1)T_BoomElevator,"elevator archive restored action");
    for(int tic=0;tic<40&&s[1].floordata;tic++)T_BoomElevator(elevator);
    B_Check(s[1].floorheight==0&&s[1].ceilingheight==128*FRACUNIT,"restored elevator preserves room height");free(archive);
    B_Reset(s);l[0].special=253;l[0].tag=7;l[0].dx=32*FRACUNIT;
    P_BoomSpawn();actor.player=NULL;actor.z=actor.floorz=s[1].floorheight;actor.flags=0;actor.momx=actor.momy=0;s[1].thinglist=&actor;
    P_BoomTick();
    B_Check(s[1].floor_xoffs==-FRACUNIT,"floor scroller");
    B_Check(actor.momx==3*FRACUNIT/32,"conveyor carries actor");
    sd[0].textureoffset=FRACUNIT/3;
    size_t bytes=P_BoomSave(NULL);byte *state=malloc(bytes);P_BoomSave(state);P_BoomTick();sd[0].textureoffset=0;
    if(M_CheckParm("-checkboomcorrupt")){state[bytes-12]=99;P_BoomLoad(state,bytes);I_Error("Corrupt Boom save unexpectedly accepted");}
    B_Check(P_BoomLoad(state,bytes)==bytes,"versioned scroller extension");
    B_Check(s[1].floor_xoffs==-FRACUNIT&&sd[0].textureoffset==FRACUNIT/3,"fractional scroll state restored");
    B_Check(P_BoomLoad(state,8)==0,"legacy save has no Boom extension");
    size_t oldBytes=bytes-(size_t)numsectors*4;byte *oldState=malloc(oldBytes);memcpy(oldState,state,12);
    size_t cursor=12;
    for(int i=0;i<numsectors;i++){memcpy(oldState+cursor,state+12+i*20,16);cursor+=16;}
    memcpy(oldState+cursor,state+12+numsectors*20,bytes-12-numsectors*20);
    oldState[oldBytes-12]=1;
    for(int i=0;i<4;i++)oldState[oldBytes-8+i]=(byte)(oldBytes>>(i*8));
    B_Check(P_BoomLoad(oldState,oldBytes)==oldBytes,"version 1 Boom save remains readable");
    free(oldState);free(state);
    s[1].thinglist=NULL;l[0].special=242;P_BoomSpawn();viewz=100*FRACUNIT;
    sector_t copy;sector_t *visual=P_BoomRenderSector(&s[1],&copy,0);
    B_Check(visual!=&s[1]&&visual->floorheight==0&&s[1].floorheight==64*FRACUNIT,"fake floor changes drawing only");
    /* Teleport the real mobj through the gameplay collision path. The
     * isolated one-subsector fixture has no sector/blockmap links to mutate. */
    B_Reset(s);actor.player=&player;actor.flags=MF_NOBLOCKMAP|MF_NOSECTOR;
    int savedNodes=numnodes,savedSubs=numsubsectors;subsector_t *savedSubsectors=subsectors;
    numnodes=0;numsubsectors=1;subsectors=&sub;sub.sector=&s[1];
    mobj_t *destination=Z_Malloc(sizeof(*destination),PU_LEVEL,NULL);memset(destination,0,sizeof(*destination));
    destination->type=MT_TELEPORTMAN;destination->subsector=&sub;destination->x=100*FRACUNIT;destination->angle=ANG90;
    destination->thinker.function.acp1=(actionf_p1)P_MobjThinker;P_AddThinker(&destination->thinker);
    actor.x=actor.y=0;actor.z=actor.floorz=64*FRACUNIT;actor.momx=5*FRACUNIT;actor.momy=0;actor.angle=0;
    trigger.special=208;trigger.dx=32*FRACUNIT;trigger.dy=0;
    B_Check(P_BoomActivate(&trigger,&actor,1,0)==0,"silent teleport rejects backside");
    B_Check(P_BoomActivate(&trigger,&actor,0,0)==1,"silent thing teleport contact");
    B_Check(actor.x==100*FRACUNIT&&actor.angle==0&&actor.momx>4*FRACUNIT,"silent teleport position/orientation/momentum");
    vertex_t points[4]={{0,0},{32*FRACUNIT,0},{100*FRACUNIT,64*FRACUNIT},{132*FRACUNIT,64*FRACUNIT}};
    l[0].v1=&points[0];l[0].v2=&points[1];l[1].v1=&points[2];l[1].v2=&points[3];
    l[0].dx=l[1].dx=32*FRACUNIT;l[0].dy=l[1].dy=0;l[0].tag=l[1].tag=7;l[0].special=244;
    actor.x=16*FRACUNIT;actor.y=0;actor.z=actor.floorz=64*FRACUNIT;actor.angle=0;actor.momx=5*FRACUNIT;actor.momy=0;
    B_Check(P_BoomActivate(&l[0],&actor,0,0)==1,"silent line teleport contact");
    B_Check(actor.x==116*FRACUNIT&&actor.angle==ANG180&&actor.momx< -4*FRACUNIT,"silent line interpolation/momentum");
    numnodes=savedNodes;numsubsectors=savedSubs;subsectors=savedSubsectors;
    B_Clear();
    sectors=savedSectors;lines=savedLines;sides=savedSides;numsectors=savedNS;numlines=savedNL;numsides=savedND;
    thinkercap=savedThinkers;viewplayer=savedView;p_boom_compat=savedCompat;
    memcpy(activeceilings,savedCeilings,sizeof(savedCeilings));memcpy(activeplats,savedPlats,sizeof(savedPlats));
    printf("PASS Boom action regressions: %d checks\n",boomChecks);
}
