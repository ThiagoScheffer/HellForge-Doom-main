/* Independent corrosive-barrel gameplay.
   Gameplay state is authoritative here; the renderer only reads pool snapshots. */
#include "p_acid.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <vector>

extern "C" {
#include "p_local.h"
#include "doomstat.h"
#include "r_state.h"
}

extern "C" {
void (*P_AcidVisualBurst)(fixed_t,fixed_t,fixed_t,fixed_t)=nullptr;
void (*P_AcidVisualCoat)(mobj_t*,fixed_t,fixed_t)=nullptr;
void (*P_AcidVisualForget)(const mobj_t*)=nullptr;
void (*P_AcidVisualClear)(void)=nullptr;
}

namespace {
constexpr int acidDamage = 5;
constexpr int acidDamageInterval = 17; /* About twice per second. */
constexpr int acidAfterExposure = 70;  /* Two seconds after leaving a pool. */
constexpr int leakLifetime = 280;      /* Eight seconds. */
constexpr int spillLifetime = 350;     /* Ten seconds. */
constexpr int maxPools = 48;
constexpr uint32_t acidSaveVersion = 1;

struct AcidPool {
    fixed_t x = 0, y = 0, radius = 0;
    int sector = -1;
    int born = 0, expires = 0, kind = P_ACID_POOL_SPILL;
    mobj_t *owner = nullptr;  /* Non-null only while this is a barrel leak. */
    mobj_t *source = nullptr; /* Attacker attribution; cleared when removed. */
};
struct Exposure {
    int until = 0;
    int nextDamage = 0;
    mobj_t *source = nullptr;
};

bool enabled = false;
int lastPoolSweep = -1;
std::vector<AcidPool> pools;
std::unordered_map<mobj_t*, Exposure> exposures;
std::unordered_set<mobj_t*> rupturedBarrels;

void put32(std::vector<unsigned char> &out, uint32_t value) {
    for (int i = 0; i < 4; ++i)
        out.push_back((unsigned char)(value >> (i * 8)));
}
uint32_t get32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
void set32(std::vector<unsigned char> &out, size_t at, uint32_t value) {
    for (int i = 0; i < 4; ++i) out[at + i] = (unsigned char)(value >> (i * 8));
}

std::vector<mobj_t*> savedActors() {
    std::vector<mobj_t*> result;
    for (thinker_t *t = thinkercap.next; t != &thinkercap; t = t->next) {
        if (t->function.acp1 == (actionf_p1)P_MobjThinker)
            result.push_back((mobj_t*)t);
    }
    return result;
}

uint32_t actorId(const std::unordered_map<mobj_t*, uint32_t> &ids, mobj_t *actor) {
    if (!actor) return 0;
    auto it = ids.find(actor);
    return it == ids.end() ? 0 : it->second;
}

void sweepPools() {
    if (lastPoolSweep == leveltime) return;
    lastPoolSweep = leveltime;
    pools.erase(std::remove_if(pools.begin(), pools.end(),
        [](const AcidPool &pool) { return pool.expires <= leveltime; }), pools.end());
}

int sectorAt(fixed_t x, fixed_t y, int fallback) {
    subsector_t *sub = R_PointInSubsector(x, y);
    if (!sub || !sub->sector) return fallback;
    const int index = (int)(sub->sector - sectors);
    if (index < 0 || index >= numsectors) return fallback;
    return index;
}

void addPool(fixed_t x, fixed_t y, fixed_t radius, int sector, int kind,
             mobj_t *owner, mobj_t *source, int lifetime) {
    if (sector < 0 || sector >= numsectors || radius <= 0) return;
    sweepPools();
    if (pools.size() >= maxPools) {
        auto oldest = std::min_element(pools.begin(), pools.end(),
            [](const AcidPool &a, const AcidPool &b) { return a.born < b.born; });
        if (oldest != pools.end()) pools.erase(oldest);
    }
    pools.push_back({x, y, radius, sector, leveltime, leveltime + lifetime,
                     kind, owner, source});
}

bool touches(const mobj_t *actor, const AcidPool &pool) {
    if (!actor || !actor->subsector || !actor->subsector->sector) return false;
    const fixed_t floor = sectors[pool.sector].floorheight;
    if (std::llabs(int64_t(actor->z)-floor)>4LL*FRACUNIT ||
        int64_t(actor->z)>int64_t(actor->floorz)+4LL*FRACUNIT) return false;
    const double dx = ((double)actor->x - (double)pool.x) / FRACUNIT;
    const double dy = ((double)actor->y - (double)pool.y) / FRACUNIT;
    const double radius = actor->radius/double(FRACUNIT);
    const double reach=radius+pool.radius/double(FRACUNIT);
    const double distanceSquared=dx*dx+dy*dy;
    if(distanceSquared>reach*reach)return false;
    auto *origin=R_PointInSubsector(pool.x,pool.y);
    if(!origin||origin->sector!=&sectors[pool.sector])return false;
    if(actor->subsector->sector!=origin->sector) {
        // The pool remains clipped to its sector. An adjacent actor can
        // touch it only if its footprint overlaps that sector at this height.
        if(actor->subsector->sector->floorheight!=floor||distanceSquared==0)return false;
        const double edge=std::min(radius/std::sqrt(distanceSquared),1.0);
        if(sectorAt(fixed_t(actor->x-dx*edge*FRACUNIT),
                    fixed_t(actor->y-dy*edge*FRACUNIT),-1)!=pool.sector)return false;
    }
    if(!distanceSquared)return true;
    mobj_t from{},to{};
    from.x=pool.x;from.y=pool.y;
    from.z=fixed_t(std::min(int64_t(floor)+FRACUNIT,int64_t(INT32_MAX)-2*FRACUNIT));
    from.height=FRACUNIT;from.subsector=origin;
    to=from;to.x=actor->x;to.y=actor->y;to.subsector=actor->subsector;
    return P_CheckSight(&from,&to)!=0;
}

bool validVictim(const mobj_t *actor) {
    if (!actor || actor->health <= 0 || !(actor->flags & MF_SHOOTABLE) ||
        (actor->flags & (MF_CORPSE|MF_MISSILE)) || actor->type==MT_BARREL ||
        actor->type==MT_KEEN) return false; // The hanging switch actor is not hostile.
    if(actor->player||actor->type==MT_SKULL||(actor->flags&MF_COUNTKILL))return true;
    return actor->info&&actor->info->seestate!=S_NULL&&
           (actor->info->meleestate!=S_NULL||actor->info->missilestate!=S_NULL);
}

bool radiationProtected(const mobj_t *actor) {
    return actor && actor->player && actor->player->powers[pw_ironfeet] > 0;
}

/* The rupture throws liquid above floor level too. Mark visible nearby actors
   as splashed so flying monsters can be exposed even when they never touch a
   floor pool. Damage itself still flows through P_DamageMobj on their thinker. */
void splashExposure(mobj_t *barrel, mobj_t *source) {
    if (!barrel) return;
    for (thinker_t *t = thinkercap.next; t != &thinkercap; t = t->next) {
        if (t->function.acp1 != (actionf_p1)P_MobjThinker) continue;
        mobj_t *actor = (mobj_t*)t;
        if (actor == barrel || !validVictim(actor) || radiationProtected(actor)) continue;
        const double dx = ((double)actor->x - (double)barrel->x) / FRACUNIT;
        const double dy = ((double)actor->y - (double)barrel->y) / FRACUNIT;
        const double actorMid = (double)actor->z + (double)actor->height * 0.5;
        const double barrelMid = (double)barrel->z + (double)barrel->height * 0.5;
        const double dz = (actorMid - barrelMid) / FRACUNIT;
        const double reach = 64.0 + (double)actor->radius / FRACUNIT;
        if (dx*dx + dy*dy + dz*dz > reach*reach || !P_CheckSight(barrel, actor)) continue;
        if(!actor->player && P_AcidVisualCoat) P_AcidVisualCoat(actor,barrel->x,barrel->y);
        auto &exposure = exposures[actor];
        if(!exposure.nextDamage||exposure.until<leveltime)exposure.nextDamage = leveltime + acidDamageInterval;
        exposure.until = leveltime + acidAfterExposure;
        exposure.source = source;
    }
}

void spillOffset(mobj_t *barrel, double angle, fixed_t distance, fixed_t radius,
                 mobj_t *source) {
    const double dx = std::cos(angle) * distance;
    const double dy = std::sin(angle) * distance;
    const fixed_t x = (fixed_t)std::clamp<int64_t>(
        (int64_t)barrel->x + (int64_t)std::llround(dx), INT32_MIN, INT32_MAX);
    const fixed_t y = (fixed_t)std::clamp<int64_t>(
        (int64_t)barrel->y + (int64_t)std::llround(dy), INT32_MIN, INT32_MAX);
    const int sector = sectorAt(x, y, barrel->subsector && barrel->subsector->sector
                                      ? (int)(barrel->subsector->sector - sectors) : -1);
    if (sector < 0 || sector >= numsectors) return;
    const int64_t floorDelta = (int64_t)sectors[sector].floorheight -
                               (int64_t)barrel->floorz;
    if (std::llabs(floorDelta) > 16LL * FRACUNIT) return;
    mobj_t landing{};
    landing.x=x;landing.y=y;
    landing.z=fixed_t(std::min(int64_t(sectors[sector].floorheight)+FRACUNIT,int64_t(INT32_MAX)-2*FRACUNIT));
    landing.height=FRACUNIT;landing.subsector=R_PointInSubsector(x,y);
    if(!landing.subsector||landing.subsector->sector!=&sectors[sector]||
       !P_CheckSight(barrel,&landing))return;
    addPool(x, y, radius, sector, P_ACID_POOL_SPILL, nullptr, source, spillLifetime);
}
} // namespace

extern "C" int P_AcidEnabled(void) {
    return enabled && !netgame && !demoplayback && !demorecording;
}

extern "C" void P_AcidConfigure(int value) {
    const bool next = value != 0;
    if (enabled && !next) P_AcidClear();
    enabled = next;
}

extern "C" void P_AcidClear(void) {
    if(P_AcidVisualClear)P_AcidVisualClear();
    pools.clear();
    exposures.clear();
    rupturedBarrels.clear();
    lastPoolSweep = -1;
}

extern "C" void P_AcidBarrelDamaged(mobj_t *barrel, mobj_t *source, int healthBefore, int healthAfter) {
    if (!P_AcidEnabled() || !barrel || barrel->type != MT_BARREL ||
        healthAfter <= 0 || healthAfter >= healthBefore || !barrel->subsector ||
        !barrel->subsector->sector || rupturedBarrels.count(barrel)) return;

    const int sector = (int)(barrel->subsector->sector - sectors);
    if (sector < 0 || sector >= numsectors) return;

    const int spawnHealth = std::max(1, barrel->info ? barrel->info->spawnhealth : healthBefore);
    const int lost = std::clamp(spawnHealth - healthAfter, 0, spawnHealth);
    const fixed_t radius = (fixed_t)((12LL + 18LL * lost / spawnHealth) * FRACUNIT);
    auto found = std::find_if(pools.begin(), pools.end(),
        [barrel](const AcidPool &pool) {
            return pool.kind == P_ACID_POOL_LEAK && pool.owner == barrel;
        });
    if (found != pools.end()) {
        found->radius = std::max(found->radius, radius);
        found->born = leveltime;
        found->expires = leveltime + leakLifetime;
        if (source) found->source = source;
    } else {
        addPool(barrel->x, barrel->y, radius, sector, P_ACID_POOL_LEAK,
                barrel, source ? source : barrel->target, leakLifetime);
    }
}

extern "C" void P_AcidBarrelExplode(mobj_t *barrel, mobj_t *source) {
    if (!P_AcidEnabled() || !barrel || barrel->type != MT_BARREL ||
        !barrel->subsector || !barrel->subsector->sector ||
        !rupturedBarrels.insert(barrel).second) return;

    const int sector = (int)(barrel->subsector->sector - sectors);
    if (sector < 0 || sector >= numsectors) return;
    if (!source) source = barrel->target;
    auto leak = std::find_if(pools.begin(), pools.end(),
        [barrel](const AcidPool &pool) {
            return pool.kind == P_ACID_POOL_LEAK && pool.owner == barrel;
        });
    if(!source&&leak!=pools.end())source=leak->source;
    if(P_AcidVisualBurst)P_AcidVisualBurst(barrel->x,barrel->y,barrel->z,barrel->height);
    splashExposure(barrel,source);
    if (leak != pools.end()) {
        leak->radius = 48 * FRACUNIT;
        leak->born = leveltime;
        leak->expires = leveltime + spillLifetime;
        leak->kind = P_ACID_POOL_SPILL;
        leak->owner = nullptr;
        leak->source = source;
    } else {
        addPool(barrel->x, barrel->y, 48 * FRACUNIT, sector,
                P_ACID_POOL_SPILL, nullptr, source, spillLifetime);
    }

    const double base = (double)barrel->angle * (6.2831853071795864769 / 4294967296.0);
    for (int i = 0; i < 3; ++i) {
        const double angle = base + i * 2.0943951023931954923;
        spillOffset(barrel, angle, 30 * FRACUNIT, 25 * FRACUNIT, source);
    }
}

static bool acidTick(mobj_t *actor,int floorDamage) {
    if (!actor) return false;
    if (!P_AcidEnabled()) return false;
    sweepPools();

    /* A damaged, still-intact barrel continues to seep. The puddle is refreshed
       at a low cadence and spreads slowly; no per-frame particle allocation. */
    if (actor->type == MT_BARREL && actor->health > 0) {
        if (leveltime % 35 == 0) {
            for (AcidPool &pool : pools) {
                if (pool.kind == P_ACID_POOL_LEAK && pool.owner == actor &&
                    actor->subsector&&actor->subsector->sector) {
                    pool.x=actor->x;pool.y=actor->y;
                    pool.sector=int(actor->subsector->sector-sectors);
                    pool.expires = leveltime + leakLifetime;
                    pool.radius = std::min(pool.radius + FRACUNIT, 30 * FRACUNIT);
                }
            }
        }
        return false;
    }

    if(pools.empty()&&exposures.empty())return false;
    if (!validVictim(actor) || radiationProtected(actor)) {
        exposures.erase(actor);
        return false;
    }

    const AcidPool *contact = nullptr;
    for (const AcidPool &pool : pools) {
        if (pool.expires > leveltime && touches(actor, pool) &&
            (!contact || pool.radius > contact->radius))
            contact = &pool;
    }

    auto it = exposures.find(actor);
    if (contact) {
        if (it == exposures.end()) {
            Exposure state;
            state.until = leveltime + acidAfterExposure;
            state.nextDamage = leveltime + acidDamageInterval;
            state.source = contact->source;
            it = exposures.emplace(actor, state).first;
        } else {
            it->second.until = leveltime + acidAfterExposure;
            it->second.source = contact->source;
        }
    }
    if (it == exposures.end()) return false;
    if (leveltime > it->second.until) {
        exposures.erase(it);
        return false;
    }
    if (leveltime < it->second.nextDamage) return false;

    mobj_t *source = it->second.source;
    it->second.nextDamage = leveltime + acidDamageInterval;
    P_DamageMobjAcid(actor,source,std::max(acidDamage,floorDamage));
    if (actor->thinker.function.acv==(actionf_v)(-1)||actor->health<=0)exposures.erase(actor);
    return true;
}

extern "C" void P_AcidTick(mobj_t *actor) { acidTick(actor,0); }

extern "C" void P_AcidTickWithFloor(mobj_t *actor,int floorDamage) {
    if(!acidTick(actor,floorDamage))P_DamageMobjAcid(actor,nullptr,floorDamage);
}

extern "C" void P_AcidForget(mobj_t *actor) {
    if(P_AcidVisualForget)P_AcidVisualForget(actor);
    if (!actor) return;
    exposures.erase(actor);
    rupturedBarrels.erase(actor);
    for (AcidPool &pool : pools) {
        if (pool.owner == actor) {pool.owner = nullptr;pool.kind=P_ACID_POOL_SPILL;}
        if (pool.source == actor) pool.source = nullptr;
    }
    for (auto &entry : exposures)
        if (entry.second.source == actor) entry.second.source = nullptr;
}

extern "C" int P_AcidPoolCount(void) {
    if (!P_AcidEnabled()) return 0;
    sweepPools();
    return (int)pools.size();
}
extern "C" int P_AcidPoolGet(int index, p_acid_pool_view_t *out) {
    if (!out || index < 0 || !P_AcidEnabled()) return 0;
    sweepPools();
    if(size_t(index)>=pools.size())return 0;
    const AcidPool &pool=pools[size_t(index)];
    out->x = pool.x;
    out->y = pool.y;
    out->radius = pool.radius;
    out->sector = pool.sector;
    out->age = std::max(0, leveltime - pool.born);
    out->remaining = std::max(0, pool.expires - leveltime);
    out->kind = pool.kind;
    out->hasOwner = pool.owner && pool.owner->health>0;
    if (out->hasOwner) {
        out->ownerX = pool.owner->x;
        out->ownerY = pool.owner->y;
        out->ownerZ = pool.owner->z;
        out->ownerHeight = pool.owner->height;
    } else {
        out->ownerX = out->ownerY = out->ownerZ = out->ownerHeight = 0;
    }
    return 1;
}

extern "C" size_t P_AcidSave(unsigned char *destination) {
    if (!P_AcidEnabled()) return 0;
    sweepPools();
    for (auto it = exposures.begin(); it != exposures.end();) {
        if (!it->first || it->first->health <= 0 || it->second.until < leveltime)
            it = exposures.erase(it);
        else ++it;
    }
    if (pools.empty() && exposures.empty()) return 0;
    const std::vector<mobj_t*> actors = savedActors();
    std::unordered_map<mobj_t*, uint32_t> ids;
    ids.reserve(actors.size());
    for (size_t i = 0; i < actors.size(); ++i)
        ids.emplace(actors[i], (uint32_t)i + 1);

    std::vector<unsigned char> out;
    put32(out, acidSaveVersion);
    const size_t poolCountOffset = out.size(); put32(out, 0);
    const size_t exposureCountOffset = out.size(); put32(out, 0);
    put32(out, 0); /* Reserved for future format flags. */

    uint32_t poolCount = 0;
    for (const AcidPool &pool : pools) {
        put32(out, (uint32_t)pool.x);
        put32(out, (uint32_t)pool.y);
        put32(out, (uint32_t)pool.radius);
        put32(out, (uint32_t)pool.sector);
        put32(out, (uint32_t)std::clamp(pool.expires - leveltime, 0, spillLifetime));
        put32(out, (uint32_t)std::clamp(leveltime - pool.born, 0, spillLifetime));
        put32(out, (uint32_t)pool.kind);
        put32(out, actorId(ids, pool.owner));
        put32(out, actorId(ids, pool.source));
        ++poolCount;
    }
    set32(out, poolCountOffset, poolCount);

    uint32_t exposureCount = 0;
    for (size_t i=0;i<actors.size();++i) {
        auto it=exposures.find(actors[i]);
        if(it==exposures.end())continue;
        const auto &state=it->second;
        put32(out, uint32_t(i+1));
        put32(out, (uint32_t)std::clamp(state.until - leveltime, 0, acidAfterExposure));
        put32(out, (uint32_t)std::clamp(state.nextDamage - leveltime, 0, acidDamageInterval));
        put32(out, actorId(ids, state.source));
        ++exposureCount;
    }
    set32(out, exposureCountOffset, exposureCount);

    put32(out, (uint32_t)(out.size() + 8));
    out.insert(out.end(), {'A','C','0','1'});
    if (destination) std::memcpy(destination, out.data(), out.size());
    return out.size();
}

extern "C" size_t P_AcidLoad(const unsigned char *data, size_t length) {
    P_AcidClear();
    if (!data || length < 24 || std::memcmp(data + length - 4, "AC01", 4))
        return 0;
    const uint32_t bytes = get32(data + length - 8);
    if (bytes < 24 || bytes > length) return 0;
    const unsigned char *p = data + length - bytes;
    const uint32_t version = get32(p);
    const uint32_t poolCount = get32(p + 4);
    const uint32_t exposureCount = get32(p + 8);
    constexpr size_t poolStride = 36, exposureStride = 16;
    if (version != acidSaveVersion || poolCount > maxPools ||
        exposureCount > 100000 ||
        bytes != 24 + (size_t)poolCount * poolStride +
                 (size_t)exposureCount * exposureStride)
        return bytes;

    if (!P_AcidEnabled()) return bytes;
    const std::vector<mobj_t*> actors = savedActors();
    auto actorRef = [&](uint32_t id) -> mobj_t* {
        return id && id <= actors.size() ? actors[id - 1] : nullptr;
    };

    const unsigned char *record = p + 16;
    for (uint32_t i = 0; i < poolCount; ++i, record += poolStride) {
        AcidPool pool;
        pool.x = (fixed_t)get32(record);
        pool.y = (fixed_t)get32(record + 4);
        pool.radius = (fixed_t)get32(record + 8);
        pool.sector = (int)get32(record + 12);
        const uint32_t remaining = std::min(get32(record + 16), (uint32_t)spillLifetime);
        const uint32_t age = std::min(get32(record + 20), (uint32_t)spillLifetime);
        const uint32_t kind=get32(record+24);
        if(kind!=P_ACID_POOL_LEAK&&kind!=P_ACID_POOL_SPILL)continue;
        pool.kind=int(kind);
        pool.owner = actorRef(get32(record + 28));
        pool.source = actorRef(get32(record + 32));
        if (!remaining || pool.radius <= 0 || pool.radius > 96 * FRACUNIT ||
            pool.sector < 0 || pool.sector >= numsectors ||
            sectorAt(pool.x,pool.y,-1)!=pool.sector)
            continue;
        if (pool.kind == P_ACID_POOL_LEAK &&
            (!pool.owner || pool.owner->type != MT_BARREL || pool.owner->health<=0))continue;
        if(pool.kind==P_ACID_POOL_SPILL)pool.owner=nullptr;
        pool.born = leveltime - (int)age;
        pool.expires = leveltime + (int)remaining;
        pools.push_back(pool);
    }

    for (uint32_t i = 0; i < exposureCount; ++i, record += exposureStride) {
        mobj_t *actor = actorRef(get32(record));
        const uint32_t until = std::min(get32(record + 4), (uint32_t)acidAfterExposure);
        const uint32_t next = std::min(get32(record + 8), (uint32_t)acidDamageInterval);
        if (!validVictim(actor)||radiationProtected(actor))continue;
        Exposure state;
        state.until = leveltime + (int)until;
        state.nextDamage = leveltime + (int)next;
        state.source = actorRef(get32(record + 12));
        exposures[actor] = state;
    }
    lastPoolSweep = leveltime;
    return bytes;
}
