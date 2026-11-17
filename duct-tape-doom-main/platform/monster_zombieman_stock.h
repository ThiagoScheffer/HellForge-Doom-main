// Stock Zombieman (POSS) sprite signatures: generated from a user-owned IWAD.
// The empty default intentionally rejects unverified artwork. No art assets.
#ifndef HELLFORGE_MONSTER_ZOMBIEMAN_STOCK_H
#define HELLFORGE_MONSTER_ZOMBIEMAN_STOCK_H
#include <cstdint>
namespace doom_zombieman_stock {
struct Entry {int width,height; uint32_t hash;};
static constexpr Entry entries[]={ {0,0,0u} }; // Generated entries replace sentinel.
inline bool matches(int w,int h,uint32_t hash) {
    for(const Entry &e:entries)if(e.width==w&&e.height==h&&e.hash==hash)return true;
    return false;
}
}
#endif
