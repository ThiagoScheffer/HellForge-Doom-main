#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace hellforge::corpse_environment {

enum class Medium : std::uint8_t { None, Acid, Lava };

struct Reaction {
    float progress=0.f;
    Medium medium=Medium::None;
    bool touching=false;

    bool complete() const {return progress>=1.f;}
};

struct Events {bool entered=false,completed=false;};

inline float speed(int selection) {
    return selection==0?.5f:selection==2?2.f:1.f;
}

// Contact and per-medium preferences are resolved by the host. A loss of
// support pauses consumption, including across live configuration changes.
inline Events advance(Reaction &state,Medium contact,float seconds,float rate,bool fragment=false) {
    if(state.complete())return {};
    const bool entered=contact!=Medium::None&&(!state.touching||contact!=state.medium);
    state.touching=contact!=Medium::None;
    if(!state.touching)return {};
    state.medium=contact;
    if(std::isfinite(seconds)&&seconds>0.f&&std::isfinite(rate)&&rate>0.f) {
        const float duration=contact==Medium::Acid?(fragment?1.5f:6.f):(fragment?2.f:5.f);
        state.progress=std::min(1.f,state.progress+seconds*rate/duration);
        if(state.progress>1.f-1e-5f)state.progress=1.f;
    }
    const bool complete=state.complete();
    if(complete)state.touching=false;
    return {entered,complete};
}

} // namespace hellforge::corpse_environment
