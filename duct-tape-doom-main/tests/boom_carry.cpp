#include <cstdio>
#include <cstdlib>
#include <vector>
extern "C" {
#include "doomdef.h"
#include "p_local.h"
#include "p_boom.h"
#include "m_bbox.h"
}
static void check(bool ok,const char *name){if(!ok){std::fprintf(stderr,"FAIL %s\n",name);std::exit(1);}std::printf("PASS %s\n",name);}
int main(){
    sector_t world[3]={};subsector_t leaves[3]={};side_t side[1]={};
    vertex_t vertices[12]={};line_t edges[12]={};line_t *sectorEdges[3][4]={};
    for(int s=0;s<3;++s){
        int left=s==2?1024:s*64;
        const int xy[4][2]={{left,0},{left+64,0},{left+64,64},{left,64}};
        world[s].floorheight=0;world[s].ceilingheight=128*FRACUNIT;world[s].linecount=4;world[s].lines=sectorEdges[s];leaves[s].sector=&world[s];
        for(int j=0;j<4;++j){vertices[s*4+j].x=xy[j][0]*FRACUNIT;vertices[s*4+j].y=xy[j][1]*FRACUNIT;}
        for(int j=0;j<4;++j){auto &e=edges[s*4+j];e.v1=&vertices[s*4+j];e.v2=&vertices[s*4+(j+1)%4];
            e.dx=e.v2->x-e.v1->x;e.dy=e.v2->y-e.v1->y;e.slopetype=!e.dx?ST_VERTICAL:ST_HORIZONTAL;
            e.frontsector=&world[s];e.sidenum[0]=0;e.sidenum[1]=-1;M_ClearBox(e.bbox);
            M_AddToBox(e.bbox,e.v1->x,e.v1->y);M_AddToBox(e.bbox,e.v2->x,e.v2->y);sectorEdges[s][j]=&e;
        }
    }
    sectors=world;numsectors=3;lines=edges;numlines=12;sides=side;numsides=1;side[0].sector=&world[0];
    world[0].tag=1;edges[0].special=253;edges[0].tag=1;
    mobj_t actor[8]={};
    for(int i=0;i<8;++i){actor[i].subsector=&leaves[1];actor[i].x=72*FRACUNIT;actor[i].y=32*FRACUNIT;actor[i].radius=16*FRACUNIT;actor[i].snext=i==7?nullptr:&actor[i+1];}
    actor[0].flags=MF_NOBLOCKMAP; // Voodoo/cosmetic contacts must not rely on BLOCKMAP membership.
    actor[1].x=88*FRACUNIT;actor[2].flags=MF_NOCLIP;actor[3].flags=MF_NOGRAVITY;
    actor[4].z=8*FRACUNIT;actor[4].floorz=8*FRACUNIT;
    actor[5].x=72*FRACUNIT;actor[5].y=-8*FRACUNIT;
    actor[6].x=112*FRACUNIT;actor[6].radius=64*FRACUNIT;
    actor[7].flags=MF_NOGRAVITY;actor[7].z=-8*FRACUNIT;
    world[1].thinglist=actor;
    P_BoomSpawn();P_BoomTick();const fixed_t impulse=3*FRACUNIT/16;
    check(actor[0].momx==impulse,"footprint touching conveyor carries from adjacent sector, including NOBLOCKMAP");
    check(actor[1].momx==0,"separated footprint does not carry");
    check(actor[2].momx==0,"noclip excluded");check(actor[3].momx==0,"floating actor above water excluded");
    check(actor[4].momx==0,"actor above conveyor floor excluded even on higher footing");
    check(actor[5].momx==impulse,"corner contact applies once despite multiple crossed edges");
    check(actor[6].momx==impulse,"large stock actor radius reaches neighboring conveyor");
    world[0].heightsec=2;world[2].floorheight=16*FRACUNIT;P_BoomTick();
    check(actor[7].momx==impulse,"submerged floating actor receives carry");
    check(actor[2].momx==0,"submersion does not bypass noclip exclusion");
    world[0].heightsec=-1;
    std::vector<byte> saved(P_BoomSave(nullptr));P_BoomSave(saved.data());
    P_BoomSpawn();check(P_BoomLoad(saved.data(),saved.size())==saved.size(),"existing Boom save extension still round-trips");
    fixed_t before=actor[0].momx;P_BoomTick();check(actor[0].momx==before+impulse,"map-derived conveyor contacts rebuild after save initialization");
    actor[0].x=200*FRACUNIT;before=actor[0].momx;P_BoomTick();check(actor[0].momx==before,"leaving footprint contact stops additional carry");
}
