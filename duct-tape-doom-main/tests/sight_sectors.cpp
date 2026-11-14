// SF2012 MAP02 contains ML_TWOSIDED lines with no back sidedef.
// Exercise the actual BSP sight leaf path, without a renderer or WAD dependency.
#include <cstdio>
#include <cstdlib>
extern "C" {
#include "doomdef.h"
#include "p_local.h"
#include "r_state.h"
extern divline_t strace;
extern fixed_t t2x,t2y,sightzstart,topslope,bottomslope;
boolean P_CrossSubsector(int);
}
int main() {
    sector_t room[2]={};vertex_t v[2]={};line_t wall={};seg_t edge={};subsector_t leaf={};
    room[0].ceilingheight=room[1].ceilingheight=128*FRACUNIT;
    v[0].x=v[1].x=16*FRACUNIT;v[0].y=-64*FRACUNIT;v[1].y=64*FRACUNIT;
    wall.v1=&v[0];wall.v2=&v[1];wall.flags=ML_TWOSIDED;
    edge.linedef=&wall;edge.frontsector=&room[0];edge.backsector=&room[1];
    leaf.numlines=1;segs=&edge;numsegs=1;subsectors=&leaf;numsubsectors=1;lines=&wall;numlines=1;
    strace.x=-64*FRACUNIT;strace.dx=128*FRACUNIT;t2x=64*FRACUNIT;
    sightzstart=32*FRACUNIT;topslope=32*FRACUNIT;bottomslope=0;
    auto check=[&](bool expected,const char *name){++validcount;
        if((bool)P_CrossSubsector(0)!=expected){std::fprintf(stderr,"FAIL %s\n",name);std::exit(1);}
        std::printf("PASS %s\n",name);std::fflush(stdout);};
    check(true,"open two-sided sight");
    wall.flags=0;check(false,"ordinary one-sided wall");wall.flags=ML_TWOSIDED;
    edge.backsector=nullptr;check(false,"SF2012 two-sided flag without back sector");
    edge.backsector=&room[1];edge.frontsector=nullptr;check(false,"missing front sector");
    edge.frontsector=&room[0];room[1].floorheight=128*FRACUNIT;
    check(false,"closed two-sided opening");
}
