/* Boom-compatible map actions. Bit assignments and movement semantics follow
 * the Boom reference distributed with PrBoom+ (GPL-2.0-or-later):
 * https://github.com/coelckers/prboom-plus/tree/master/prboom2
 * Vanilla thinker layouts are retained so ordinary save archives still work.
 */
#include <stdlib.h>
#include <string.h>
#include "doomdef.h"
#include "doomstat.h"
#include "p_local.h"
#include "p_boom.h"
#include "z_zone.h"
#include "s_sound.h"
#include "sounds.h"
#include "i_system.h"
#include "m_random.h"
#include "g_game.h"
#include "r_main.h"
#include "r_sky.h"
#include <math.h>
#include <limits.h>
#include "m_bbox.h"
extern void P_CalcHeight(player_t *player);

int p_boom_compat;
static fixed_t bounded(int64_t value){
    if(value>32000LL*FRACUNIT)value=32000LL*FRACUNIT;
    if(value< -32000LL*FRACUNIT)value= -32000LL*FRACUNIT;
    return (fixed_t)value;
}
static fixed_t adjacent(sector_t *s,int ceiling,int direction,int nearest){
    fixed_t current=ceiling?s->ceilingheight:s->floorheight,result=current;
    int found=0;
    for(int i=0;i<s->linecount;i++){
        sector_t *o=getNextSector(s->lines[i],s);if(!o)continue;
        fixed_t h=ceiling?o->ceilingheight:o->floorheight;
        if(nearest && (direction>0?h<=current:h>=current))continue;
        if(!found || (nearest?(direction>0?h<result:h>result):(direction>0?h>result:h<result)))result=h;
        found=1;
    }
    return result;
}
static fixed_t shortest(sector_t *s,int upper){
    fixed_t best=32000*FRACUNIT;
    for(int i=0;i<s->linecount;i++)if(s->lines[i]->backsector){
        for(int side=0;side<2;side++){
            side_t *sd=&sides[s->lines[i]->sidenum[side]];
            int t=upper?sd->toptexture:sd->bottomtexture;
            if(t>0 && textureheight[t]<best)best=textureheight[t];
        }
    }
    return best;
}
static fixed_t destination(sector_t *s,int ceiling,unsigned value){
    int direction=(value&64)?1:-1,target=(value>>7)&7;
    fixed_t h=ceiling?s->ceilingheight:s->floorheight;
    switch(target){
        case 0:return adjacent(s,ceiling,1,0);
        case 1:return adjacent(s,ceiling,-1,0);
        case 2:return adjacent(s,ceiling,direction,1);
        case 3:return adjacent(s,!ceiling,ceiling?1:-1,0);
        case 4:return ceiling?s->floorheight:s->ceilingheight;
        case 5:return bounded((int64_t)h+direction*(int64_t)shortest(s,ceiling));
        case 6:return bounded((int64_t)h+direction*24LL*FRACUNIT);
        default:return bounded((int64_t)h+direction*32LL*FRACUNIT);
    }
}
static void startFloor(sector_t *s,fixed_t dest,fixed_t speed,int direction,int crush,sector_t *model,int change){
    floormove_t *f=Z_Malloc(sizeof(*f),PU_LEVSPEC,0);memset(f,0,sizeof(*f));
    f->sector=s;f->speed=speed;f->direction=direction;f->floordestheight=dest;f->crush=crush;
    f->type=direction>0?raiseFloor:lowerFloor;
    if(change && model){
        f->texture=model->floorpic;
        f->newspecial=change==1?0:(change==3?model->special:s->special);
        f->type=direction>0?donutRaise:lowerAndChange;
    }
    f->thinker.function.acp1=(actionf_p1)T_MoveFloor;s->floordata=f;P_AddThinker(&f->thinker);
}
static sector_t *modelFor(line_t *l,sector_t *s,fixed_t dest,unsigned v){
    if(!(v&32))return l->frontsector;
    int target=(v>>7)&7;
    for(int i=0;i<s->linecount;i++){
        sector_t *o=getNextSector(s->lines[i],s);
        if(o && ((target==3||target==4)?o->ceilingheight:o->floorheight)==dest)return o;
    }
    return NULL;
}
static int unlocked(mobj_t *actor,unsigned v){
    if(!actor->player)return 0;
    player_t *p=actor->player;int key=(v>>6)&7,either=(v&512)!=0;
    int red=p->cards[it_redcard]||p->cards[it_redskull];
    int blue=p->cards[it_bluecard]||p->cards[it_blueskull];
    int yellow=p->cards[it_yellowcard]||p->cards[it_yellowskull];
    int ok=0;
    switch(key){
        case 0:ok=red||blue||yellow;break;
        case 1:ok=either?red:p->cards[it_redcard];break;
        case 2:ok=either?blue:p->cards[it_bluecard];break;
        case 3:ok=either?yellow:p->cards[it_yellowcard];break;
        case 4:ok=either?red:p->cards[it_redskull];break;
        case 5:ok=either?blue:p->cards[it_blueskull];break;
        case 6:ok=either?yellow:p->cards[it_yellowskull];break;
        case 7:ok=either?(red&&blue&&yellow):(p->cards[0]&&p->cards[1]&&p->cards[2]&&p->cards[3]&&p->cards[4]&&p->cards[5]);break;
    }
    if(!ok){p->message="You need a key to open this door";S_StartSound(actor,sfx_oof);}
    return ok;
}
static int generalized(line_t *l,mobj_t *actor){
    unsigned v=(unsigned)l->special;int manual=(v&7)>=6,success=0;
    if(v>=0x4000 && v<0x6000 && (v&0xc00)){
        /* No such actions occur in the target WAD. Do not execute a subtly
         * different ceiling transfer until its save metadata is implemented. */
        I_Error("Unsupported Boom ceiling texture transfer: %u",v);
    }
    if(v>=0x3800&&v<0x3c00&&!unlocked(actor,v))return 0;
    if(v>=0x3400&&v<0x3800&&((v>>8)&3)==3)P_ActivateInStasis(l->tag);
    for(int index=0;index<numsectors;index++){
        sector_t *s=&sectors[index];
        if(manual?s!=l->backsector:s->tag!=l->tag)continue;
        if(!manual&&!l->tag)continue;
        int speed=(v>>3)&3,direction=(v&64)?1:-1;
        if(v>=0x6000){
            if(s->floordata)continue;
            fixed_t dest=destination(s,0,v);
            startFloor(s,dest,FLOORSPEED<<speed,direction,(v&0x1000)!=0,modelFor(l,s,dest,v),(v>>10)&3);success=1;
        }else if(v>=0x4000){
            if(s->specialdata)continue;
            ceiling_t *c=Z_Malloc(sizeof(*c),PU_LEVSPEC,0);memset(c,0,sizeof(*c));
            c->sector=s;c->direction=direction;c->speed=CEILSPEED<<speed;c->crush=(v&0x1000)!=0;c->tag=l->tag;
            c->type=direction>0?raiseToHighest:lowerToFloor;
            c->topheight=c->bottomheight=destination(s,1,v);
            c->thinker.function.acp1=(actionf_p1)T_MoveCeiling;s->specialdata=c;
            P_AddThinker(&c->thinker);P_AddActiveCeiling(c);success=1;
        }else if(v>=0x3800){
            if(s->specialdata)continue;
            int locked=v<0x3c00,kind=locked?((v&32)?1:0):(v>>5)&3;
            vldoor_t *d=Z_Malloc(sizeof(*d),PU_LEVSPEC,0);memset(d,0,sizeof(*d));
            d->sector=s;d->speed=VDOORSPEED<<speed;
            static const int waits[]={35,150,300,1050};
            d->topwait=locked?150:waits[(v>>8)&3];
            d->topheight=kind==2?s->ceilingheight:P_FindLowestCeilingSurrounding(s)-4*FRACUNIT;
            d->type=kind==0?normal:kind==1?open:kind==2?boomCloseThenOpen:close;
            d->direction=kind>=2?-1:1;d->thinker.function.acp1=(actionf_p1)T_VerticalDoor;
            s->specialdata=d;P_AddThinker(&d->thinker);S_StartSound((mobj_t*)&s->soundorg,kind>=2?sfx_dorcls:sfx_doropn);success=1;
        }else if(v>=0x3400){
            if(s->floordata)continue;
            plat_t *p=Z_Malloc(sizeof(*p),PU_LEVSPEC,0);memset(p,0,sizeof(*p));
            int target=(v>>8)&3;static const int waits[]={35,105,175,350};
            p->sector=s;p->speed=(PLATSPEED*2)<<speed;p->wait=waits[(v>>6)&3];p->tag=l->tag;
            p->type=target==3?perpetualRaise:downWaitUpStay;
            p->low=target==1?adjacent(s,0,-1,1):target==2?adjacent(s,1,-1,0):P_FindLowestFloorSurrounding(s);
            if(p->low>s->floorheight)p->low=s->floorheight;
            p->high=target==3?P_FindHighestFloorSurrounding(s):s->floorheight;
            if(p->high<s->floorheight)p->high=s->floorheight;
            p->status=target==3?(P_Random()&1):down;
            p->thinker.function.acp1=(actionf_p1)T_PlatRaise;s->floordata=p;P_AddThinker(&p->thinker);P_AddActivePlat(p);success=1;
        }else if(v>=0x3000){
            if(s->floordata||s->stairlock)continue;
            int group=index+1;
            int dir=(v&256)?1:-1,texture=s->floorpic;static const int steps[]={4,8,16,24};
            fixed_t step=steps[(v>>6)&3]*FRACUNIT,height=s->floorheight;
            static const fixed_t rates[]={FLOORSPEED/4,FLOORSPEED/2,FLOORSPEED*2,FLOORSPEED*4};
            fixed_t rate=rates[speed];
            for(int count=0;s&&count<numsectors;count++){
                height=bounded((int64_t)height+dir*(int64_t)step);
                startFloor(s,height,rate,dir,0,NULL,0);success=1;
                s->stairlock=group;((floormove_t*)s->floordata)->type=boomBuildStairs;((floormove_t*)s->floordata)->newspecial=group;
                sector_t *next=NULL;
                for(int j=0;j<s->linecount;j++){
                    line_t *edge=s->lines[j];sector_t *other=edge->backsector;
                    if(edge->frontsector==s&&other&&!other->floordata&&!other->stairlock&&((v&512)||other->floorpic==texture)){next=other;break;}
                }
                s=next;
            }
        }else{
            if(s->specialdata)continue;
            ceiling_t *c=Z_Malloc(sizeof(*c),PU_LEVSPEC,0);memset(c,0,sizeof(*c));
            c->sector=s;c->speed=CEILSPEED<<speed;c->crush=1;c->direction=-1;c->tag=l->tag;
            c->type=(v&64)?silentCrushAndRaise:(speed>=2?fastCrushAndRaise:crushAndRaise);
            c->topheight=s->ceilingheight;c->bottomheight=s->floorheight+8*FRACUNIT;
            c->thinker.function.acp1=(actionf_p1)T_MoveCeiling;s->specialdata=c;
            P_AddThinker(&c->thinker);P_AddActiveCeiling(c);success=1;
        }
        if(manual)break;
    }
    if(success && v>=0x3000 && v<0x3400 && (v&1))l->special^=256;
    return success;
}
/* An elevator uses the existing floor archive record: newspecial carries
 * its fixed ceiling/floor gap; the action pointer selects the mover. */
void T_BoomElevator(floormove_t *f){
    sector_t *s=f->sector;fixed_t old=s->floorheight;
    result_e result;
    if(f->direction<0){
        T_MovePlane(s,f->speed,f->floordestheight+f->newspecial,0,1,-1);
        result=T_MovePlane(s,f->speed,f->floordestheight,0,0,-1);
    }else{
        result=T_MovePlane(s,f->speed,f->floordestheight,0,0,1);
        T_MovePlane(s,f->speed,s->floorheight+f->newspecial,0,1,1);
    }
    if(s->ceilingheight-s->floorheight!=f->newspecial){
        s->floorheight=old;s->ceilingheight=old+f->newspecial;P_ChangeSector(s,0);
    }
    if(result==pastdest){s->floordata=s->specialdata=NULL;P_RemoveThinker(&f->thinker);}
}
static int lowerNearest(line_t *l,int elevator){
    int success=0;
    for(int i=0;i<numsectors;i++)if(l->tag&&sectors[i].tag==l->tag){
        sector_t *s=&sectors[i];if(s->floordata||(elevator&&s->specialdata))continue;
        fixed_t dest=adjacent(s,0,-1,1);
        startFloor(s,dest,elevator?4*FRACUNIT:FLOORSPEED,-1,0,NULL,0);
        if(elevator){floormove_t *f=s->floordata;f->newspecial=s->ceilingheight-s->floorheight;
            f->thinker.function.acp1=(actionf_p1)T_BoomElevator;s->specialdata=f;}
        success=1;
    }
    return success;
}
static void rotateTeleport(mobj_t *thing,angle_t angle,fixed_t height){
    fixed_t x=thing->momx,y=thing->momy,c=finecosine[angle>>ANGLETOFINESHIFT],s=finesine[angle>>ANGLETOFINESHIFT];
    thing->angle+=angle;thing->momx=FixedMul(x,c)-FixedMul(y,s);thing->momy=FixedMul(y,c)+FixedMul(x,s);
    thing->z=thing->floorz+height;
    if(thing->player&&thing->player->mo==thing){
        fixed_t delta=thing->player->deltaviewheight;thing->player->deltaviewheight=0;
        P_CalcHeight(thing->player);thing->player->deltaviewheight=delta;
    }
}
static int silentTeleport(line_t *l,int side,mobj_t *actor,int lineExit){
    if(side||(actor->flags&MF_MISSILE))return 0;
    fixed_t height=actor->z-actor->floorz;
    angle_t from=R_PointToAngle2(0,0,l->dx,l->dy);
    if(lineExit){
        for(int i=0;i<numlines;i++){
            line_t *dest=&lines[i];if(dest==l||dest->tag!=l->tag||!dest->backsector)continue;
            if(!l->dx&&!l->dy)return 0;
            fixed_t pos=llabs(l->dx)>llabs(l->dy)?FixedDiv(actor->x-l->v1->x,l->dx):FixedDiv(actor->y-l->v1->y,l->dy);
            fixed_t x=dest->v2->x-FixedMul(pos,dest->dx),y=dest->v2->y-FixedMul(pos,dest->dy);
            int stepdown=dest->frontsector->floorheight<dest->backsector->floorheight;
            int exitSide=actor->player&&stepdown;
            for(int fudge=0;fudge<10&&P_PointOnLineSide(x,y,dest)!=exitSide;fudge++){
                if(llabs(dest->dx)>llabs(dest->dy))y-=((dest->dx<0)!=exitSide)?-1:1;
                else x+=((dest->dy<0)!=exitSide)?-1:1;
            }
            if(!P_TeleportMove(actor,x,y))return 0;
            rotateTeleport(actor,ANG180+R_PointToAngle2(0,0,dest->dx,dest->dy)-from,height);
            actor->z=height+(stepdown?dest->backsector:dest->frontsector)->floorheight;
            if(actor->player&&actor->player->mo==actor)actor->player->viewz=actor->z+actor->player->viewheight;
            return 1;
        }
    }else for(int i=0;i<numsectors;i++)if(sectors[i].tag==l->tag){
        for(thinker_t *th=thinkercap.next;th!=&thinkercap;th=th->next){
            if(th->function.acp1!=(actionf_p1)P_MobjThinker)continue;
            mobj_t *dest=(mobj_t*)th;
            if(dest->type!=MT_TELEPORTMAN||dest->subsector->sector!=&sectors[i])continue;
            if(!P_TeleportMove(actor,dest->x,dest->y))return 0;
            rotateTeleport(actor,from-dest->angle+ANG90,height);return 1;
        }
    }
    return 0;
}
static int extended(line_t *l,mobj_t *actor,int side,int trigger){
    int v=l->special,success=0;
    switch(v){
        case 142:if(trigger!=0||!actor->player)return 0;success=EV_DoFloor(l,raiseFloor512);break;
        case 153:
            if(trigger!=0||!actor->player)return 0;
            for(int i=0;i<numsectors;i++)if(l->tag&&sectors[i].tag==l->tag){
                sectors[i].floorpic=l->frontsector->floorpic;sectors[i].special=l->frontsector->special;success=1;
            }break;
        case 159:if(trigger!=1||side||!actor->player)return 0;success=EV_DoFloor(l,lowerAndChange);break;
        case 219:if(trigger!=0||!actor->player)return 0;success=lowerNearest(l,0);break;
        case 221:if(trigger!=1||side||!actor->player)return 0;success=lowerNearest(l,0);break;
        case 233:if(trigger!=1||side||!actor->player)return 0;success=lowerNearest(l,1);break;
        case 208:if(trigger==0)return silentTeleport(l,side,actor,0);return 0;
        case 244:if(trigger==0)return silentTeleport(l,side,actor,1);return 0;
        case 268:if(trigger==0&&!actor->player)success=silentTeleport(l,side,actor,0);break;
        default:return 0; // spawn-time property, not an activatable line
    }
    if(success){if(trigger==1)P_ChangeSwitchTexture(l,0);else l->special=0;}
    return success;
}
void P_BoomStairFinished(int group){
    for(int i=0;i<numsectors;i++)if(sectors[i].stairlock==group&&sectors[i].floordata)return;
    for(int i=0;i<numsectors;i++)if(sectors[i].stairlock==group)sectors[i].stairlock=0;
}
int P_BoomActivate(line_t *l,mobj_t *actor,int side,int trigger){
    unsigned v=(unsigned)l->special;
    if(v<142)return -1;
    if(v<0x2f80 || v>=0x8000)return extended(l,actor,side,trigger);
    int type=v&7;
    if((trigger==0 && type>1)||(trigger==1 && type!=2&&type!=3&&type!=6&&type!=7)||(trigger==2 && type!=4&&type!=5))return 0;
    if(trigger==1 && side)return 0;
    if(actor->flags&MF_MISSILE)return 0;
    if(!actor->player){
        int allowed=v>=0x4000?(!(v&0xc00)&&(v&32)):
            v>=0x3c00?(v&128):v>=0x3800?0:(v&32);
        if(!allowed || (l->flags&ML_SECRET))return 0;
    }
    int success=generalized(l,actor);
    if(success){
        if(type>=2&&type<=5)P_ChangeSwitchTexture(l,type&1);
        else if(!(type&1))l->special=0;
    }
    return success;
}

/* Scrollers are map-derived records. Only their changing displacement and
 * velocity are saved; sector/index provenance is reconstructed from the map. */
typedef struct {int type,index,control,accel;fixed_t dx,dy,last,vx,vy;int neighborStart,neighborCount;} boom_scroll_t;
static boom_scroll_t *scrolls;
static int scrollCount,scrollCapacity;
static int *carryNeighbors,carryNeighborCount,carryNeighborCapacity;
static fixed_t (*sectorBounds)[4];
static void carryNeighbor(int sector){
    if(carryNeighborCount==carryNeighborCapacity){
        if(carryNeighborCapacity>INT_MAX/2)I_Error("Boom: too many conveyor contacts");
        carryNeighborCapacity=carryNeighborCapacity?carryNeighborCapacity*2:128;
        void *next=realloc(carryNeighbors,(size_t)carryNeighborCapacity*sizeof(*carryNeighbors));
        if(!next)I_Error("Boom: cannot allocate conveyor contacts");carryNeighbors=next;
    }
    carryNeighbors[carryNeighborCount++]=sector;
}
static void buildCarryNeighbors(void){
    int carries=0;for(int i=0;i<scrollCount;++i)if(scrolls[i].type==3){carries=1;break;}
    if(!carries)return;
    fixed_t radius=0;
    for(int i=0;i<NUMMOBJTYPES;++i)if(mobjinfo[i].radius>radius)radius=mobjinfo[i].radius;
    sectorBounds=calloc((size_t)numsectors,sizeof(*sectorBounds));
    if(numsectors&&!sectorBounds)I_Error("Boom: cannot allocate conveyor bounds");
    for(int i=0;i<numsectors;++i){
        M_ClearBox(sectorBounds[i]);
        for(int j=0;j<sectors[i].linecount;++j){line_t *l=sectors[i].lines[j];
            M_AddToBox(sectorBounds[i],l->v1->x,l->v1->y);M_AddToBox(sectorBounds[i],l->v2->x,l->v2->y);
        }
    }
    for(int i=0;i<scrollCount;++i)if(scrolls[i].type==3){
        boom_scroll_t *s=&scrolls[i];fixed_t *b=sectorBounds[s->index];s->neighborStart=carryNeighborCount;
        for(int j=0;j<numsectors;++j){fixed_t *other=sectorBounds[j];
            if(j==s->index||!sectors[j].linecount)continue;
            if((int64_t)other[BOXLEFT]>(int64_t)b[BOXRIGHT]+radius||(int64_t)other[BOXRIGHT]<(int64_t)b[BOXLEFT]-radius||
               (int64_t)other[BOXBOTTOM]>(int64_t)b[BOXTOP]+radius||(int64_t)other[BOXTOP]<(int64_t)b[BOXBOTTOM]-radius)continue;
            carryNeighbor(j);
        }
        s->neighborCount=carryNeighborCount-s->neighborStart;
    }
}
static fixed_t contactBound(int64_t v){return v<INT_MIN?INT_MIN:v>INT_MAX?INT_MAX:(fixed_t)v;}
static int touchesCarrySector(const mobj_t *mo,const sector_t *sector){
    if(mo->subsector->sector==sector)return 1;
    fixed_t box[4]={contactBound((int64_t)mo->y+mo->radius),contactBound((int64_t)mo->y-mo->radius),
                   contactBound((int64_t)mo->x-mo->radius),contactBound((int64_t)mo->x+mo->radius)};
    const fixed_t *bounds=sectorBounds[sector-sectors];
    if(box[BOXRIGHT]<=bounds[BOXLEFT]||box[BOXLEFT]>=bounds[BOXRIGHT]||box[BOXTOP]<=bounds[BOXBOTTOM]||box[BOXBOTTOM]>=bounds[BOXTOP])return 0;
    for(int i=0;i<sector->linecount;++i){line_t *line=sector->lines[i];
        if(box[BOXRIGHT]<=line->bbox[BOXLEFT]||box[BOXLEFT]>=line->bbox[BOXRIGHT]||
           box[BOXTOP]<=line->bbox[BOXBOTTOM]||box[BOXBOTTOM]>=line->bbox[BOXTOP])continue;
        if(P_BoxOnLineSide(box,line)==-1)return 1;
    }
    return 0;
}
static void carryThing(mobj_t *mo,sector_t *sector,fixed_t dx,fixed_t dy){
    int submerged=sector->heightsec>=0&&mo->z<sectors[sector->heightsec].floorheight;
    if((mo->flags&MF_NOCLIP)||(!submerged&&((mo->flags&MF_NOGRAVITY)||mo->z>sector->floorheight)))return;
    if(!touchesCarrySector(mo,sector))return;
    mo->momx=(fixed_t)((uint32_t)mo->momx+dx);mo->momy=(fixed_t)((uint32_t)mo->momy+dy);
}
static fixed_t sumHeights(int sector){return (fixed_t)((uint32_t)sectors[sector].floorheight+(uint32_t)sectors[sector].ceilingheight);}
static void addScroll(int type,int index,fixed_t dx,fixed_t dy,int control,int accel){
    if(scrollCount==scrollCapacity){
        if(scrollCapacity>=1048576)I_Error("Boom: too many scrollers");
        scrollCapacity=scrollCapacity?scrollCapacity*2:128;
        void *next=realloc(scrolls,(size_t)scrollCapacity*sizeof(*scrolls));
        if(!next)I_Error("Boom: cannot allocate scrollers");scrolls=next;
    }
    boom_scroll_t *s=&scrolls[scrollCount++];memset(s,0,sizeof(*s));
    s->type=type;s->index=index;s->dx=dx;s->dy=dy;s->control=control;s->accel=accel;
    if(control>=0)s->last=sumHeights(control);
}
static int passiveSpecial(int value){
    return value==208||value==213||(value>=214&&value<=218)||value==219||value==221||value==233||value==242||value==244||
        (value>=245&&value<=255)||value==260||value==261||value==268||value==271||value==272||value==142||value==153||value==159;
}
void P_BoomSpawn(void){
    free(carryNeighbors);carryNeighbors=NULL;carryNeighborCount=carryNeighborCapacity=0;
    free(sectorBounds);sectorBounds=NULL;
    free(scrolls);scrolls=NULL;scrollCount=scrollCapacity=0;p_boom_compat=0;
    for(int i=0;i<numsectors;i++){
        sector_t *s=&sectors[i];s->heightsec=s->floorlightsec=s->ceilinglightsec=-1;s->skyline=0;
        s->floor_xoffs=s->floor_yoffs=s->ceiling_xoffs=s->ceiling_yoffs=0;s->stairlock=0;
        if(s->special>=32)p_boom_compat=1;
    }
    for(int i=0;i<numlines;i++){
        line_t *l=&lines[i];int v=l->special;
        if(v<142)continue;p_boom_compat=1;
        if(v<0x2f80&&!passiveSpecial(v))fprintf(stderr,"Boom: unsupported line special %d on line %d\n",v,i);
        if(l->sidenum[0]<0||!l->frontsector)I_Error("Boom: line %d has no front sidedef",i);
        int source=(int)(l->frontsector-sectors);
        if(v==242||v==213||v==261||v==271||v==272){
            for(int j=0;j<numsectors;j++)if(l->tag&&sectors[j].tag==l->tag){
                if(v==242)sectors[j].heightsec=source;
                if(v==213)sectors[j].floorlightsec=source;
                if(v==261)sectors[j].ceilinglightsec=source;
                if(v==271||v==272)sectors[j].skyline=(v==272?-1:1)*(i+1);
            }
        }
        if(v==260){
            /* Translucent midtextures: rendering property, never collision. */
            l->tranlump=1;
            for(int j=0;j<numlines;j++)if(l->tag&&lines[j].tag==l->tag)lines[j].tranlump=1;
        }
        int control=-1,accel=0;
        if(v>=245&&v<=249){v+=5;control=source;}
        else if(v>=214&&v<=218){v+=36;control=source;accel=1;}
        fixed_t dx=l->dx>>5,dy=l->dy>>5;
        if(v==255){side_t *side=&sides[l->sidenum[0]];addScroll(0,l->sidenum[0],-side->textureoffset,side->rowoffset,-1,0);}
        else if(v>=250&&v<=253){
            for(int j=0;j<numsectors;j++)if(l->tag&&sectors[j].tag==l->tag){
                if(v==250)addScroll(2,j,-dx,dy,control,accel);
                if(v==251||v==253)addScroll(1,j,-dx,dy,control,accel);
                if(v==252||v==253)addScroll(3,j,FixedMul(dx,3*FRACUNIT/32),FixedMul(dy,3*FRACUNIT/32),control,accel);
            }
        }else if(v==254){
            for(int j=0;j<numlines;j++)if(j!=i&&l->tag&&lines[j].tag==l->tag&&lines[j].sidenum[0]>=0){
                line_t *edge=&lines[j];double length=hypot((double)edge->dx,edge->dy);
                if(length>0)addScroll(0,edge->sidenum[0],(fixed_t)(-((double)dx*edge->dx+(double)dy*edge->dy)/length),
                    (fixed_t)(((double)dx*edge->dy-(double)dy*edge->dx)/length),control,accel);
            }
        }
    }
    buildCarryNeighbors();
    if(p_boom_compat)printf("Boom map features: %d scrollers\n",scrollCount);
}
void P_BoomTick(void){
    for(int i=0;i<scrollCount;i++){
        boom_scroll_t *s=&scrolls[i];fixed_t dx=s->dx,dy=s->dy;
        if(s->control>=0){fixed_t h=sumHeights(s->control),delta=(fixed_t)((uint32_t)h-(uint32_t)s->last);
            s->last=h;dx=FixedMul(dx,delta);dy=FixedMul(dy,delta);}
        if(s->accel){dx=(fixed_t)((uint32_t)dx+s->vx);dy=(fixed_t)((uint32_t)dy+s->vy);s->vx=dx;s->vy=dy;}
        if(s->type==0){side_t *side=&sides[s->index];side->textureoffset=(fixed_t)((uint32_t)side->textureoffset+dx);side->rowoffset=(fixed_t)((uint32_t)side->rowoffset+dy);}
        else{
            sector_t *sector=&sectors[s->index];
            if(s->type==1){sector->floor_xoffs=(fixed_t)((uint32_t)sector->floor_xoffs+dx);sector->floor_yoffs=(fixed_t)((uint32_t)sector->floor_yoffs+dy);}
            if(s->type==2){sector->ceiling_xoffs=(fixed_t)((uint32_t)sector->ceiling_xoffs+dx);sector->ceiling_yoffs=(fixed_t)((uint32_t)sector->ceiling_yoffs+dy);}
            if(s->type==3&&(dx||dy)){
                for(mobj_t *mo=sector->thinglist;mo;mo=mo->snext)carryThing(mo,sector,dx,dy);
                /* Actors keep contact while their footprint overlaps a conveyor,
                 * even after their centre enters a non-conveyor gate sector.
                 * Candidate sectors are unique and map-derived; no actor/save
                 * fields or new collision structure are needed. */
                for(int n=0;n<s->neighborCount;++n)
                    for(mobj_t *mo=sectors[carryNeighbors[s->neighborStart+n]].thinglist;mo;mo=mo->snext)
                        carryThing(mo,sector,dx,dy);
            }
        }
    }
}
sector_t *P_BoomRenderSector(sector_t *sec,sector_t *copy,int back){
    if(sec->heightsec<0)return sec;
    sector_t *control=&sectors[sec->heightsec];*copy=*sec;
    int viewer=-1;
    if(viewplayer&&viewplayer->mo)viewer=viewplayer->mo->subsector->sector->heightsec;
    int underwater=viewer>=0&&viewz<=sectors[viewer].floorheight;
    copy->floorheight=control->floorheight;copy->ceilingheight=control->ceilingheight;
    if(underwater){
        copy->floorheight=sec->floorheight;copy->ceilingheight=control->floorheight-1;
        if(!back){
            copy->floorpic=control->floorpic;copy->floor_xoffs=control->floor_xoffs;copy->floor_yoffs=control->floor_yoffs;
            copy->ceilingpic=control->ceilingpic;copy->ceiling_xoffs=control->ceiling_xoffs;copy->ceiling_yoffs=control->ceiling_yoffs;
            if(control->ceilingpic==skyflatnum){copy->floorheight=copy->ceilingheight+1;copy->ceilingpic=copy->floorpic;}
            copy->lightlevel=control->lightlevel;copy->floorlightsec=control->floorlightsec;copy->ceilinglightsec=control->ceilinglightsec;
        }
    }else if(viewer>=0&&viewz>=sectors[viewer].ceilingheight&&sec->ceilingheight>control->ceilingheight){
        copy->ceilingheight=control->ceilingheight;copy->floorheight=control->ceilingheight+1;
        copy->floorpic=copy->ceilingpic=control->ceilingpic;
        copy->floor_xoffs=copy->ceiling_xoffs=control->ceiling_xoffs;copy->floor_yoffs=copy->ceiling_yoffs=control->ceiling_yoffs;
        if(control->floorpic!=skyflatnum){copy->ceilingheight=sec->ceilingheight;copy->floorpic=control->floorpic;
            copy->floor_xoffs=control->floor_xoffs;copy->floor_yoffs=control->floor_yoffs;}
        copy->lightlevel=control->lightlevel;copy->floorlightsec=control->floorlightsec;copy->ceilinglightsec=control->ceilinglightsec;
    }
    return copy;
}
static void write32(byte *p,uint32_t v){for(int i=0;i<4;i++)p[i]=(byte)(v>>(i*8));}
static uint32_t read32(const byte *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
size_t P_BoomSave(byte *out){
    if(!p_boom_compat)return 0;
    size_t length=24+(size_t)numsectors*20+(size_t)numsides*8+(size_t)scrollCount*12;
    if(length>INT_MAX)I_Error("Boom save extension too large");
    if(!out)return length;
    write32(out,(uint32_t)numsectors);write32(out+4,(uint32_t)numsides);write32(out+8,(uint32_t)scrollCount);size_t at=12;
    for(int i=0;i<numsectors;i++){
        write32(out+at,sectors[i].floor_xoffs);write32(out+at+4,sectors[i].floor_yoffs);
        write32(out+at+8,sectors[i].ceiling_xoffs);write32(out+at+12,sectors[i].ceiling_yoffs);
        write32(out+at+16,sectors[i].stairlock);at+=20;
    }
    for(int i=0;i<numsides;i++){write32(out+at,sides[i].textureoffset);write32(out+at+4,sides[i].rowoffset);at+=8;}
    for(int i=0;i<scrollCount;i++){write32(out+at,scrolls[i].last);write32(out+at+4,scrolls[i].vx);write32(out+at+8,scrolls[i].vy);at+=12;}
    write32(out+at,2);write32(out+at+4,(uint32_t)length);memcpy(out+at+8,"BM01",4);return length;
}
size_t P_BoomLoad(const byte *data,size_t length){
    if(length<12||memcmp(data+length-4,"BM01",4))return 0;
    uint32_t bytes=read32(data+length-8),version=read32(data+length-12);
    if(bytes<24||bytes>length||(version!=1&&version!=2))I_Error("Invalid Boom save extension");
    const byte *in=data+length-bytes;
    if(read32(in)!=(uint32_t)numsectors||read32(in+4)!=(uint32_t)numsides||read32(in+8)!=(uint32_t)scrollCount||bytes!=P_BoomSave(NULL)-(version==1?(size_t)numsectors*4:0))
        I_Error("Boom save extension does not match this map");
    size_t at=12;
    for(int i=0;i<numsectors;i++){
        sectors[i].floor_xoffs=(int32_t)read32(in+at);sectors[i].floor_yoffs=(int32_t)read32(in+at+4);
        sectors[i].ceiling_xoffs=(int32_t)read32(in+at+8);sectors[i].ceiling_yoffs=(int32_t)read32(in+at+12);at+=16;
        sectors[i].stairlock=0;if(version>=2){sectors[i].stairlock=(int32_t)read32(in+at);at+=4;
            if(sectors[i].stairlock<0||sectors[i].stairlock>numsectors)I_Error("Invalid saved stair lock");}
    }
    for(int i=0;i<numsides;i++){sides[i].textureoffset=(int32_t)read32(in+at);sides[i].rowoffset=(int32_t)read32(in+at+4);at+=8;}
    for(int i=0;i<scrollCount;i++){scrolls[i].last=(int32_t)read32(in+at);scrolls[i].vx=(int32_t)read32(in+at+4);scrolls[i].vy=(int32_t)read32(in+at+8);at+=12;}
    return bytes;
}
