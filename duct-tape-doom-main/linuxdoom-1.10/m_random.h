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
// DESCRIPTION:
//
//    
//-----------------------------------------------------------------------------


#ifndef __M_RANDOM__
#define __M_RANDOM__


#include "doomtype.h"



// Returns a number from 0 to 255,
// from a lookup table.
int M_Random (void);

// As M_Random, but used only by the play simulation.
int P_Random (void);

// Fix randoms for demos.
void M_ClearRandom (void);

// Diagnostic runner only: count existing call sites, without changing RNG code.
#ifdef DOOM_MONSTER_BENCHMARK
int MP_BenchRandom(void);
int MP_BenchMiscRandom(void);
#define P_Random MP_BenchRandom
#define M_Random MP_BenchMiscRandom
#endif


#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
