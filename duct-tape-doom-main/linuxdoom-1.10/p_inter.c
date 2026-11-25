#include "i_lan.h"
#include "m_arcade.h"
#include "p_arena.h"
#include "m_arcade.h"
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
//	Handling interactions (i.e., collisions).
//
//-----------------------------------------------------------------------------


static const char
rcsid[] = "$Id: p_inter.c,v 1.4 1997/02/03 22:45:11 b1 Exp $";


// Data.
#include "p_notices.h"
#include "doomdef.h"
#include "dstrings.h"
#include "sounds.h"

#include "doomstat.h"

#include "m_random.h"
#include "i_system.h"

#include "am_map.h"

#include "p_local.h"
#include "p_acid.h"
#include "p_incendiary.h"
#include "p_encounter.h"
#include "r_state.h"

#include "s_sound.h"

#ifdef __GNUG__
#pragma implementation "p_inter.h"
#endif
#include "p_inter.h"
#include "p_drop_policy.h"
#include "p_health_recovery.h"
#include <stdio.h>
#include "d_deh.h"

unsigned p_toasty_serial;
int p_toasty_tic;
int p_toasty_preview;


#define BONUSADD	6




// a weapon is found with two clip loads,
// a big item has five clip loads
int	maxammo[NUMAMMO] = {200, 50, 300, 50};
extern int p_boostchaingun;
int P_BoostChaingunEnabled(void){return p_boostchaingun&&!netgame&&!demoplayback&&!demorecording&&!arcade_active;}
void P_UpdateChaingunAmmo(player_t *player){
    int cap=maxammo[am_clip]*(player->backpack?(P_BoostChaingunEnabled()?3:2):1);
    player->maxammo[am_clip]=cap;
    if(player->ammo[am_clip]>cap)player->ammo[am_clip]=cap;
}

int	clipammo[NUMAMMO] = {10, 4, 20, 1};

static int monsterWeaponDrops;
// These tags identify the quantity in an already-spawned drop, independent
// of the player's current setting and DeHackEd clip sizes.
enum { DROP_AMMO_FULL = 0x4841, DROP_AMMO_SCARCE = 0x4842 };

void P_SetMonsterWeaponDrops(int mode)
{
    monsterWeaponDrops = mode == MONSTER_DROPS_AMMO || mode == MONSTER_DROPS_SCARCE
        ? mode : MONSTER_DROPS_CLASSIC;
}

boolean P_GiveBody(player_t *player, int num);

static int healthRecoveryMode;
void P_SetHealthRecovery(int mode)
{
    healthRecoveryMode = mode == HEALTH_RECOVERY_MEDIKITS || mode == HEALTH_RECOVERY_ALL
        ? mode : HEALTH_RECOVERY_CLASSIC;
}
int P_HealthRecoveryMode(void) { return healthRecoveryMode; }
int P_HealthRecoveryAllowed(void)
{
    return !netgame && !lan_session && !deathmatch && !demoplayback && !demorecording && !arcade_active;
}

enum { RECOVERY_TAG_BASE = 0x4d00, RECOVERY_TAG_MASK = 0xff00, RECOVERY_MAX_POINTS = 24 };
static boolean P_HasRecoveryTag(const mobj_t *item)
{
    return item && (item->monsterAmmoDrop & RECOVERY_TAG_MASK) == RECOVERY_TAG_BASE;
}
static int P_RecoveryPackHealth(const mobj_t *item)
{
    int points;
    if (!P_HasRecoveryTag(item) || item->type != MT_MISC10 || item->sprite != SPR_STIM ||
        (item->flags & (MF_SPECIAL | MF_DROPPED)) != (MF_SPECIAL | MF_DROPPED) ||
        item->state != &states[S_STIM]) return 0;
    points = item->monsterAmmoDrop & 0xff;
    return points >= 1 && points <= RECOVERY_MAX_POINTS ? points : 0;
}
static void P_SetRecoveryPackHealth(mobj_t *item, int points)
{
    item->monsterAmmoDrop = (unsigned short)(RECOVERY_TAG_BASE | points);
}
typedef struct { int restored, remaining; } medical_recovery_t;
static medical_recovery_t P_CalculateMedicalRecovery(int health, int capacity)
{
    medical_recovery_t result = {0, capacity > 0 ? capacity : 0};
    if (health > 0 && health < MAXHEALTH && capacity > 0) {
        int missing = MAXHEALTH - health;
        result.restored = capacity < missing ? capacity : missing;
        result.remaining -= result.restored;
    }
    return result;
}
/* Only mutate inert, stock-compatible actors; never run a patched state action. */
static boolean P_MedicalConversionSafe(const mobj_t *item)
{
    const mobjinfo_t *src = &mobjinfo[item->type], *dst = &mobjinfo[MT_MISC10];
    const state_t *a = &states[src->spawnstate], *b = &states[S_STIM];
    return (item->type == MT_MISC10 || item->type == MT_MISC11) &&
        src->spawnstate == (item->type == MT_MISC10 ? S_STIM : S_MEDI) &&
        dst->spawnstate == S_STIM && item->state == a &&
        a->sprite == (item->type == MT_MISC10 ? SPR_STIM : SPR_MEDI) &&
        b->sprite == SPR_STIM && a->frame == 0 && b->frame == 0 &&
        a->tics == -1 && b->tics == -1 && !a->action.acp1 && !b->action.acp1 &&
        src->flags == MF_SPECIAL && dst->flags == MF_SPECIAL &&
        item->radius == dst->radius && item->height == dst->height &&
        src->radius == dst->radius && src->height == dst->height &&
        !(item->flags & (MF_NOSECTOR | MF_NOBLOCKMAP));
}
typedef enum { MEDICAL_NOT_HANDLED, MEDICAL_REJECTED, MEDICAL_CONSUMED, MEDICAL_RETAINED } medical_pickup_result_t;
static char recoveryMessages[MAXPLAYERS][80];
static medical_pickup_result_t P_TouchMedicalRecovery(mobj_t *item, player_t *player)
{
    int pack = P_RecoveryPackHealth(item), capacity, index;
    boolean medikit;
    medical_recovery_t result;
    /* A malformed tag must never fall through to standard Stimpack healing. */
    if (item->type == MT_MISC10 && item->sprite == SPR_STIM && P_HasRecoveryTag(item) && !pack) return MEDICAL_REJECTED;
    if (pack && !P_HealthRecoveryAllowed()) return MEDICAL_REJECTED;
    medikit = item->type == MT_MISC11 && item->sprite == SPR_MEDI;
    if (!pack && (!P_HealthRecoveryAllowed() || healthRecoveryMode == HEALTH_RECOVERY_CLASSIC ||
        (!medikit && !(healthRecoveryMode == HEALTH_RECOVERY_ALL && item->type == MT_MISC10 && item->sprite == SPR_STIM))))
        return MEDICAL_NOT_HANDLED;
    capacity = pack ? pack : medikit ? 25 : 10;
    result = P_CalculateMedicalRecovery(player->health, capacity);
    if (!result.restored) return MEDICAL_REJECTED;
    if (result.remaining && !pack && !P_MedicalConversionSafe(item)) return MEDICAL_NOT_HANDLED;
    if (!P_GiveBody(player, result.restored)) return MEDICAL_REJECTED;
    if (result.remaining) {
        if (item->flags & MF_COUNTITEM) player->itemcount++;
        item->flags = (item->flags | MF_SPECIAL | MF_DROPPED) & ~MF_COUNTITEM;
        item->type = MT_MISC10;
        item->info = &mobjinfo[MT_MISC10];
        item->state = &states[S_STIM];
        item->sprite = item->state->sprite;
        item->frame = item->state->frame;
        item->tics = item->state->tics;
        P_SetRecoveryPackHealth(item, result.remaining);
    }
    if (pack || result.remaining) {
        index = (int)(player - players);
        if (index >= 0 && index < MAXPLAYERS) {
            const char *label = pack ? "RECOVERY" : medikit ? "MEDIKIT" : "STIMPACK";
            if (result.remaining)
                snprintf(recoveryMessages[index], sizeof(recoveryMessages[index]), "%s +%d HP. %d HP LEFT.", label, result.restored, result.remaining);
            else snprintf(recoveryMessages[index], sizeof(recoveryMessages[index]), "RECOVERY +%d HP.", result.restored);
            player->message = recoveryMessages[index];
        }
    } else if (medikit) player->message = DEH_String(player->health < 25 ? GOTMEDINEED : GOTMEDIKIT);
    else player->message = DEH_String(GOTSTIM);
    return result.remaining ? MEDICAL_RETAINED : MEDICAL_CONSUMED;
}

typedef struct {
    mobjtype_t type;
    unsigned short ammoTag;
} monster_death_drop_t;

static monster_death_drop_t P_MonsterDeathDrop(mobjtype_t monster)
{
    if (monster == MT_POSSESSED || monster == MT_WOLFSS)
        return (monster_death_drop_t){MT_CLIP, 0};
    if (monster != MT_SHOTGUY && monster != MT_CHAINGUY)
        return (monster_death_drop_t){NUMMOBJTYPES, 0};
    if (monsterWeaponDrops == MONSTER_DROPS_CLASSIC || netgame || lan_session ||
        deathmatch || demoplayback || demorecording || arcade_active)
        return (monster_death_drop_t){monster == MT_SHOTGUY ? MT_SHOTGUN : MT_CHAINGUN, 0};
    return (monster_death_drop_t){
        monster == MT_SHOTGUY ? MT_MISC22 : MT_CLIP,
        monsterWeaponDrops == MONSTER_DROPS_SCARCE ? DROP_AMMO_SCARCE : DROP_AMMO_FULL
    };
}

static int P_MonsterDropAmmo(const mobj_t *item, ammotype_t *ammo)
{
    if (!(item->flags & MF_DROPPED) ||
        (item->monsterAmmoDrop != DROP_AMMO_FULL && item->monsterAmmoDrop != DROP_AMMO_SCARCE))
        return 0;
    const boolean scarce = item->monsterAmmoDrop == DROP_AMMO_SCARCE;
    if (item->type == MT_MISC22) {
        *ammo = am_shell;
        return scarce ? 2 : 4;
    }
    if (item->type == MT_CLIP) {
        *ammo = am_clip;
        return scarce ? 5 : 10;
    }
    return 0;
}

//
// GET STUFF
//

//
// P_GiveAmmo
// Num is the number of clip loads,
// not the individual count (0= 1/2 clip).
// Returns false if the ammo can't be picked up at all
//

static boolean P_GiveAmmoInternal(player_t *player, ammotype_t ammo, int num,
                                 boolean clipLoads, boolean autoSwitch)
{
    int		oldammo;
	
    if (ammo == am_noammo)
	return false;
		
    if (ammo < 0 || ammo > NUMAMMO)
	I_Error ("P_GiveAmmo: bad type %i", ammo);
		
    if ( player->ammo[ammo] == player->maxammo[ammo]  )
	return false;
		
    if (clipLoads)
	num = num ? num * clipammo[ammo] : clipammo[ammo] / 2;
    
    if (gameskill == sk_baby
	|| gameskill == sk_nightmare)
    {
	// give double ammo in trainer mode,
	// you'll need in nightmare
	num <<= 1;
    }
    
		
    oldammo = player->ammo[ammo];
    player->ammo[ammo] += num;

    if (player->ammo[ammo] > player->maxammo[ammo])
	player->ammo[ammo] = player->maxammo[ammo];

    // If non zero ammo, 
    // don't change up weapons,
    // player was lower on purpose.
    if (!autoSwitch || oldammo)
	return true;	

    // We were down to zero,
    // so select a new weapon.
    // Preferences are not user selectable.
    switch (ammo)
    {
      case am_clip:
	if (player->readyweapon == wp_fist)
	{
	    if (player->weaponowned[wp_chaingun])
		player->pendingweapon = wp_chaingun;
	    else
		player->pendingweapon = wp_pistol;
	}
	break;
	
      case am_shell:
	if (player->readyweapon == wp_fist
	    || player->readyweapon == wp_pistol)
	{
	    if (player->weaponowned[wp_shotgun])
		player->pendingweapon = wp_shotgun;
	}
	break;
	
      case am_cell:
	if (player->readyweapon == wp_fist
	    || player->readyweapon == wp_pistol)
	{
	    if (player->weaponowned[wp_plasma])
		player->pendingweapon = wp_plasma;
	}
	break;
	
      case am_misl:
	if (player->readyweapon == wp_fist)
	{
	    if (player->weaponowned[wp_missile])
		player->pendingweapon = wp_missile;
	}
      default:
	break;
    }
	
    return true;
}

boolean P_GiveAmmo(player_t *player, ammotype_t ammo, int num)
{
    return P_GiveAmmoInternal(player, ammo, num, true, true);
}

//
// P_GiveWeapon
// The weapon name may have a MF_DROPPED flag ored in.
//
boolean
P_GiveWeapon
( player_t*	player,
  weapontype_t	weapon,
  boolean	dropped )
{
    boolean	gaveammo;
    boolean	gaveweapon;
	
    if (netgame
	&& (deathmatch!=2)
	 && !dropped )
    {
	// leave placed weapons forever on net games
	if (player->weaponowned[weapon])
	    return false;

	player->bonuscount += BONUSADD;
	player->weaponowned[weapon] = true;
        P_NoticeWeapon(player,weapon);

	if (deathmatch)
	    P_GiveAmmo (player, weaponinfo[weapon].ammo, 5);
	else
	    P_GiveAmmo (player, weaponinfo[weapon].ammo, 2);
	player->pendingweapon = weapon;

	if (player == &players[consoleplayer])
	    S_StartSound (NULL, sfx_wpnup);
	return false;
    }
	
    if (weaponinfo[weapon].ammo != am_noammo)
    {
	// give one clip with a dropped weapon,
	// two clips with a found weapon
	if (dropped)
	    gaveammo = P_GiveAmmo (player, weaponinfo[weapon].ammo, 1);
	else
	    gaveammo = P_GiveAmmo (player, weaponinfo[weapon].ammo, 2);
    }
    else
	gaveammo = false;
	
    if (player->weaponowned[weapon])
	gaveweapon = false;
    else
    {
	gaveweapon = true;
	player->weaponowned[weapon] = true;
        P_NoticeWeapon(player,weapon);
	player->pendingweapon = weapon;
    }
	
    return (gaveweapon || gaveammo);
}

 

//
// P_GiveBody
// Returns false if the body isn't needed at all
//
boolean
P_GiveBody
( player_t*	player,
  int		num )
{
    if (player->health >= MAXHEALTH)
	return false;
		
    player->health += num;
    if (player->health > MAXHEALTH)
	player->health = MAXHEALTH;
    player->mo->health = player->health;
	
    return true;
}



//
// P_GiveArmor
// Returns false if the armor is worse
// than the current armor.
//
boolean
P_GiveArmor
( player_t*	player,
  int		armortype )
{
    int		hits;
	
    hits = armortype*100;
    if (player->armorpoints >= hits)
	return false;	// don't pick up
		
    player->armortype = armortype;
    player->armorpoints = hits;
	
    return true;
}



//
// P_GiveCard
//
/* Presentation-only events: not saved and never used by gameplay. */
unsigned p_keypickup_serial[MAXPLAYERS];
int p_keypickup_card[MAXPLAYERS], p_keypickup_tic[MAXPLAYERS];
void
P_GiveCard
( player_t*	player,
  card_t	card )
{
    if (player->cards[card])
	return;
    
    player->bonuscount = BONUSADD;
    player->cards[card] = 1;
    P_ArenaKey(player);
    { int index;
      for(index=0;index<MAXPLAYERS;++index) if(player==&players[index]) {
        p_keypickup_card[index] = card;
        p_keypickup_tic[index] = leveltime;
        ++p_keypickup_serial[index];
        break;
      }
    }
}


//
// P_GivePower
//
boolean
P_GivePower
( player_t*	player,
  int /*powertype_t*/	power )
{
    if (power == pw_invulnerability)
    {
	player->powers[power] = INVULNTICS;
	return true;
    }
    
    if (power == pw_invisibility)
    {
	player->powers[power] = INVISTICS;
	player->mo->flags |= MF_SHADOW;
	return true;
    }
    
    if (power == pw_infrared)
    {
	player->powers[power] = INFRATICS;
	return true;
    }
    
    if (power == pw_ironfeet)
    {
	player->powers[power] = IRONTICS;
	return true;
    }
    
    if (power == pw_strength)
    {
	P_GiveBody (player, 100);
	player->powers[power] = 1;
	return true;
    }
	
    if (player->powers[power])
	return false;	// already got it
		
    player->powers[power] = 1;
    return true;
}



//
// P_TouchSpecialThing
//
void
P_TouchSpecialThing
( mobj_t*	special,
  mobj_t*	toucher )
{
    player_t*	player;
    int		i;
    fixed_t	delta;
    int		sound;
		
    delta = special->z - toucher->z;

    if (delta > toucher->height
	|| delta < -8*FRACUNIT)
    {
	// out of reach
	return;
    }
    
	
    sound = sfx_itemup;	
    player = toucher->player;

    // Dead thing touching.
    // Can happen with a sliding player corpse.
    if (toucher->health <= 0)
	return;

    medical_pickup_result_t medical = P_TouchMedicalRecovery(special, player);
    if (medical == MEDICAL_REJECTED) return;
    if (medical == MEDICAL_RETAINED) {
        player->bonuscount += BONUSADD;
        if (player == &players[consoleplayer]) S_StartSound(NULL, sound);
        return;
    }
    ammotype_t ammo;
    const int amount = medical == MEDICAL_CONSUMED ? 0 : P_MonsterDropAmmo(special, &ammo);
    if (amount) {
        if (!P_GiveAmmoInternal(player, ammo, amount, false, false))
            return;
        player->message = DEH_String(ammo == am_shell ? "Picked up shotgun shells." : GOTCLIP);
    }
    // Ordinary pickups retain their sprite-based behavior.
    else if (medical != MEDICAL_CONSUMED) switch (special->sprite)
    {
	// armor
      case SPR_ARM1:
	if (!P_GiveArmor (player, deh_green_armor_class))
	    return;
	player->message = DEH_String (GOTARMOR);
	break;
		
      case SPR_ARM2:
	if (!P_GiveArmor (player, deh_blue_armor_class))
	    return;
	player->message = DEH_String (GOTMEGA);
	break;
	
	// bonus items
      case SPR_BON1:
	player->health++;		// can go over 100%
	if (player->health > deh_max_health)
	    player->health = deh_max_health;
	player->mo->health = player->health;
	player->message = DEH_String (GOTHTHBONUS);
	break;
	
      case SPR_BON2:
	player->armorpoints++;		// can go over 100%
	if (player->armorpoints > deh_max_armor)
	    player->armorpoints = deh_max_armor;
	if (!player->armortype)
	    player->armortype = deh_green_armor_class;
	player->message = DEH_String (GOTARMBONUS);
	break;
	
      case SPR_SOUL:
	player->health += deh_soulsphere_health;
	if (player->health > deh_max_soulsphere)
	    player->health = deh_max_soulsphere;
	player->mo->health = player->health;
	player->message = DEH_String (GOTSUPER);
	sound = sfx_getpow;
	break;
	
      case SPR_MEGA:
	if (gamemode != commercial)
	    return;
	player->health = deh_megasphere_health;
	player->mo->health = player->health;
	P_GiveArmor (player, deh_blue_armor_class);
	player->message = DEH_String (GOTMSPHERE);
	sound = sfx_getpow;
	break;
	
	// cards
	// leave cards for everyone
      case SPR_BKEY:
	AM_KeyCollected(special, (int)(player-players));
	if (!player->cards[it_bluecard])
	    player->message = DEH_String (GOTBLUECARD);
	P_GiveCard (player, it_bluecard);
	if (!netgame)
	    break;
	return;
	
      case SPR_YKEY:
	AM_KeyCollected(special, (int)(player-players));
	if (!player->cards[it_yellowcard])
	    player->message = DEH_String (GOTYELWCARD);
	P_GiveCard (player, it_yellowcard);
	if (!netgame)
	    break;
	return;
	
      case SPR_RKEY:
	AM_KeyCollected(special, (int)(player-players));
	if (!player->cards[it_redcard])
	    player->message = DEH_String (GOTREDCARD);
	P_GiveCard (player, it_redcard);
	if (!netgame)
	    break;
	return;
	
      case SPR_BSKU:
	AM_KeyCollected(special, (int)(player-players));
	if (!player->cards[it_blueskull])
	    player->message = DEH_String (GOTBLUESKUL);
	P_GiveCard (player, it_blueskull);
	if (!netgame)
	    break;
	return;
	
      case SPR_YSKU:
	AM_KeyCollected(special, (int)(player-players));
	if (!player->cards[it_yellowskull])
	    player->message = DEH_String (GOTYELWSKUL);
	P_GiveCard (player, it_yellowskull);
	if (!netgame)
	    break;
	return;
	
      case SPR_RSKU:
	AM_KeyCollected(special, (int)(player-players));
	if (!player->cards[it_redskull])
	    player->message = DEH_String (GOTREDSKULL);
	P_GiveCard (player, it_redskull);
	if (!netgame)
	    break;
	return;
	
	// medikits, heals
      case SPR_STIM:
	if (!P_GiveBody (player, 10))
	    return;
	player->message = DEH_String (GOTSTIM);
	break;
	
      case SPR_MEDI:
	if (!P_GiveBody (player, 25))
	    return;

	if (player->health < 25)
	    player->message = DEH_String (GOTMEDINEED);
	else
	    player->message = DEH_String (GOTMEDIKIT);
	break;

	
	// power ups
      case SPR_PINV:
	if (!P_GivePower (player, pw_invulnerability))
	    return;
	player->message = DEH_String (GOTINVUL);
	sound = sfx_getpow;
	break;
	
      case SPR_PSTR:
	if (!P_GivePower (player, pw_strength))
	    return;
	player->message = DEH_String (GOTBERSERK);
	if (player->readyweapon != wp_fist)
	    player->pendingweapon = wp_fist;
	sound = sfx_getpow;
	break;
	
      case SPR_PINS:
	if (!P_GivePower (player, pw_invisibility))
	    return;
	player->message = DEH_String (GOTINVIS);
	sound = sfx_getpow;
	break;
	
      case SPR_SUIT:
	if (!P_GivePower (player, pw_ironfeet))
	    return;
	player->message = DEH_String (GOTSUIT);
	sound = sfx_getpow;
	break;
	
      case SPR_PMAP:
	if (!P_GivePower (player, pw_allmap))
	    return;
	player->message = DEH_String (GOTMAP);
	sound = sfx_getpow;
	break;
	
      case SPR_PVIS:
	if (!P_GivePower (player, pw_infrared))
	    return;
	player->message = DEH_String (GOTVISOR);
	sound = sfx_getpow;
	break;
	
	// ammo
      case SPR_CLIP:
	if (special->flags & MF_DROPPED)
	{
	    if (!P_GiveAmmo (player,am_clip,0))
		return;
	}
	else
	{
	    if (!P_GiveAmmo (player,am_clip,1))
		return;
	}
	player->message = DEH_String (GOTCLIP);
	break;
	
      case SPR_AMMO:
	if (!P_GiveAmmo (player, am_clip,5))
	    return;
	player->message = DEH_String (GOTCLIPBOX);
	break;
	
      case SPR_ROCK:
	if (!P_GiveAmmo (player, am_misl,1))
	    return;
	player->message = DEH_String (GOTROCKET);
	break;
	
      case SPR_BROK:
	if (!P_GiveAmmo (player, am_misl,5))
	    return;
	player->message = DEH_String (GOTROCKBOX);
	break;
	
      case SPR_CELL:
	if (!P_GiveAmmo (player, am_cell,1))
	    return;
	player->message = DEH_String (GOTCELL);
	break;
	
      case SPR_CELP:
	if (!P_GiveAmmo (player, am_cell,5))
	    return;
	player->message = DEH_String (GOTCELLBOX);
	break;
	
      case SPR_SHEL:
	if (!P_GiveAmmo (player, am_shell,1))
	    return;
	player->message = DEH_String (GOTSHELLS);
	break;
	
      case SPR_SBOX:
	if (!P_GiveAmmo (player, am_shell,5))
	    return;
	player->message = DEH_String (GOTSHELLBOX);
	break;
	
      case SPR_BPAK:
	if (!player->backpack)
	{
	    for (i=0 ; i<NUMAMMO ; i++)
		player->maxammo[i] *= 2;
	    player->backpack = true;
	}
	P_UpdateChaingunAmmo(player);
	for (i=0 ; i<NUMAMMO ; i++)
	    P_GiveAmmo (player, i, 1);
	player->message = DEH_String (GOTBACKPACK);
	break;
	
	// weapons
      case SPR_BFUG:
	if (!P_GiveWeapon (player, wp_bfg, false) )
	    return;
	player->message = DEH_String (GOTBFG9000);
	sound = sfx_wpnup;	
	break;
	
      case SPR_MGUN:
	if (!P_GiveWeapon (player, wp_chaingun, special->flags&MF_DROPPED) )
	    return;
	player->message = DEH_String (GOTCHAINGUN);
	sound = sfx_wpnup;	
	break;
	
      case SPR_CSAW:
	if (!P_GiveWeapon (player, wp_chainsaw, false) )
	    return;
	player->message = DEH_String (GOTCHAINSAW);
	sound = sfx_wpnup;	
	break;
	
      case SPR_LAUN:
	if (!P_GiveWeapon (player, wp_missile, false) )
	    return;
	player->message = DEH_String (GOTLAUNCHER);
	sound = sfx_wpnup;	
	break;
	
      case SPR_PLAS:
	if (!P_GiveWeapon (player, wp_plasma, false) )
	    return;
	player->message = DEH_String (GOTPLASMA);
	sound = sfx_wpnup;	
	break;
	
      case SPR_SHOT:
	if (!P_GiveWeapon (player, wp_shotgun, special->flags&MF_DROPPED ) )
	    return;
	player->message = DEH_String (GOTSHOTGUN);
	sound = sfx_wpnup;	
	break;
		
      case SPR_SGN2:
	if (!P_GiveWeapon (player, wp_supershotgun, special->flags&MF_DROPPED ) )
	    return;
	player->message = DEH_String (GOTSHOTGUN2);
	sound = sfx_wpnup;	
	break;
		
      default:
	I_Error ("P_SpecialThing: Unknown gettable thing");
    }
	
    if (!amount) switch(special->sprite){
    case SPR_SOUL:P_Notice((int)(player-players),N_SOUL,0);break;
    case SPR_MEGA:P_Notice((int)(player-players),N_MEGA,0);break;
    case SPR_PINV:P_Notice((int)(player-players),N_INVUL,0);break;
    case SPR_PSTR:P_Notice((int)(player-players),N_BERSERK,0);break;
    case SPR_PINS:P_Notice((int)(player-players),N_INVIS,0);break;
    case SPR_SUIT:P_Notice((int)(player-players),N_SUIT,0);break;
    case SPR_PMAP:P_Notice((int)(player-players),N_MAP,0);break;
    case SPR_PVIS:P_Notice((int)(player-players),N_VISOR,0);break;
    default:break;
    }
    if (special->flags & MF_COUNTITEM)
	player->itemcount++;
    Arcade_Pickup(player,special);
    P_RemoveMobj (special);
    player->bonuscount += BONUSADD;
    if (player == &players[consoleplayer])
	S_StartSound (NULL, sound);
}


//
// KillMobj
//
void
P_KillMobj
( mobj_t*	source,
  mobj_t*	target )
{
    mobj_t*	mo;

    if (R_RevenantDeath && target->type == MT_UNDEAD && !(target->flags & MF_CORPSE))
        R_RevenantDeath(target);
	
    ED_Died(target);
    if (R_GoreForget) R_GoreForget(target,1);
    P_WeakpointDied(target);
    Arcade_Kill(source,target);
    target->flags &= ~(MF_SHOOTABLE|MF_FLOAT|MF_SKULLFLY);

    if (target->type != MT_SKULL)
	target->flags &= ~MF_NOGRAVITY;

    target->flags |= MF_CORPSE|MF_DROPOFF;
    target->height >>= 2;

    if (source && source->player)
    {
	// count for intermission
	if (target->flags & MF_COUNTKILL)
	    source->player->killcount++;	

	if (target->player)
	    source->player->frags[target->player-players]++;
    }
    else if (!netgame && (target->flags & MF_COUNTKILL) )
    {
	// count all monster deaths,
	// even those caused by other monsters
	players[0].killcount++;
    }
    
    if (target->player)
    {
	// count environment kills against you
	if (!source)	
	    target->player->frags[target->player-players]++;
			
	target->flags &= ~MF_SOLID;
	target->player->playerstate = PST_DEAD;
	P_DropWeapon (target->player);

	if (target->player == &players[consoleplayer]
	    && automapactive)
	{
	    // don't die in auto map,
	    // switch view prior to dying
	    AM_Stop ();
	}
	
    }

    P_PhotoEvent(4,target);
    if (target->health < -target->info->spawnhealth 
	&& target->info->xdeathstate)
    {
	P_PhotoEvent(16,target);
	P_SetMobjState (target, target->info->xdeathstate);
    }
    else
	P_SetMobjState (target, target->info->deathstate);
    target->tics -= P_Random()&3;

    if (target->tics < 1)
	target->tics = 1;
		
    //	I_StartSound (&actor->r, actor->info->deathsound);


    // Drop stuff.
    // This determines the kind of object spawned
    // during the death frame of a thing.
    const monster_death_drop_t drop = P_MonsterDeathDrop(target->type);
    if (drop.type == NUMMOBJTYPES)
        return;
    mo = P_SpawnMobj (target->x,target->y,ONFLOORZ, drop.type);
    mo->flags |= MF_DROPPED;	// special versions of items
    mo->monsterAmmoDrop = drop.ammoTag;
}




//
// P_DamageMobj
// Damages both enemies and players
// "inflictor" is the thing that caused the damage
//  creature or missile, can be NULL (slime, etc)
// "source" is the thing to target after taking damage
//  creature or NULL
// Source and inflictor are the same for melee attacks.
// Source can be NULL for slime, barrel explosions
// and other environmental stuff.
//
static void
damageMobj
( mobj_t*	target,
  mobj_t*	inflictor,
  mobj_t*	source,
  int 		damage,
  boolean acid,
  boolean oilFire )
{
    unsigned	ang;
    int		saved;
    player_t*	player;
    fixed_t	thrust;
    int		temp;
    int elementKind,previousBurn,healthBefore;
	
    if(lan_session&&!deathmatch&&!lan_friendlyfire&&source&&source!=target&&source->player&&target->player)return;

    if ( !(target->flags & MF_SHOOTABLE) )
	return;	// shouldn't happen...

    if(target->type==MT_MISC77&&P_IncendiaryDamage(target,inflictor,source,damage))return;
		
    if(!acid&&R_DamageHit&&!p_wp_bleed_damage&&!p_wp_dot_damage)R_DamageHit(target,inflictor,source,damage);

    if (target->health <= 0)
	return;

    if(!acid)damage=P_WeakpointAdjustDamage(target,inflictor,source,damage);

    if(!acid&&!p_wp_dot_damage)P_AggressiveContact(target,inflictor,source);

    if ( target->flags & MF_SKULLFLY )
    {
	target->momx = target->momy = target->momz = 0;
    }

    if (!acid&&R_DamageHit&&!p_wp_bleed_damage&&!p_wp_dot_damage)
	R_DamageHit (target, inflictor, source, damage);
	
    player = target->player;
    if (player && gameskill == sk_baby)
	damage >>= 1; 	// take half damage in trainer mode
		

    // Some close combat weapons should not
    // inflict thrust and push the victim out of reach,
    // thus kick away unless using the chainsaw.
    if (inflictor
	&& !(target->flags & MF_NOCLIP)
	&& (!source
	    || !source->player
	    || source->player->readyweapon != wp_chainsaw))
    {
	ang = R_PointToAngle2 ( inflictor->x,
				inflictor->y,
				target->x,
				target->y);
		
	thrust = damage*(FRACUNIT>>3)*100/target->info->mass;

	// make fall forwards sometimes
	if ( damage < 40
	     && damage > target->health
	     && target->z - inflictor->z > 64*FRACUNIT
	     && (P_Random ()&1) )
	{
	    ang += ANG180;
	    thrust *= 4;
	}
		
	ang >>= ANGLETOFINESHIFT;
	target->momx += FixedMul (thrust, finecosine[ang]);
	target->momy += FixedMul (thrust, finesine[ang]);
    }
    
    // player specific
    if (player)
    {
	// end of game hell hack
	if (target->subsector->sector->special == 11
	    && damage >= target->health)
	{
	    damage = target->health - 1;
	}
	

	// Below certain threshold,
	// ignore damage in GOD mode, or with INVUL power.
	if ( damage < 1000
	     && ( (player->cheats&CF_GODMODE)
		  || player->powers[pw_invulnerability] ) )
	{
	    return;
	}
	
	if (player->armortype)
	{
	    if (player->armortype == 1)
		saved = damage/3;
	    else
		saved = damage/2;
	    
	    if (player->armorpoints <= saved)
	    {
		// armor is used up
		saved = player->armorpoints;
		player->armortype = 0;
	    }
	    player->armorpoints -= saved;
	    damage -= saved;
	}
	player->health -= damage; 	// mirror mobj health here for Dave
	if (player->health < 0)
	    player->health = 0;
	
	P_NoticeDamage(player,damage,source);
	player->attacker = source;
	player->damagecount += damage;	// add damage after armor / invuln

	if (player->damagecount > 100)
	    player->damagecount = 100;	// teleport stomp does 10k points...
	
	temp = damage < 100 ? damage : 100;

	if (player == &players[consoleplayer])
	    I_Tactile (40,10,40+temp*2);
    }
    
    // do the damage	
    if(!acid&&!p_wp_bleed_damage&&!p_wp_dot_damage)P_AggressiveDamage(target,inflictor,source,damage);
    if(!acid&&damage>0)Arcade_Impact(inflictor,source,target);
    Arcade_Damage(source,target,damage);
    if(damage>0)P_PhotoEvent(2,target);
    elementKind=acid?0:oilFire?5:P_CombatDamageKind(inflictor,source);
    previousBurn=acid?0:P_CombatVisualKind(target);
    healthBefore=target->health;
    target->health -= damage;	
    if (!acid && target->type==MT_BARREL && damage>0)
        P_AcidBarrelDamaged(target,source,healthBefore,target->health);
    if(!acid)P_WeakpointRecordHealthLoss(target,source,healthBefore,target->health);
    P_SessionDamage(target,source,healthBefore,damage);
    if (target->health <= 0)
    {
	P_NoticeKill(source,target,inflictor);
	P_KillMobj (source, target);
        if(!acid)P_CombatConfirmed(target,source,elementKind,damage,previousBurn);
	return;
    }

    if(R_VileHurt&&target->type==MT_VILE&&target->health<healthBefore&&
       !p_wp_bleed_damage&&!p_wp_dot_damage)
        R_VileHurt(target);
    if(!acid)P_CombatConfirmed(target,source,elementKind,damage,-1);
    if(p_wp_bleed_damage||p_wp_dot_damage)return;
    if(damage>0&&!acid){P_WeakpointRetaliationDamage(target,inflictor,source);P_WeakpointFeedingHurtFrom(target,source);}

    if ( (P_Random () < target->info->painchance)
	 && !(target->flags&MF_SKULLFLY) )
    {
	target->flags |= MF_JUSTHIT;	// fight back!
	
	P_SetMobjState (target, target->info->painstate);
    }
			
    target->reactiontime = 0;		// we're awake now...	

    if ( !P_WeakpointBlind(target) && !P_WeakpointFrenzied(target) && !P_WeakpointFeeding(target) && (!target->threshold || target->type == MT_VILE)
	 && source && source != target
	 && source->type != MT_VILE)
    {
	// if not intent on another player,
	// chase after this one
	target->target = source;
	target->threshold = BASETHRESHOLD;
	if (target->state == &states[target->info->spawnstate]
	    && target->info->seestate != S_NULL)
	    P_SetMobjState (target, target->info->seestate);
    }
			
}

void P_DamageMobj(mobj_t *target,mobj_t *inflictor,mobj_t *source,int damage)
{
    damageMobj(target,inflictor,source,damage,false,false);
}

void P_DamageMobjAcid(mobj_t *target,mobj_t *source,int damage)
{
    damageMobj(target,NULL,source,damage,true,false);
}

void P_DamageMobjFire(mobj_t *target,mobj_t *inflictor,mobj_t *source,int damage)
{
    int previousDot=p_wp_dot_damage;
    if(!inflictor)p_wp_dot_damage=5; /* Periodic oil damage suppresses pain and weakpoint reactions. */
    damageMobj(target,inflictor,source,damage,false,true);
    p_wp_dot_damage=previousDot;
}
