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
// DESCRIPTION:
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: m_argv.c,v 1.1 1997/02/03 22:45:10 b1 Exp $";


#include "i_compat.h"

int		myargc;
char**		myargv;




//
// M_CheckParm
// Checks for the given parameter
// in the program's command line arguments.
// Returns the argument number (1 to argc-1)
// or 0 if not present
#ifdef DOOM_MONSTER_PROFILE_ARGUMENTS
#include "../platform/monster_profile.h"
int M_CheckParm_mp_body(char *check);
int M_CheckParm(char *check){
    if(!mp_enabled)return M_CheckParm_mp_body(check);
    MP_Scope scope=MP_Begin(MP_ARGUMENTS);int result=M_CheckParm_mp_body(check);MP_End(scope);return result;
}
#define MP_ARGUMENT_BODY M_CheckParm_mp_body
#else
#define MP_ARGUMENT_BODY M_CheckParm
#endif
int MP_ARGUMENT_BODY (char *check)
{
    int		i;

    for (i = 1;i<myargc;i++)
    {
	if ( !strcasecmp(check, myargv[i]) )
	    return i;
    }

    return 0;
}




