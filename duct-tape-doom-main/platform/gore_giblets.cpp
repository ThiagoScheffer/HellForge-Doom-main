#include "gore_giblets.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace hellforge::giblets {
namespace {
constexpr float kGravity = 950.0f; // Doom world units / second squared; tune in-game
constexpr float kStep = 1.0f / 70.0f;
constexpr float kPi = 3.14159265358979323846f;

Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
Vec3 operator*(Vec3 a, float s) { return {a.x*s, a.y*s, a.z*s}; }
float dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
float length(Vec3 a) { return std::sqrt(dot(a,a)); }
Vec3 unit(Vec3 a, Vec3 fallback) {
    const float d = length(a);
    return std::isfinite(d) && d > 0.0001f ? a*(1.0f/d) : fallback;
}
bool finite(Vec3 a) {
    return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z);
}
std::uint32_t random32(std::uint32_t &s) {
    // Independent from P_Random / M_Random. No shared mutable global state.
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return s;
}
float random01(std::uint32_t &s) {
    return float(random32(s) & 0x00ffffffu) / float(0x01000000u);
}
float between(std::uint32_t &s, float lo, float hi) {
    return lo + (hi-lo)*random01(s);
}
struct Material { float bounce, tangentialDrag, minRestSpeed, lifetime, sizeLow, sizeHigh; };
Material material(Kind k) {
    switch (k) {
        case Kind::Bone: return {0.43f,0.79f,24.0f,4.5f,1.9f,4.3f};
        case Kind::Viscera: return {0.07f,0.42f,38.0f,3.6f,3.8f,8.5f};
        case Kind::HeavyTissue: return {0.17f,0.57f,33.0f,3.3f,2.8f,6.5f};
        case Kind::Flesh: return {0.22f,0.63f,28.0f,2.8f,1.4f,3.4f};
        case Kind::BrainLike: return {0.16f,0.50f,34.0f,3.5f,2.8f,6.5f};
    }
    return {0.2f,0.6f,28.0f,3.0f,1.4f,3.0f};
}
struct Adhesion {float hold,compression,sag;};
Adhesion adhesion(Kind kind) {
    switch(kind) {
    case Kind::Viscera:return {2.6f,.60f,2.f};
    case Kind::BrainLike:return {1.8f,.48f,1.5f};
    case Kind::HeavyTissue:return {1.5f,.42f,1.2f};
    case Kind::Flesh:return {.8f,.32f,.8f};
    case Kind::Bone:return {};
    }
    return {};
}
float markScale(Kind kind) {
    switch (kind) {
        case Kind::Viscera: return 3.6f;
        case Kind::HeavyTissue: return 2.6f;
        case Kind::Bone: return 0.8f;
        case Kind::Flesh: return 1.8f;
        case Kind::BrainLike: return 2.8f;
    }
    return 1.0f;
}
} // namespace

bool System::seenEvent(std::uint64_t id) const {
    if (id == 0) return false;
    for (std::uint64_t recent : recentEvents_)
        if (recent == id) return true;
    return false;
}

int System::emit(const Burst &request) {
    // Safety gates are deliberately repeated here; the caller still owns
    // weapon, monster, severity, palette, F4 and gameplay eligibility.
    if (!request.eligible || request.mapKillCount >= 200 || request.mapKillCount < 0 ||
        !finite(request.origin) || !finite(request.direction) ||
        !std::isfinite(request.intensity) || !std::isfinite(request.presentation) || request.pieces == 0 ||
        request.kind>Kind::BrainLike || request.tint>Tint::PaleBone ||
        seenEvent(request.eventId)) return 0;

    const Vec3 forward = unit(request.direction, {1,0,0});
    const Vec3 side = unit({-forward.y, forward.x, 0}, {0,1,0});
    const Vec3 lift = unit({-forward.x*forward.z, -forward.y*forward.z,
                            forward.x*forward.x+forward.y*forward.y}, {0,0,1});
    const float intensity = std::clamp(request.intensity, 0.25f, 2.0f);
    std::uint32_t rng = request.cosmeticSeed ? request.cosmeticSeed : 0x9e3779b9u;
    int added = 0;
    size_t active=activeCount();
    const size_t limit=std::min<size_t>(MaxPieces,request.maxActive);
    const float response=std::clamp(request.presentation,0.f,1.f);
    const int count = std::min<int>(MaxPiecesPerBurst, request.pieces);
    for (int i = 0; i < count; ++i) {
        Piece *free=nullptr;
        if(active<limit) {
            for(auto &candidate:pieces_)if(!candidate.used){free=&candidate;break;}
            ++active;
        } else if(limit) {
            free=oldestStationary();
            if(free)++stats_.evictedResting;
        }
        if (!free) { ++stats_.droppedByBudget; continue; }
        *free=Piece{};
        free->sticky=request.sticky;
        free->softDeformation=request.softDeformation;
        const auto kind=request.kind==Kind::BrainLike&&i%3==2?Kind::Flesh:request.kind;
        const Material m=material(kind);
        const float speed = between(rng, 95.0f, 215.0f)*intensity;
        const float scatter=request.radial?1.65f:.50f;
        const Vec3 spread = side * between(rng,-scatter,scatter) +
                            lift * between(rng,-0.18f,0.65f);
        const Vec3 direction = unit(forward+spread, forward);
        free->p = request.origin + direction * between(rng,0.0f,1.2f);
        free->eventId = request.eventId;
        free->v = direction*speed + Vec3{0,0,between(rng,5.0f,70.0f)};
        free->size = between(rng,m.sizeLow,m.sizeHigh)*std::sqrt(intensity)*(1.f+.20f*response);
        free->radius = std::clamp(free->size*0.3f,0.5f,3.0f);
        free->rotation = between(rng,-kPi,kPi);
        free->angularSpeed = between(rng,-11.0f,11.0f)*(1.f-.40f*response);
        free->age = 0;
        free->lifespan = request.brief?1.2f:m.lifetime + between(rng,-0.3f,0.5f);
        free->brief=request.brief;
        free->kind = kind;
        if(added==0&&compatibleArtwork(request.featuredArtwork,kind))
            free->artwork=request.featuredArtwork;
        free->tint = request.tint;
        free->variant = std::uint8_t(random32(rng)%8);
        free->contacts = 0;
        free->resting = false;
        free->used = true;
        ++added; ++stats_.spawned;
    }
    if (request.eventId) {
        recentEvents_[eventCursor_] = request.eventId;
        eventCursor_ = (eventCursor_+1)%recentEvents_.size();
    }
    stats_.active = static_cast<std::uint32_t>(activeCount());
    return added;
}

void System::step(float dt, const Host &host, int &impactBudget) {
    for (Piece &p : pieces_) {
        if (!p.used) continue;
        p.age += dt;
        if (p.age >= p.lifespan) { p.used = false; continue; }
        if (p.attachment==Attachment::Wall) {
            const float phase=std::clamp((p.age-p.attachedAt)/(p.attachedUntil-p.attachedAt),0.f,1.f);
            p.sag=p.softDeformation&&host.softDeformation?adhesion(p.kind).sag*phase*phase:0.f;
            Vec3 contact{},normal{};
            if(!host.resolveWall||!host.resolveWall(host.context,p.anchor,p.sag,contact,normal)) {p.used=false;continue;}
            const float normalLength=length(normal);
            if(!finite(contact)||!std::isfinite(normalLength)||normalLength<.01f) {p.used=false;continue;}
            if(!host.softDeformation)p.squash=0.f;
            p.contact=contact;p.normal=normal*(1.f/normalLength);
            p.p=contact+p.normal*(p.radius+.1f);
            if(p.age>=p.attachedUntil) {
                p.attachment=Attachment::None;p.anchor={};p.contact={};p.normal={};p.surface={};
                p.squash=0;p.sag=0;p.v={0,0,-8.f};
            }
            continue;
        }
        p.squash=std::max(0.f,p.squash-dt*2.5f);
        if (p.resting) continue;
        // Semi-implicit Euler: integrate gravity before tracing the displacement.
        p.v.z -= kGravity*dt;
        const Vec3 next = p.p + p.v*dt;
        if (!finite(next)) { p.used = false; continue; }
        TraceHit hit{};
        ++stats_.traces;
        if (!host.trace(host.context,p.p,next,p.radius,hit)) {
            p.p = next; p.rotation += p.angularSpeed*dt; continue;
        }
        const auto surface=hit.surface.type;
        const bool solid=surface==Surface::Wall?
            hit.surface.line>=0&&hit.surface.side>=0&&hit.surface.side<=1&&hit.surface.section>=0&&hit.surface.section<=2:
            (surface==Surface::Floor||surface==Surface::Ceiling)&&hit.surface.sector>=0;
        if (!solid || !finite(hit.center) || !finite(hit.contact) || !finite(hit.normal) || length(hit.normal)<0.01f ||
            !std::isfinite(hit.fraction) || hit.fraction < 0 || hit.fraction > 1) { p.used = false; continue; }

        const Vec3 normal = unit(hit.normal, {0,0,1});
        const float incoming = length(p.v);
        const float inward = dot(p.v,normal);
        p.p = hit.center + normal*0.1f;
        const Material m = material(p.kind);
        const auto profile=adhesion(p.kind);
        const bool markable=hit.canMark&&inward<0&&incoming>=48.f&&p.contacts<2;
        WallAnchor anchor{};
        if(p.sticky&&!p.attachedOnce&&surface==Surface::Wall&&markable&&profile.hold>0.f&&
           attachedCount()<MaxAttached&&host.attach&&host.resolveWall&&host.attach(host.context,hit,anchor)&&
           anchor.surface.type==Surface::Wall&&anchor.surface.line==hit.surface.line&&
           anchor.surface.side==hit.surface.side&&anchor.surface.section==hit.surface.section&&
           std::isfinite(anchor.along)&&anchor.along>=0&&std::isfinite(anchor.offsetZ)&&anchor.texture>=0) {
            if(stationaryCount()>=MaxResting) {oldestStationary()->used=false;++stats_.evictedResting;}
            const float variation=.92f+.16f*(p.variant/7.f);
            const float hold=profile.hold*variation;
            p.attachment=Attachment::Wall;p.anchor=anchor;p.attachedOnce=true;
            p.attachedAt=p.age;p.attachedUntil=p.age+(p.brief?std::min(hold,.8f):hold);
            p.lifespan=std::max(p.lifespan,p.attachedUntil+.8f);
            p.contact=hit.contact;p.normal=normal;p.surface=hit.surface;
            p.p=p.contact+normal*(p.radius+.1f);
            p.v={};p.angularSpeed=0;p.squash=p.softDeformation&&host.softDeformation?profile.compression:0.f;
        } else if(p.softDeformation&&host.softDeformation&&inward<0) {
            p.squash=profile.compression*std::clamp(-inward/180.f,0.f,1.f);
        }
        // Only apply impact callbacks to geometrically valid, explicitly markable
        // surfaces. Host applies F4 wall-blood and shared decal budgets again.
        if (markable && p.kind!=Kind::Bone) {
            if (impactBudget > 0 && host.impact) {
                Impact e;
                e.eventId = p.eventId;
                e.center = hit.center;
                e.contact = hit.contact;
                e.normal = normal;
                e.kind = p.kind;
                e.tint = p.tint;
                e.surface = hit.surface;
                e.strength = std::clamp(incoming/300.0f,0.0f,1.0f);
                e.markSize = std::clamp(p.size*markScale(p.kind),2.0f,32.0f);
                e.variant = p.variant;
                e.attached=p.attachment==Attachment::Wall;
                e.cling=e.attached||(surface==Surface::Wall&&p.kind==Kind::Viscera);
                host.impact(host.context,e);
                --impactBudget; ++stats_.emittedImpacts;
            } else ++stats_.suppressedImpacts;
        }
        ++p.contacts;
        if(p.attachment==Attachment::Wall) {
            if(host.contact)host.contact(host.context,p.eventId,surface,false);
            continue;
        }
        // Wall contacts can leave a smear, then the piece falls with the
        // material's normal bounce and drag.
        if (p.contacts >= 4 && surface != Surface::Floor) {
            if(host.contact)host.contact(host.context,p.eventId,surface,false);
            p.used = false;
            continue;
        }
        if (inward < 0) {
            const Vec3 nvel = normal * (-inward*m.bounce);
            const Vec3 tvel = (p.v-normal*inward)*m.tangentialDrag;
            p.v = nvel + tvel;
        } else p.v = p.v*m.tangentialDrag;
        if (surface == Surface::Floor && (p.contacts >= 4 || length(p.v) <= m.minRestSpeed)) {
            if(stationaryCount()>=MaxResting) {
                oldestStationary()->used=false;
                ++stats_.evictedResting;
            }
            p.resting = true; p.v = {};
            p.contact = hit.contact; p.normal = normal; p.surface = hit.surface;
            p.restedAt=p.age;
            p.lifespan=p.age+(p.brief?2.0f:p.kind==Kind::Flesh?15.f:p.kind==Kind::Bone?21.f:26.f);
        } else p.rotation += p.angularSpeed*dt;
        if(host.contact)host.contact(host.context,p.eventId,surface,p.resting);
    }
}

void System::advance(float seconds, const Host &host) {
    if (!std::isfinite(seconds) || seconds <= 0) return;
    if (!host.trace) return; // fail closed: no known collision geometry
    accumulator_ += std::min(seconds,0.12f);
    int impactBudget = MaxImpactEventsPerAdvance;
    int iterations = 0;
    while (accumulator_ >= kStep && iterations < 8) {
        step(kStep,host,impactBudget);
        accumulator_ -= kStep;
        ++iterations;
    }
    // Never perform a catch-up burst after a stalled/paused frame.
    if (iterations == 8) accumulator_ = 0;
    stats_.active = static_cast<std::uint32_t>(activeCount());
}

void System::forEachVisual(VisualFn draw, void *context) const {
    if (!draw) return;
    for (const Piece &p : pieces_) if (p.used) {
        draw(context, {p.p,p.kind,p.tint,p.size,p.rotation,
                       std::clamp(p.age/p.lifespan,0.0f,1.0f),p.variant,p.resting,
                       p.contact,p.normal,p.surface,p.eventId,p.attachment,
                       p.attachment==Attachment::Wall?p.age-p.attachedAt:0.f,p.squash,p.sag,p.anchor,p.artwork});
    }
}

void System::clear() {
    pieces_ = {};
    recentEvents_ = {};
    eventCursor_ = 0;
    accumulator_ = 0;
    stats_ = {};
}

std::size_t System::activeCount() const {
    std::size_t total = 0;
    for (const Piece &p : pieces_) if (p.used) ++total;
    return total;
}

bool System::hasActiveEvent(std::uint64_t id) const {
    for (const Piece &p : pieces_) if (p.used && p.eventId == id) return true;
    return false;
}

std::size_t System::restingCount() const {
    return std::count_if(pieces_.begin(),pieces_.end(),[](const Piece &p){return p.used&&p.resting;});
}

std::size_t System::attachedCount() const {
    return std::count_if(pieces_.begin(),pieces_.end(),[](const Piece &p){return p.used&&p.attachment==Attachment::Wall;});
}

std::size_t System::stationaryCount() const {
    return std::count_if(pieces_.begin(),pieces_.end(),[](const Piece &p){return p.used&&(p.resting||p.attachment==Attachment::Wall);});
}

System::Piece *System::oldestStationary() {
    Piece *oldest=nullptr;
    const auto stationaryAge=[](const Piece &p){return p.age-(p.resting?p.restedAt:p.attachedAt);};
    for(auto &p:pieces_)
        if(p.used&&(p.resting||p.attachment==Attachment::Wall)&&(!oldest||stationaryAge(p)>stationaryAge(*oldest)))oldest=&p;
    return oldest;
}

} // namespace hellforge::giblets
