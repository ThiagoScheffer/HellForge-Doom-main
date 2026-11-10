/* Uncompressed ZDBSP extended nodes. Read packed fields explicitly: the
 * on-disk seg is eleven bytes and must never be cast to a native struct. */
typedef struct { const byte *at,*end; } xnode_reader_t;
static uint32_t X_Read(xnode_reader_t *r,int bytes)
{
    if((size_t)(r->end-r->at)<(size_t)bytes)I_Error("XNOD: truncated node data");
    uint32_t value=0;
    for(int i=0;i<bytes;i++)value|=(uint32_t)*r->at++<<(i*8);
    return value;
}
static int X_Count(xnode_reader_t *r,size_t minimum,size_t native)
{
    uint32_t n=X_Read(r,4);
    if(n>INT_MAX/native || (minimum && n>(size_t)(r->end-r->at)/minimum))
        I_Error("XNOD: invalid array count %u",n);
    return (int)n;
}
static fixed_t X_Short(xnode_reader_t *r){return (int16_t)X_Read(r,2)*FRACUNIT;}
static void P_LoadXNodes(int lump)
{
    int length=W_LumpLength(lump);
    byte *data=W_CacheLumpNum(lump,PU_STATIC);
    xnode_reader_t r={data+4,data+length};
    uint32_t original=X_Read(&r,4);
    int added=X_Count(&r,8,sizeof(vertex_t));
    if(original!=(uint32_t)numvertexes || original>INT_MAX/sizeof(vertex_t)-(size_t)added)
        I_Error("XNOD: inconsistent original vertex count");
    vertex_t *old=vertexes;
    numvertexes+=(int)added;
    vertexes=Z_Malloc(numvertexes*sizeof(vertex_t),PU_LEVEL,0);
    memcpy(vertexes,old,(size_t)original*sizeof(vertex_t));
    for(uint32_t i=original;i<(uint32_t)numvertexes;i++){
        vertexes[i].x=(int32_t)X_Read(&r,4);vertexes[i].y=(int32_t)X_Read(&r,4);
    }
    /* LINEDEFS have already been loaded and hold vertex pointers. */
    for(int i=0;i<numlines;i++){
        lines[i].v1=vertexes+(lines[i].v1-old);
        lines[i].v2=vertexes+(lines[i].v2-old);
    }
    Z_Free(old);
    numsubsectors=X_Count(&r,4,sizeof(subsector_t));
    if(!numsubsectors)I_Error("XNOD: no subsectors");
    subsectors=Z_Malloc(numsubsectors*sizeof(subsector_t),PU_LEVEL,0);
    memset(subsectors,0,numsubsectors*sizeof(subsector_t));
    uint32_t total=0;
    for(int i=0;i<numsubsectors;i++){
        uint32_t count=X_Read(&r,4);
        if(!count || count>INT_MAX-total)I_Error("XNOD: invalid subsector seg count");
        subsectors[i].firstline=(int)total;subsectors[i].numlines=(int)count;total+=count;
    }
    numsegs=X_Count(&r,11,sizeof(seg_t));
    if((uint32_t)numsegs!=total)I_Error("XNOD: inconsistent seg count");
    segs=Z_Malloc(numsegs*sizeof(seg_t),PU_LEVEL,0);memset(segs,0,numsegs*sizeof(seg_t));
    for(int i=0;i<numsegs;i++){
        uint32_t v1=X_Read(&r,4),v2=X_Read(&r,4),line=X_Read(&r,2),side=X_Read(&r,1);
        if(v1>=(uint32_t)numvertexes||v2>=(uint32_t)numvertexes||line>=(uint32_t)numlines||side>1)
            I_Error("XNOD: invalid seg reference");
        seg_t *seg=&segs[i];line_t *ld=&lines[line];
        if(ld->sidenum[side]<0 || ld->sidenum[side]>=numsides)I_Error("XNOD: missing seg sidedef");
        seg->v1=&vertexes[v1];seg->v2=&vertexes[v2];seg->linedef=ld;
        seg->sidedef=&sides[ld->sidenum[side]];seg->frontsector=seg->sidedef->sector;
        if((ld->flags&ML_TWOSIDED)&&ld->sidenum[side^1]>=0)seg->backsector=sides[ld->sidenum[side^1]].sector;
        seg->angle=R_PointToAngle2(seg->v1->x,seg->v1->y,seg->v2->x,seg->v2->y);
        vertex_t *origin=side?ld->v2:ld->v1;
        double dx=(double)seg->v1->x-origin->x,dy=(double)seg->v1->y-origin->y;
        double distance=sqrt(dx*dx+dy*dy);
        if(distance>INT_MAX)I_Error("XNOD: seg offset exceeds fixed-point range");
        seg->offset=(fixed_t)distance;
    }
    numnodes=X_Count(&r,32,sizeof(node_t));
    nodes=Z_Malloc(numnodes*sizeof(node_t),PU_LEVEL,0);
    for(int i=0;i<numnodes;i++){
        node_t *n=&nodes[i];n->x=X_Short(&r);n->y=X_Short(&r);n->dx=X_Short(&r);n->dy=X_Short(&r);
        for(int j=0;j<2;j++)for(int k=0;k<4;k++)n->bbox[j][k]=X_Short(&r);
        n->children[0]=X_Read(&r,4);n->children[1]=X_Read(&r,4);
    }
    if(r.at!=r.end)I_Error("XNOD: unexpected trailing node data");
    Z_Free(data);
    printf("XNOD: %d vertices, %d subsectors, %d segs, %d nodes\n",numvertexes,numsubsectors,numsegs,numnodes);
}
