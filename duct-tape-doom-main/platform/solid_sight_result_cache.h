#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

// Optional, exact negative solid-map LOS cache. No actor pointers, no spatial
// quantization, no expiration heuristics. The caller owns geometry invalidation.
namespace doom_solid_sight {
template<class Coordinate, std::size_t Buckets=4096, std::size_t Ways=4>
class NegativeCache {
    static_assert(Buckets && !(Buckets&(Buckets-1)),"power-of-two bucket count");
    static_assert(Ways>0 && Ways<=256,"bounded associativity");
public:
    using Key=std::array<Coordinate,8>;
    struct Stats {
        std::uint64_t hits=0,misses=0,stores=0,replacements=0,invalidations=0;
    };
    static constexpr std::size_t capacity=Buckets*Ways;
    const Stats& stats() const {return counters;}
    void invalidate() {
        ++counters.invalidations;
        if(++generation==0) {
            // Zero is reserved for an unused entry, including after wraparound.
            for(auto& bucket:entries)for(auto& e:bucket)e.generation=0;
            generation=1;
        }
    }
    void reset() {invalidate();counters={};}
    bool contains(const Key& key) {
        for(const auto& e:entries[bucketFor(key)])
            if(e.generation==generation&&e.key==key){++counters.hits;return true;}
        ++counters.misses;return false;
    }
    void remember(const Key& key) {
        auto index=bucketFor(key);auto& bucket=entries[index];
        for(auto& e:bucket)if(e.generation==generation&&e.key==key)return;
        for(auto& e:bucket)if(e.generation!=generation){e={key,generation};++counters.stores;return;}
        auto& cursor=next[index];
        bucket[cursor]={key,generation};
        cursor=static_cast<std::uint8_t>((std::size_t(cursor)+1)%Ways);
        ++counters.stores;++counters.replacements;
    }
private:
    struct Entry {Key key{};std::uint64_t generation=0;};
    std::array<std::array<Entry,Ways>,Buckets> entries{};
    std::array<std::uint8_t,Buckets> next{};
    std::uint64_t generation=1;
    Stats counters{};
    static std::size_t bucketFor(const Key& key) {
        std::uint64_t h=1469598103934665603ULL;
        for(auto v:key){h^=static_cast<std::uint64_t>(v);h*=1099511628211ULL;}
        // Coordinates often have the same low fixed-point bits. Avalanche
        // before masking so map-grid positions do not cluster in one bucket.
        h^=h>>33;h*=0xff51afd7ed558ccdULL;
        h^=h>>33;h*=0xc4ceb9fe1a85ec53ULL;h^=h>>33;
        return static_cast<std::size_t>(h)&(Buckets-1);
    }
};
}
