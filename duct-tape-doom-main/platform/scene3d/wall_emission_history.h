/* Bounded history for animated emissive wall spans.
   The ordinary current entry remains the fast path. History is consulted only
   on an animated material transition with unchanged texture-space bounds. */
#ifndef DOOM_WALL_EMISSION_HISTORY_H
#define DOOM_WALL_EMISSION_HISTORY_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <utility>

namespace doom_wall_emission {
template<class Entry, class PayloadBytes>
class History {
    using Key = std::pair<std::size_t,int>;
    std::map<Key,Entry> entries;
    std::size_t bytes=0;
    const std::size_t entryLimit,byteLimit;

    void remember(std::size_t span,Entry &current) {
        const Key key{span,current.texture};
        const std::size_t charge=PayloadBytes{}(current);
        auto old=entries.find(key);
        const std::size_t oldCharge=old==entries.end()?0:PayloadBytes{}(old->second);
        if((old==entries.end()&&entries.size()>=entryLimit)||
           charge>byteLimit-(bytes-oldCharge))return;
        // Do not move the current entry if the bounded history is full.
        if(old==entries.end())entries.emplace(key,std::move(current));
        else old->second=std::move(current);
        bytes=bytes-oldCharge+charge;
    }

public:
    History(std::size_t maxEntries,std::size_t maxPayloadBytes):
        entryLimit(maxEntries),byteLimit(maxPayloadBytes) {}

    // Caller has checked the current entry and whether the target is animated.
    // On false, caller computes and replaces current exactly as before.
    template<class Bounds>
    bool transition(std::size_t span,int texture,std::uint64_t mask,
                    const Bounds &bounds,Entry &current) {
        if(current.texture<0||current.texture==texture||current.bounds!=bounds)
            return false;
        auto found=entries.find({span,texture});
        if(found!=entries.end()&&found->second.mask==mask&&
           found->second.bounds==bounds) {
            bytes-=PayloadBytes{}(found->second);
            Entry next=std::move(found->second);
            entries.erase(found);
            remember(span,current);
            current=std::move(next);
            return true;
        }
        remember(span,current);
        return false;
    }

    void clear() {entries.clear();bytes=0;}
    std::size_t size() const {return entries.size();}
    std::size_t payloadBytes() const {return bytes;}
};
} // namespace doom_wall_emission
#endif
