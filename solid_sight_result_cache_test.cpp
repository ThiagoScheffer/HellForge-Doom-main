#include "duct-tape-doom-main/platform/solid_sight_result_cache.h"
#include <cassert>
#include <cstdio>
#include <algorithm>
#include <cstdint>

using Cache=doom_solid_sight::NegativeCache<std::int32_t>;
using Key=Cache::Key;
static Cache cache;

int main() {
    Key k{1,-2,3,56,10,20,30,56};
    assert(!cache.contains(k));
    cache.remember(k);
    assert(cache.contains(k));
    for(std::size_t i=0;i<k.size();++i){
        Key changed=k; ++changed[i];
        assert(!cache.contains(changed));
    }
    Key reversed=k;
    for(std::size_t i=0;i<4;++i)std::swap(reversed[i],reversed[4+i]);
    assert(!cache.contains(reversed));
    cache.invalidate();
    assert(!cache.contains(k));
    cache.remember(k);
    assert(cache.contains(k));
    cache.reset();
    assert(cache.stats().hits==0 && cache.stats().misses==0);
    assert(!cache.contains(k));
    std::puts("PASS: complete directional key, invalidation, reset");

    // Force all queries to the same bucket. Eviction may lose a hit but
    // must never create a false hit. Duplicate insertions do not evict.
    doom_solid_sight::NegativeCache<std::int32_t,1,2> collision;
    Key x{1},y{2},z{3};
    collision.remember(x);collision.remember(y);collision.remember(x);
    assert(collision.contains(x) && collision.contains(y));
    assert(!collision.contains(z));
    collision.remember(z);
    assert(!collision.contains(x) && collision.contains(y) && collision.contains(z));
    assert(collision.stats().replacements==1);
    collision.invalidate();
    assert(!collision.contains(y)&&!collision.contains(z));
    std::puts("PASS: full bucket collision, deterministic replacement, no false hit");

    // Synthetic workload: 1,817 exact stationary observer/target pairs,
    // two requests per simulated round, then a target movement and edit.
    cache.reset();
    unsigned computed=0;
    for(int round=0;round<40;++round){
        for(int i=0;i<1817;++i){
            Key q{(i-900)*65536,((i*37)%1000)*65536,0,56*65536,100*65536,200*65536,0,56*65536};
            for(int duplicate=0;duplicate<2;++duplicate){
                if(!cache.contains(q)){++computed;cache.remember(q);}
            }
        }
    }
    auto measured=cache.stats();
    assert(measured.hits+measured.misses==40u*1817u*2u);
    assert(measured.misses==computed);
    assert(measured.hits>140000);
    std::printf("PASS: synthetic stationary workload hits %llu misses %llu replacements %llu; bytes %zu\n",
        (unsigned long long)measured.hits,(unsigned long long)measured.misses,
        (unsigned long long)measured.replacements,sizeof(cache));
    Key q{-900*65536,0,0,56*65536,100*65536,200*65536,0,56*65536};
    Key moved=q; ++moved[4];
    assert(!cache.contains(moved));
    cache.invalidate();
    assert(!cache.contains(q));
    std::puts("PASS: exact target motion and geometry edit force recomputation");
}
