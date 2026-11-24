#ifndef DOOM_SCENE3D_GORE_POLICY_H
#define DOOM_SCENE3D_GORE_POLICY_H
#include <array>
#include <cstdint>
extern "C" {
#include "info.h"
}
namespace doom3d {
enum class GoreRegion : uint8_t { Unknown, Head, Chest, Belly, Count };
enum class GoreOutcome : uint8_t {
    None, HumanFlesh, HeadBurst, SkullBurst, ChestCavity, BellyCavity,
    ImpTissue, RevenantBone, MancubusHeavy, PinkyViscera, CacoViscera
};
enum class GoreReason : uint8_t { Emitted, Health, Damage, Opaque, Range, Feature, Already };
struct GoreResult { GoreOutcome outcome; GoreReason reason; };

inline constexpr float goreRange=128.f;
inline constexpr float goreImpactRange=320.f;
inline constexpr int livingCavityHealthPercent=50, pinkyHealthPercent=45, cacoHealthPercent=50;
inline constexpr int mancubusFatalDamage=35, revenantDamage=30,
                     pinkyDamage=20, pinkyFatalDamage=30, cacoDamage=20,
                     cacoSingleContactDamage=30, cacoFatalDamage=25;
inline const char *goreStyleName(int type) {
    switch(type) {
    case MT_POSSESSED:case MT_SHOTGUY:return "human_flesh";
    case MT_TROOP:return "imp_tissue";
    case MT_UNDEAD:return "revenant_bone";
    case MT_FATSO:return "mancubus_heavy";
    case MT_SERGEANT:return "pinky_viscera";
    case MT_HEAD:return "caco_blue_viscera";
    default:return nullptr;
    }
}
inline bool goreSupported(int type) {return goreStyleName(type)!=nullptr;}
inline bool goreHuman(int type) {return type==MT_POSSESSED||type==MT_SHOTGUY;}
inline bool goreLivingCavity(int type) {return type==MT_TROOP||type==MT_FATSO;}

struct GoreDecision {
    int type=-1, contacts=0, spriteContacts=0, legacyHeads=0;
    int preHP=0, postHP=0, maxHP=0;
    int64_t effectiveDamage=0;
    std::array<int,static_cast<unsigned>(GoreRegion::Count)> hits{};
    float distance=0;
    bool extreme=false, headEnabled=false, bodyEnabled=false, legacyStock=false, already=false;
    bool fatal() const {return postHP<=0;}
    int count(GoreRegion region) const {return hits[static_cast<unsigned>(region)];}
    bool lowHealth(int percent) const {
        return postHP>0&&maxHP>0&&int64_t(postHP)*100<=int64_t(maxHP)*percent;
    }
};

inline GoreResult goreCandidate(const GoreDecision &d) {
    using O=GoreOutcome;
    using R=GoreReason;
    if(!goreSupported(d.type))return {O::None,R::Feature};
    if(d.distance>goreRange)return {O::None,R::Range};
    if(d.effectiveDamage<=0)return {O::None,R::Damage};
    const int head=d.count(GoreRegion::Head), chest=d.count(GoreRegion::Chest), belly=d.count(GoreRegion::Belly);
    const int torso=chest+belly;
    if(!d.fatal()&&goreLivingCavity(d.type)) {
        if(!d.bodyEnabled)return {O::None,R::Feature};
        if(!d.lowHealth(livingCavityHealthPercent))return {O::None,R::Health};
        if(d.already)return {O::None,R::Already};
        if(!d.spriteContacts||!torso)return {O::None,R::Opaque};
        return {chest>=belly?O::ChestCavity:O::BellyCavity,R::Emitted};
    }
    if(goreHuman(d.type)) {
        if(!d.fatal())return {O::None,R::Health};
        if(d.legacyStock) {
            // M1 measures the head majority over all admitted pellet contacts.
            if(d.headEnabled&&!d.extreme&&d.legacyHeads>0&&d.legacyHeads*2>=d.contacts)
                return {O::HeadBurst,R::Emitted};
            return d.spriteContacts?GoreResult{O::HumanFlesh,R::Emitted}:GoreResult{O::None,R::Opaque};
        }
        if(!d.bodyEnabled)return {O::None,R::Feature};
        return d.spriteContacts?GoreResult{O::HumanFlesh,R::Emitted}:GoreResult{O::None,R::Opaque};
    }
    if(!d.bodyEnabled&&!(d.fatal()&&d.headEnabled&&(d.type==MT_TROOP||d.type==MT_UNDEAD)))
        return {O::None,R::Feature};
    if(!d.spriteContacts)return {O::None,R::Opaque};
    const bool headMajority=d.fatal()&&!d.extreme&&d.headEnabled&&head>0&&head*2>=d.spriteContacts;
    if(d.type==MT_UNDEAD) {
        if(d.effectiveDamage<revenantDamage)return {O::None,R::Damage};
        if(headMajority)return {O::SkullBurst,R::Emitted};
        if(!d.bodyEnabled)return {O::None,R::Feature};
        return head||torso?GoreResult{O::RevenantBone,R::Emitted}:GoreResult{O::None,R::Opaque};
    }
    if(d.type==MT_TROOP&&headMajority)return {O::HeadBurst,R::Emitted};
    if(!d.bodyEnabled)return {O::None,R::Feature};
    if(!d.fatal()&&d.already)return {O::None,R::Already};
    switch(d.type) {
    case MT_TROOP:
        return {O::ImpTissue,R::Emitted};
    case MT_FATSO:
        if(d.effectiveDamage<mancubusFatalDamage)return {O::None,R::Damage};
        return {O::MancubusHeavy,R::Emitted};
    case MT_SERGEANT:
        if(!d.fatal()&&!d.lowHealth(pinkyHealthPercent))return {O::None,R::Health};
        if(d.effectiveDamage<(d.fatal()?pinkyFatalDamage:pinkyDamage))return {O::None,R::Damage};
        return torso?GoreResult{O::PinkyViscera,R::Emitted}:GoreResult{O::None,R::Opaque};
    case MT_HEAD:
        if(!d.fatal()&&!d.lowHealth(cacoHealthPercent))return {O::None,R::Health};
        if(d.effectiveDamage<(d.fatal()?cacoFatalDamage:cacoDamage))return {O::None,R::Damage};
        if(!d.fatal()&&d.spriteContacts<2&&d.effectiveDamage<cacoSingleContactDamage)
            return {O::None,R::Opaque};
        return {O::CacoViscera,R::Emitted};
    default:return {O::None,R::Feature};
    }
}
}
#endif

