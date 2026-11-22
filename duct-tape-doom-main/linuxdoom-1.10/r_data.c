// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// $Log:$
//
// Revision 1.3  1997/01/29 20:10
// DESCRIPTION:
//	Preparation of data for rendering,
//	generation of lookups, caching, retrieval by name.
//
//-----------------------------------------------------------------------------


static const char
rcsid[] = "$Id: r_data.c,v 1.4 1997/02/03 16:47:55 b1 Exp $";

#include "i_system.h"
#include "z_zone.h"

#include "m_swap.h"

#include "w_wad.h"

#include "doomdef.h"
#include "r_local.h"
#include "p_local.h"

#include "doomstat.h"
#include "r_sky.h"

#ifdef _WIN32
#include <malloc.h>
#ifndef alloca
#define alloca _alloca
#endif
#define strncasecmp _strnicmp
#else
#include <alloca.h>
#include <strings.h>
#endif


#include "r_data.h"

//
// Graphics.
// DOOM graphics for walls and sprites
// is stored in vertical runs of opaque pixels (posts).
// A column is composed of zero or more posts,
// a patch or sprite is composed of zero or more columns.
// 



//
// Texture definition.
// Each texture is composed of one or more patches,
// with patches being lumps stored in the WAD.
// The lumps are referenced by number, and patched
// into the rectangular texture space using origin
// and possibly other attributes.
//
typedef struct
{
    int16_t	originx;
    int16_t	originy;
    int16_t	patch;
    int16_t	stepdir;
    int16_t	colormap;
} mappatch_t;


//
// Texture definition.
// A DOOM wall texture is a list of patches
// which are to be combined in a predefined order.
//
typedef struct
{
    char		name[8];
    int32_t		masked;	
    int16_t		width;
    int16_t		height;
    int32_t		columndirectory;	// OBSOLETE
    int16_t		patchcount;
    mappatch_t	patches[1];
} maptexture_t;

_Static_assert(sizeof(mappatch_t) == 10, "WAD texture patch layout");
_Static_assert(offsetof(maptexture_t, patches) == 22, "WAD texture header layout");


// A single patch from a texture definition,
//  basically a rectangular area within
//  the texture rectangle.
typedef struct
{
    // Block origin (allways UL),
    // which has allready accounted
    // for the internal origin of the patch.
    int		originx;	
    int		originy;
    int		patch;
} texpatch_t;


// A maptexturedef_t describes a rectangular texture,
//  which is composed of one or more mappatch_t structures
//  that arrange graphic patches.
typedef struct
{
    // Keep name for switch changing, etc.
    char	name[8];		
    short	width;
    short	height;
    
    // All the patches[patchcount]
    //  are drawn back to front into the cached texture.
    short	patchcount;
    texpatch_t	patches[1];		
    
} texture_t;



int		firstflat;
int		lastflat;
int		numflats;

int		firstpatch;
int		lastpatch;
int		numpatches;

int		firstspritelump;
int		lastspritelump;
int		numspritelumps;

int		numtextures;
texture_t**	textures;

void R_TextureDimensions(int texture, int *width, int *height)
{
    if (texture < 0 || texture >= numtextures) I_Error("Invalid GPU texture");
    *width = textures[texture]->width;
    *height = textures[texture]->height;
}

void R_TextureName(int texture, char name[9])
{
    if (texture < 0 || texture >= numtextures) I_Error("Invalid GPU texture");
    memcpy(name, textures[texture]->name, 8);
    name[8] = 0;
}

boolean R_TexturePatchInfo(int texture, int part, char name[9], int bounds[4])
{
    if (texture < 0 || texture >= numtextures) I_Error("Invalid GPU texture");
    const texture_t *tex = textures[texture];
    if (part < 0 || part >= tex->patchcount) return false;
    const texpatch_t *source = &tex->patches[part];
    const patch_t *patch = W_CacheLumpNum(source->patch, PU_CACHE);
    memcpy(name, lumpinfo[source->patch].name, 8);
    name[8] = 0;
    bounds[0] = source->originx > 0 ? source->originx : 0;
    bounds[1] = source->originy > 0 ? source->originy : 0;
    int right = source->originx + SHORT(patch->width);
    int bottom = source->originy + SHORT(patch->height);
    bounds[2] = right < tex->width ? right : tex->width;
    bounds[3] = bottom < tex->height ? bottom : tex->height;
    return true;
}

boolean R_TextureFromBaseWad(int texture)
{
    if (texture < 0 || texture >= numtextures) return false;
    int handle = lumpinfo[0].handle;
    int second = W_CheckNumForName("TEXTURE2");
    if (handle < 0 || lumpinfo[W_GetNumForName("TEXTURE1")].handle != handle ||
        lumpinfo[W_GetNumForName("PNAMES")].handle != handle ||
        (second >= 0 && lumpinfo[second].handle != handle))
        return false;
    const texture_t *tex = textures[texture];
    for (int i = 0; i < tex->patchcount; ++i)
        if (lumpinfo[tex->patches[i].patch].handle != handle) return false;
    return true;
}

void R_CopyIndexedTexture(int texture, byte *pixels)
{
    texture_t *tex = textures[texture];
    memset(pixels, 0, (size_t)tex->width * tex->height * 2);
    for (int n = 0; n < tex->patchcount; ++n)
    {
        texpatch_t *part = &tex->patches[n];
        int size = W_LumpLength(part->patch);
        patch_t *patch = W_CacheLumpNum(part->patch, PU_CACHE);
        int width = SHORT(patch->width);
        if (size < 8 || width < 1 || 8 + (size_t)width * 4 > (size_t)size)
            I_Error("Invalid texture patch header");
        for (int x = 0; x < width; ++x)
        {
            int destx = x + part->originx;
            if (destx < 0 || destx >= tex->width) continue;
            size_t offset = (uint32_t)LONG(patch->columnofs[x]);
            int previous = -1;
            while (offset < (size_t)size && ((byte*)patch)[offset] != 255)
            {
                byte *post = (byte*)patch + offset;
                if (offset + 4 > (size_t)size || offset + 4 + post[1] > (size_t)size)
                    I_Error("Invalid texture patch column");
                int top = post[0];
                if (top <= previous) top += previous;
                previous = top;
                for (int y = 0; y < post[1]; ++y)
                {
                    int desty = top + y + part->originy;
                    if (desty < 0 || desty >= tex->height) continue;
                    size_t pixel = ((size_t)desty * tex->width + destx) * 2;
                    pixels[pixel] = post[3 + y];
                    pixels[pixel + 1] = 255;
                }
                offset += post[1] + 4;
            }
        }
    }
}


int*			texturewidthmask;
// needed for texture pegging
fixed_t*		textureheight;		
int*			texturecompositesize;
int32_t**		texturecolumnlump;
uint32_t**		texturecolumnofs;
byte**			texturecomposite;
static byte **texturemasked;
static uint32_t **texturemaskedofs;

// for global animation
int*		flattranslation;
int*		texturetranslation;

// needed for pre rendering
fixed_t*	spritewidth;	
fixed_t*	spriteoffset;
fixed_t*	spritetopoffset;

lighttable_t	*colormaps;


//
// MAPTEXTURE_T CACHING
// When a texture is first needed,
//  it counts the number of composite columns
//  required in the texture and allocates space
//  for a column directory and any new columns.
// The directory will simply point inside other patches
//  if there is only one patch in a given column,
//  but any columns with multiple patches
//  will have new column_ts generated.
//



//
// R_DrawColumnInCache
// Clip and draw a column
//  from a patch into a cached post.
//
void
R_DrawColumnInCache
( column_t*	patch,
  byte*		cache,
  int		originy,
  int		cacheheight )
{
    int		count;
    int		position;
    int previous = -1;
    byte*	source;

	
    while (patch->topdelta != 0xff)
    {
	source = (byte *)patch + 3;
	count = patch->length;
	int top = patch->topdelta;
        if (top <= previous) top += previous;
        previous = top;
        position = originy + top;

	if (position < 0)
	{
	    count += position;
            if(count>0)source -= position;
	    position = 0;
	}

	if (position + count > cacheheight)
	    count = cacheheight - position;

	if (count > 0)
	    memcpy (cache + position, source, count);
		
	patch = (column_t *)(  (byte *)patch + patch->length + 4); 
    }
}



//
// R_GenerateComposite
// Using the texture definition,
//  the composite texture is created from the patches,
//  and each column is cached.
//
void R_GenerateComposite (int texnum)
{
    byte*		block;
    texture_t*		texture;
    texpatch_t*		patch;	
    patch_t*		realpatch;
    int			x;
    int			x1;
    int			x2;
    int			i;
    column_t*		patchcol;
    int32_t*		collump;
    uint32_t*	colofs;
	
    texture = textures[texnum];

    block = Z_Malloc (texturecompositesize[texnum],
		      PU_STATIC, 
		      &texturecomposite[texnum]);	

    memset(block, 0, (size_t)texturecompositesize[texnum]);
    collump = texturecolumnlump[texnum];
    colofs = texturecolumnofs[texnum];
    
    // Composite the columns together.
    patch = texture->patches;
		
    for (i=0 , patch = texture->patches;
	 i<texture->patchcount;
	 i++, patch++)
    {
	realpatch = W_CacheLumpNum (patch->patch, PU_CACHE);
	x1 = patch->originx;
	x2 = x1 + SHORT(realpatch->width);

	if (x1<0)
	    x = 0;
	else
	    x = x1;
	
	if (x2 > texture->width)
	    x2 = texture->width;

	for ( ; x<x2 ; x++)
	{
	    // Column does not have multiple patches?
	    if (collump[x] >= 0)
		continue;
	    
	    patchcol = (column_t *)((byte *)realpatch
				    + LONG(realpatch->columnofs[x-x1]));
	    R_DrawColumnInCache (patchcol,
				 block + colofs[x],
				 patch->originy,
				 texture->height);
	}
						
    }

    // Now that the texture has been built in column cache,
    //  it is purgable from zone memory.
    Z_ChangeTag (block, PU_CACHE);
}



//
// R_GenerateLookup
//
void R_GenerateLookup (int texnum)
{
    texture_t*		texture;
    byte *complexColumn;
    unsigned int*	patchcount;	// avoid wrapping after 255 overlapping patches
    texpatch_t*		patch;	
    patch_t*		realpatch;
    int			x;
    int			x1;
    int			x2;
    int			i;
    int32_t*		collump;
    uint32_t*	colofs;
	
    texture = textures[texnum];

    // Composited texture not created yet.
    texturecomposite[texnum] = 0;
    
    texturecompositesize[texnum] = 0;
    collump = texturecolumnlump[texnum];
    colofs = texturecolumnofs[texnum];
    
    // Now count the number of columns
    //  that are covered by more than one patch.
    // Fill in the lump / offset, so columns
    //  with only a single patch are all done.
    patchcount = calloc ((size_t)texture->width, sizeof(*patchcount));
    complexColumn=calloc((size_t)texture->width,1);
    if (!patchcount || !complexColumn) I_Error("R_GenerateLookup: out of memory");
    patch = texture->patches;
		
    for (i=0 , patch = texture->patches;
	 i<texture->patchcount;
	 i++, patch++)
    {
	realpatch = W_CacheLumpNum (patch->patch, PU_CACHE);
        int patchsize = W_LumpLength(patch->patch);
        if (patchsize < 8 || SHORT(realpatch->width) < 1 ||
            8 + (size_t)SHORT(realpatch->width)*4 > (size_t)patchsize)
            I_Error("R_GenerateLookup: invalid patch header");
        for (int c=0;c<SHORT(realpatch->width);++c) {
            size_t at=(uint32_t)LONG(realpatch->columnofs[c]);
            if (at < 8 + (size_t)SHORT(realpatch->width)*4)
                I_Error("R_GenerateLookup: invalid patch offset");
            for (;;) {
                if (at >= (size_t)patchsize) I_Error("R_GenerateLookup: unterminated patch column");
                const byte *post=(const byte*)realpatch+at;
                if (*post==255) break;
                if ((size_t)patchsize-at < 4+(size_t)post[1])
                    I_Error("R_GenerateLookup: truncated patch column");
                at+=4+(size_t)post[1];
            }
        }

	x1 = patch->originx;
	x2 = x1 + SHORT(realpatch->width);
	
	if (x1 < 0)
	    x = 0;
	else
	    x = x1;

	if (x2 > texture->width)
	    x2 = texture->width;
	for ( ; x<x2 ; x++)
	{
            const column_t *col=(const column_t*)((const byte*)realpatch+(uint32_t)LONG(realpatch->columnofs[x-x1]));
            if(patch->originy || col->topdelta || col->length<texture->height)complexColumn[x]=1;
	    patchcount[x]++;
	    collump[x] = patch->patch;
	    colofs[x] = LONG(realpatch->columnofs[x-x1])+3;
	}
    }
	
    for (x=0 ; x<texture->width ; x++)
    {
        if (!patchcount[x])
        {
            collump[x] = -1;
            colofs[x] = texturecompositesize[texnum];
            if (texturecompositesize[texnum] > INT_MAX - texture->height)
                I_Error("R_GenerateLookup: composite too large");
            texturecompositesize[texnum] += texture->height;
            continue;
        }
	// I_Error ("R_GenerateLookup: column without a patch");
	
	if (patchcount[x] > 1 || complexColumn[x])
	{
	    // Use the cached block.
	    collump[x] = -1;	
	    colofs[x] = texturecompositesize[texnum];
	    
	    if (texturecompositesize[texnum] > INT_MAX-texture->height)
	    {
		I_Error ("R_GenerateLookup: texture %i composite exceeds allocator capacity",
			 texnum);
	    }
	    
	    texturecompositesize[texnum] += texture->height;
	}
    }	
    free(patchcount);free(complexColumn);
}




//
// R_GetColumn
//
byte*
R_GetColumn
( int		tex,
  int		col )
{
    int		lump;
    int		ofs;
	
    if(textures[tex]->width==(texturewidthmask[tex]+1))col &= texturewidthmask[tex];
    else {col%=textures[tex]->width;if(col<0)col+=textures[tex]->width;}
    lump = texturecolumnlump[tex][col];
    ofs = texturecolumnofs[tex][col];
    
    if (lump >= 0)
	return (byte *)W_CacheLumpNum(lump,PU_CACHE)+ofs;

    if (!texturecomposite[tex])
	R_GenerateComposite (tex);

    return texturecomposite[tex] + ofs;
}




/* A separate, purgeable post stream preserves transparency in multi-patch
 * masked walls. R_GetColumn remains the dense, wrapped wall sampler. */
typedef struct {byte *data;size_t count,capacity;} texture_posts_t;
static void R_PostBytes(texture_posts_t *v,const byte *bytes,size_t count){
    if(count>INT_MAX-v->count)I_Error("Masked texture exceeds allocator capacity");
    if(v->count+count>v->capacity){
        size_t capacity=v->capacity?v->capacity:4096;
        while(capacity<v->count+count){if(capacity>INT_MAX/2){capacity=INT_MAX;break;}capacity*=2;}
        void *next=realloc(v->data,capacity);if(!next)I_Error("Masked texture: out of memory");v->data=next;v->capacity=capacity;
    }
    memcpy(v->data+v->count,bytes,count);v->count+=count;
}
byte *R_GetMaskedColumn(int tex,int col){
    texture_t *t=textures[tex];col%=t->width;if(col<0)col+=t->width;
    if(!texturemasked[tex]){
        size_t pixels=(size_t)t->width*t->height;
        if(pixels>INT_MAX/2)I_Error("Masked texture too large");
        byte *indexed=malloc(pixels*2);if(!indexed)I_Error("Masked texture: out of memory");
        R_CopyIndexedTexture(tex,indexed);
        if(!texturemaskedofs[tex])texturemaskedofs[tex]=Z_Malloc(t->width*sizeof(uint32_t),PU_STATIC,0);
        texture_posts_t v={0};
        for(int x=0;x<t->width;x++){
            texturemaskedofs[tex][x]=(uint32_t)v.count;int previous=-1,y=0;
            while(y<t->height){
                while(y<t->height&&!indexed[((size_t)y*t->width+x)*2+1])++y;
                if(y==t->height)break;
                while(y>254&&y-previous>254){
                    byte empty[4]={254,0,0,0};R_PostBytes(&v,empty,4);
                    previous=previous<254?254:previous+254;
                }
                int top=y<=254&&y>previous?y:y-previous;
                if(top<0||top>254)I_Error("Masked texture: invalid tall post");
                int length=0;byte samples[254];
                while(y+length<t->height&&length<254&&indexed[((size_t)(y+length)*t->width+x)*2+1]){
                    samples[length]=indexed[((size_t)(y+length)*t->width+x)*2];++length;
                }
                byte header[3]={(byte)top,(byte)length,0},zero=0;
                R_PostBytes(&v,header,3);R_PostBytes(&v,samples,(size_t)length);R_PostBytes(&v,&zero,1);
                previous=y;y+=length;
            }
            byte end=255;R_PostBytes(&v,&end,1);
        }
        texturemasked[tex]=Z_Malloc((int)v.count,PU_STATIC,&texturemasked[tex]);
        memcpy(texturemasked[tex],v.data,v.count);free(v.data);free(indexed);Z_ChangeTag(texturemasked[tex],PU_CACHE);
    }
    return texturemasked[tex]+texturemaskedofs[tex][col];
}

/* Integration check uses the independent indexed compositor as an oracle.
 * Composite columns must match even after their byte offset crosses 64 KiB. */
void R_ValidateTextureComposites(void)
{
    int columns=0,large=0;
    for(int i=0;i<numtextures;i++){
        texture_t *t=textures[i];
        byte *expected=malloc((size_t)t->width*t->height*2);
        if(!expected)I_Error("Texture validation: out of memory");
        R_CopyIndexedTexture(i,expected);
        for(int x=0;x<t->width;x++)if(texturecolumnlump[i][x]<0){
            byte *column=R_GetColumn(i,x);++columns;
            if(texturecolumnofs[i][x]>65535)++large;
            for(int y=0;y<t->height;y++){
                size_t at=((size_t)y*t->width+x)*2;
                if(column[y]!=(expected[at+1]?expected[at]:0))
                    I_Error("Texture validation: %.8s %dx%d pixel %d,%d mismatch %d != %d, offset %u",t->name,t->width,t->height,x,y,column[y],expected[at],texturecolumnofs[i][x]);
            }
        }
        if(!memcmp(t->name,"BTST",4))for(int x=0;x<t->width;x++){
            byte *column=R_GetMaskedColumn(i,x);int previous=-1;
            byte *decoded=calloc((size_t)t->height,2);if(!decoded)I_Error("Masked validation allocation");
            for(byte *post=column;*post!=255;post+=post[1]+4){
                int top=post[0];if(top<=previous)top+=previous;previous=top;
                if(top<0||top+post[1]>t->height)I_Error("Masked validation bounds");
                for(int y=0;y<post[1];y++){decoded[(top+y)*2]=post[3+y];decoded[(top+y)*2+1]=255;}
            }
            for(int y=0;y<t->height;y++){
                size_t at=((size_t)y*t->width+x)*2;
                if(decoded[y*2+1]!=expected[at+1] || (decoded[y*2+1]&&decoded[y*2]!=expected[at]))
                    I_Error("Masked texture roundtrip mismatch %.8s %d,%d",t->name,x,y);
            }
            free(decoded);
        }
        free(expected);
    }
    printf("PASS texture composites: %d columns, %d offsets above 64KiB\n",columns,large);
}

//
// R_InitTextures
// Initializes the texture list
//  with the textures from the world map.
//
void R_InitTextures (void)
{
    maptexture_t*	mtexture;
    texture_t*		texture;
    mappatch_t*		mpatch;
    texpatch_t*		patch;

    int			i;
    int			j;

    int*		maptex;
    int*		maptex2;
    int*		maptex1;
    
    char		name[9];
    char*		names;
    char*		name_p;
    
    int*		patchlookup;
    
    int			totalwidth;
    int			nummappatches;
    int			offset;
    int			maxoff;
    int			maxoff2;
    int			numtextures1;
    int			numtextures2;

    int*		directory;
    
    int			temp1;
    int			temp2;
    int			temp3;

    
    // Load the patch names from pnames.lmp.
    name[8] = 0;	
    names = W_CacheLumpName ("PNAMES", PU_STATIC);
    int namesLength=W_LumpLength(W_GetNumForName("PNAMES"));
    if (namesLength < 4) I_Error("R_InitTextures: truncated PNAMES");
    nummappatches = LONG ( *((int *)names) );
    if (nummappatches<0 || nummappatches>(namesLength-4)/8) I_Error("R_InitTextures: invalid PNAMES count");
    name_p = names+4;
    patchlookup = malloc ((size_t)nummappatches*sizeof(*patchlookup));
    if (!patchlookup) I_Error("R_InitTextures: out of memory");
    
    for (i=0 ; i<nummappatches ; i++)
    {
	strncpy (name,name_p+i*8, 8);
	patchlookup[i] = W_CheckNumForName (name);
    }
    Z_Free (names);
    
    // Load the map texture definitions from textures.lmp.
    // The data is contained in one or two lumps,
    //  TEXTURE1 for shareware, plus TEXTURE2 for commercial.
    if(W_LumpLength(W_GetNumForName("TEXTURE1"))<4)I_Error("R_InitTextures: truncated TEXTURE1");
    maptex = maptex1 = W_CacheLumpName ("TEXTURE1", PU_STATIC);
    numtextures1 = LONG(*maptex);
    maxoff = W_LumpLength (W_GetNumForName ("TEXTURE1"));
    directory = maptex+1;
	
    if (W_CheckNumForName ("TEXTURE2") != -1)
    {
        if(W_LumpLength(W_GetNumForName("TEXTURE2"))<4)I_Error("R_InitTextures: truncated TEXTURE2");
	maptex2 = W_CacheLumpName ("TEXTURE2", PU_STATIC);
	numtextures2 = LONG(*maptex2);
	maxoff2 = W_LumpLength (W_GetNumForName ("TEXTURE2"));
    }
    else
    {
	maptex2 = NULL;
	numtextures2 = 0;
	maxoff2 = 0;
    }
    if(numtextures1<0||numtextures2<0||numtextures1>INT_MAX-numtextures2)I_Error("R_InitTextures: texture count overflow");
    numtextures = numtextures1 + numtextures2;
	
    if (numtextures1<0 || numtextures1>(maxoff-4)/4 || numtextures2<0 || numtextures2>(maxoff2 ? (maxoff2-4)/4 : 0) ||
        numtextures1>INT_MAX-numtextures2 || numtextures<1 || numtextures>INT_MAX/(int)sizeof(*textures))
        I_Error("R_InitTextures: invalid texture directory count");
    textures = Z_Malloc (numtextures*sizeof(*textures), PU_STATIC, 0);
    texturecolumnlump = Z_Malloc (numtextures*sizeof(*texturecolumnlump), PU_STATIC, 0);
    texturecolumnofs = Z_Malloc (numtextures*sizeof(*texturecolumnofs), PU_STATIC, 0);
    texturecomposite = Z_Malloc (numtextures*sizeof(*texturecomposite), PU_STATIC, 0);
    texturemasked=Z_Malloc(numtextures*sizeof(*texturemasked),PU_STATIC,0);memset(texturemasked,0,numtextures*sizeof(*texturemasked));
    texturemaskedofs=Z_Malloc(numtextures*sizeof(*texturemaskedofs),PU_STATIC,0);memset(texturemaskedofs,0,numtextures*sizeof(*texturemaskedofs));
    texturecompositesize = Z_Malloc (numtextures*sizeof(*texturecompositesize), PU_STATIC, 0);
    texturewidthmask = Z_Malloc (numtextures*sizeof(*texturewidthmask), PU_STATIC, 0);
    textureheight = Z_Malloc (numtextures*sizeof(*textureheight), PU_STATIC, 0);

    totalwidth = 0;
    
    //	Really complex printing shit...
    temp1 = W_GetNumForName ("S_START");  // P_???????
    temp2 = W_GetNumForName ("S_END") - 1;
    temp3 = ((temp2-temp1+63)/64) + ((numtextures+63)/64);
    printf("[");
    for (i = 0; i < temp3; i++)
	printf(" ");
    printf("         ]");
    for (i = 0; i < temp3; i++)
	printf("\x8");
    printf("\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8");	
	
    for (i=0 ; i<numtextures ; i++, directory++)
    {
	if (!(i&63))
	    printf (".");

	if (i == numtextures1)
	{
	    // Start looking in second texture file.
	    maptex = maptex2;
	    maxoff = maxoff2;
	    directory = maptex+1;
	}
		
	offset = LONG(*directory);

	if (offset < 0 || offset > maxoff - 22)
	    I_Error ("R_InitTextures: bad texture directory");
	
	mtexture = (maptexture_t *) ( (byte *)maptex + offset);
        if (SHORT(mtexture->width)<1 || SHORT(mtexture->height)<1 ||
            SHORT(mtexture->patchcount)<1 ||
            (size_t)SHORT(mtexture->patchcount)*10 > (size_t)(maxoff-offset-22))
            I_Error("R_InitTextures: invalid texture definition");


	texture = textures[i] =
	    Z_Malloc (sizeof(texture_t)
		      + sizeof(texpatch_t)*(SHORT(mtexture->patchcount)-1),
		      PU_STATIC, 0);
	
	texture->width = SHORT(mtexture->width);
	texture->height = SHORT(mtexture->height);
	texture->patchcount = SHORT(mtexture->patchcount);

	memcpy (texture->name, mtexture->name, sizeof(texture->name));
	mpatch = &mtexture->patches[0];
	patch = &texture->patches[0];

	for (j=0 ; j<texture->patchcount ; j++, mpatch++, patch++)
	{
	    patch->originx = SHORT(mpatch->originx);
	    patch->originy = SHORT(mpatch->originy);
	    int patchindex = (uint16_t)SHORT(mpatch->patch);
            if (patchindex >= nummappatches) I_Error("R_InitTextures: invalid patch index");
            patch->patch = patchlookup[patchindex];
	    if (patch->patch == -1)
	    {
		I_Error ("R_InitTextures: Missing patch in texture %s",
			 texture->name);
	    }
	}		
	texturecolumnlump[i] = Z_Malloc (texture->width*sizeof(**texturecolumnlump), PU_STATIC,0);
	texturecolumnofs[i] = Z_Malloc (texture->width*sizeof(**texturecolumnofs), PU_STATIC,0);

	j = 1;
	while (j*2 <= texture->width)
	    j<<=1;

	texturewidthmask[i] = j-1;
	textureheight[i] = texture->height<<FRACBITS;
		
	totalwidth += texture->width;
    }

    free(patchlookup);
    Z_Free (maptex1);
    if (maptex2)
	Z_Free (maptex2);
    
    // Precalculate whatever possible.	
    for (i=0 ; i<numtextures ; i++)
	R_GenerateLookup (i);
    
    // Create translation table for global animation.
    texturetranslation = Z_Malloc ((numtextures+1)*4, PU_STATIC, 0);
    
    for (i=0 ; i<numtextures ; i++)
	texturetranslation[i] = i;
}



//
// R_InitFlats
//
void R_InitFlats (void)
{
    int		i;
	
    firstflat = W_GetNumForName ("F_START") + 1;
    lastflat = W_GetNumForName ("F_END") - 1;
    numflats = lastflat - firstflat + 1;
	
    // Create translation table for global animation.
    flattranslation = Z_Malloc ((numflats+1)*4, PU_STATIC, 0);
    
    for (i=0 ; i<numflats ; i++)
	flattranslation[i] = i;
}


//
// R_InitSpriteLumps
// Finds the width and hoffset of all sprites in the wad,
//  so the sprite does not need to be cached completely
//  just for having the header info ready during rendering.
//
void R_InitSpriteLumps (void)
{
    int		i;
    patch_t	*patch;
	
    firstspritelump = W_GetNumForName ("S_START") + 1;
    lastspritelump = W_GetNumForName ("S_END") - 1;
    
    numspritelumps = lastspritelump - firstspritelump + 1;
    spritewidth = Z_Malloc (numspritelumps*4, PU_STATIC, 0);
    spriteoffset = Z_Malloc (numspritelumps*4, PU_STATIC, 0);
    spritetopoffset = Z_Malloc (numspritelumps*4, PU_STATIC, 0);
	
    for (i=0 ; i< numspritelumps ; i++)
    {
	if (!(i&63))
	    printf (".");

	patch = W_CacheLumpNum (firstspritelump+i, PU_CACHE);
	spritewidth[i] = SHORT(patch->width)<<FRACBITS;
	spriteoffset[i] = SHORT(patch->leftoffset)<<FRACBITS;
	spritetopoffset[i] = SHORT(patch->topoffset)<<FRACBITS;
    }
}



//
// R_InitColormaps
//
void R_InitColormaps (void)
{
    int	lump, length;
    
    // Load in the light tables, 
    //  256 byte align tables.
    lump = W_GetNumForName("COLORMAP"); 
    length = W_LumpLength (lump) + 255; 
    colormaps = Z_Malloc (length, PU_STATIC, 0); 
    colormaps = (byte *)( ((uintptr_t)colormaps + 255) & ~(uintptr_t)255); 
    W_ReadLump (lump,colormaps); 
}



//
// R_InitData
// Locates all the lumps
//  that will be used by all views
// Must be called after W_Init.
//
void R_InitData (void)
{
    R_InitTextures ();
    printf ("\nInitTextures");
    R_InitFlats ();
    printf ("\nInitFlats");
    R_InitSpriteLumps ();
    printf ("\nInitSprites");
    R_InitColormaps ();
    printf ("\nInitColormaps");
}



//
// R_FlatNumForName
// Retrieval, get a flat number for a flat name.
//
int R_CheckFlatNumForName (char* name)
{
    int		i = W_CheckFlatLump (name, firstflat, lastflat+1);

    return i == -1 ? -1 : i - firstflat;
}

int R_FlatNumForName (char* name)
{
    int		i;
    char	namet[9];

    i = R_CheckFlatNumForName (name);

    if (i == -1)
    {
	namet[8] = 0;
	memcpy (namet, name,8);
	I_Error ("R_FlatNumForName: %s not found",namet);
    }
    return i;
}




//
// R_CheckTextureNumForName
// Check whether texture is available.
// Filter out NoTexture indicator.
//
int	R_CheckTextureNumForName (char *name)
{
    int		i;

    // "NoTexture" marker.
    if (name[0] == '-')		
	return 0;
		
    for (i=0 ; i<numtextures ; i++)
	if (!strncasecmp (textures[i]->name, name, 8) )
	    return i;
		
    return -1;
}



//
// R_TextureNumForName
// Calls R_CheckTextureNumForName,
//  aborts with error message.
//
int	R_TextureNumForName (char* name)
{
    int		i;
	
    i = R_CheckTextureNumForName (name);

    if (i==-1)
    {
	I_Error ("R_TextureNumForName: %s not found",
		 name);
    }
    return i;
}




//
// R_PrecacheLevel
// Preloads all relevant graphics for the level.
//
int		flatmemory;
int		texturememory;
int		spritememory;

void R_PrecacheLevel (void)
{
    char*		flatpresent;
    char*		texturepresent;
    char*		spritepresent;

    int			i;
    int			j;
    int			k;
    int			lump;
    
    texture_t*		texture;
    thinker_t*		th;
    spriteframe_t*	sf;

    if (demoplayback)
	return;
    
    // Precache flats.
    flatpresent = alloca(numflats);
    memset (flatpresent,0,numflats);	

    for (i=0 ; i<numsectors ; i++)
    {
	flatpresent[sectors[i].floorpic] = 1;
	flatpresent[sectors[i].ceilingpic] = 1;
    }
	
    flatmemory = 0;

    for (i=0 ; i<numflats ; i++)
    {
	if (flatpresent[i])
	{
	    lump = firstflat + i;
	    flatmemory += lumpinfo[lump].size;
	    W_CacheLumpNum(lump, PU_CACHE);
	}
    }
    
    // Precache textures.
    texturepresent = alloca(numtextures);
    memset (texturepresent,0, numtextures);
	
    for (i=0 ; i<numsides ; i++)
    {
	texturepresent[sides[i].toptexture] = 1;
	texturepresent[sides[i].midtexture] = 1;
	texturepresent[sides[i].bottomtexture] = 1;
    }

    // Sky texture is always present.
    // Note that F_SKY1 is the name used to
    //  indicate a sky floor/ceiling as a flat,
    //  while the sky texture is stored like
    //  a wall texture, with an episode dependend
    //  name.
    texturepresent[skytexture] = 1;
	
    texturememory = 0;
    for (i=0 ; i<numtextures ; i++)
    {
	if (!texturepresent[i])
	    continue;

	texture = textures[i];
	
	for (j=0 ; j<texture->patchcount ; j++)
	{
	    lump = texture->patches[j].patch;
	    texturememory += lumpinfo[lump].size;
	    W_CacheLumpNum(lump , PU_CACHE);
	}
    }
    
    // Precache sprites.
    spritepresent = alloca(numsprites);
    memset (spritepresent,0, numsprites);
	
    for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    {
	if (th->function.acp1 == (actionf_p1)P_MobjThinker)
	    spritepresent[((mobj_t *)th)->sprite] = 1;
    }
	
    spritememory = 0;
    for (i=0 ; i<numsprites ; i++)
    {
	if (!spritepresent[i])
	    continue;

	for (j=0 ; j<sprites[i].numframes ; j++)
	{
	    sf = &sprites[i].spriteframes[j];
	    for (k=0 ; k<8 ; k++)
	    {
		lump = firstspritelump + sf->lump[k];
		spritememory += lumpinfo[lump].size;
		W_CacheLumpNum(lump , PU_CACHE);
	    }
	}
    }
}



