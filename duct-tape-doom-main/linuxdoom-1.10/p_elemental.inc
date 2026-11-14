/* Optional elemental feedback and injury reactions. Included by p_weakpoints.cpp
   to share actor lifecycle/save records; no new thinkers or damage-over-time. */
int p_wp_elemental=0,p_wp_reactions=0;
void (*P_CombatVisual)(mobj_t*,int)=nullptr;
int (*P_CombatCompatible)(mobj_t*)=nullptr;
void (*P_CombatStagger)(mobj_t*)=nullptr;
mobj_t *(*P_CombatDonor)(mobj_t*,int)=nullptr;
namespace {
mobj_t *fireSource=nullptr,*fireInflictor=nullptr;
std::vector<mobj_t*> fireVictims;
uint32_t cosmeticSerial=0;
int siphonBudgetTic=-1,siphonBudget=0;
bool elementalEligible(mobj_t *m) {
    if(!m||!P_WeakpointActive()||(P_WeakpointTargetAllowed&&!P_WeakpointTargetAllowed(m)))return false;
    return m->player||m->type==MT_POSSESSED||m->type==MT_SHOTGUY||m->type==MT_CHAINGUY||m->type==MT_TROOP||m->type==MT_SERGEANT;
}
bool compatible(mobj_t *m){return P_CombatCompatible&&P_CombatCompatible(m);}
}
void P_CombatConfigure(int elemental,int reactions) {
    p_wp_elemental=elemental!=0;p_wp_reactions=reactions!=0;
    for(auto &entry:runtime){auto &r=entry.second;
        if(!elemental)r.burnUntil=r.burnKind=r.charred=0;
        if(!reactions&&r.staggerUntil>leveltime&&entry.first->state&&entry.first->state->tics>0)entry.first->tics=std::min<int>(entry.first->tics,entry.first->state->tics);
        if(!reactions)r.staggerUntil=r.staggerCooldown=r.baronUntil=r.siphonUntil=r.siphonNext=r.siphonCooldown=0;
    }
    if(!elemental)P_CombatFireEnd();
}
void P_CombatFireBegin(mobj_t *source,mobj_t *fire) {
    if(!p_wp_elemental||!P_WeakpointActive())return;
    fireSource=source;fireInflictor=fire;fireVictims.clear();
}
void P_CombatFireEnd(void){fireSource=fireInflictor=nullptr;fireVictims.clear();}
int P_CombatDamageKind(mobj_t *inflictor,mobj_t *source) {
    if(!p_wp_elemental||!P_WeakpointActive()||p_wp_bleed_damage)return 0;
    if(fireSource&&source==fireSource&&(inflictor==fireSource||inflictor==fireInflictor))return 1;
    if(inflictor&&inflictor->type==MT_FATSHOT&&(inflictor->flags&MF_MISSILE))return 1;
    auto it=runtime.find(inflictor);
    return source&&source->player&&inflictor&&inflictor->type==MT_PLASMA&&it!=runtime.end()&&it->second.kind==WP_PLASMA_DIRECT?2:0;
}
int P_CombatVisualKind(mobj_t *actor) {
    if(!p_wp_elemental||!elementalEligible(actor))return 0;
    auto it=runtime.find(actor);return it!=runtime.end()&&(it->second.burnUntil>leveltime||(actor->player&&actor->health<=0))?it->second.burnKind:0;
}
int P_CombatCharred(mobj_t *actor) {
    if(!p_wp_elemental||!elementalEligible(actor)||actor->player||actor->health>0)return 0;
    auto it=runtime.find(actor);return it!=runtime.end()&&it->second.charred;
}
int P_CombatRage(mobj_t *actor) {
    if(!p_wp_reactions||!p_wp_bleeding||!allowed(actor)||actor->type!=MT_BRUISER||P_WeakpointFrenzied(actor))return 0;
    auto it=runtime.find(actor);return it!=runtime.end()&&it->second.baronUntil>leveltime;
}
void P_CombatConfirmed(mobj_t *actor,mobj_t*,int kind,int actual,int previousBurn) {
    if(actual<=0||!P_WeakpointActive()||!actor)return;
    // previousBurn == -1 is the native survivor notification. Reactions run
    // after the existing player bullet pipeline has escalated bleeding.
    if(p_wp_reactions&&p_wp_bleeding&&allowed(actor)){
        int tier=P_WeakpointBleedTier(actor);
        if(actor->type==MT_BRUISER&&(tier>=2||(int64_t)actor->health*100<=actor->info->spawnhealth*40LL)){
            auto &r=runtime[actor];if(r.baronUntil<=leveltime)r.baronUntil=leveltime+280;
        }
        if(previousBurn==0&&kind==0&&actor->type==MT_FATSO&&tier>=2&&actual>=5&&compatible(actor)){
            auto &r=runtime[actor];if(leveltime>=r.staggerCooldown&&roll(15)){
                r.staggerUntil=leveltime+14;r.staggerCooldown=leveltime+210;
                if(P_CombatStagger)P_CombatStagger(actor);
            }
        }
    }
    if(!p_wp_elemental||!elementalEligible(actor)||previousBurn==0&&kind==0&&actor->health>0)return;
    if(actor->health<=0){
        if(kind||previousBurn==1){
            auto &r=runtime[actor];r.charred=!actor->player;r.burnKind=kind?kind:previousBurn;r.burnUntil=leveltime+70;
            // Player deaths are fire-specific, never caused merely by an old cosmetic ignition.
            if(actor->player&&kind!=1){r.burnKind=0;r.burnUntil=0;}
        }
        return;
    }
    if(!kind)return;
    if(kind==1&&fireSource){
        if(std::find(fireVictims.begin(),fireVictims.end(),actor)!=fireVictims.end())return;
        if(fireVictims.size()>=256)return;fireVictims.push_back(actor);
    }
    // Render-only ignition uses its own deterministic hash, never P_Random.
    uint32_t h=(++cosmeticSerial)*747796405u+(uint32_t)leveltime*2891336453u;h=(h^(h>>16))*2246822519u;h^=h>>13;
    if(kind==2||h%100<10){auto &r=runtime[actor];r.burnKind=kind;r.burnUntil=leveltime+(kind==1?105:12);}
}
static void combatExpansionTick(mobj_t *actor) {
    if(!p_wp_elemental&&!p_wp_reactions)return;
    auto it=runtime.find(actor);
    if(it!=runtime.end()&&p_wp_elemental&&P_CombatVisualKind(actor)&&P_CombatVisual)P_CombatVisual(actor,it->second.burnKind);
    if(!p_wp_reactions||!p_wp_bleeding||!allowed(actor)||actor->type!=MT_VILE)return;
    if(it!=runtime.end()&&it->second.siphonUntil){
        auto &r=it->second;
        if(leveltime>r.siphonUntil||!P_WeakpointBleeding(actor)||!compatible(actor)||actor->health>=actor->info->spawnhealth){
            r.siphonUntil=r.siphonNext=0;r.siphonCooldown=leveltime+700;return;
        }
    }
    if(!P_WeakpointBleeding(actor)||actor->health>=actor->info->spawnhealth||!compatible(actor)||!P_CombatDonor)return;
    if(it!=runtime.end()&&(leveltime<it->second.siphonCooldown||leveltime<it->second.siphonNext)){
        if(it->second.siphonUntil&&P_CombatVisual)P_CombatVisual(actor,3);return;
    }
    if(siphonBudgetTic!=leveltime){siphonBudgetTic=leveltime;siphonBudget=0;}if(siphonBudget>=4)return;++siphonBudget;
    auto *donor=P_CombatDonor(actor,0);bool corpse=false;
    if(!donor){donor=P_CombatDonor(actor,1);corpse=true;}if(!donor)return;
    auto &r=runtime[actor];if(!r.siphonUntil){r.siphonUntil=leveltime+210;r.siphonNext=leveltime+35;return;}
    int need=std::min(12,actor->info->spawnhealth-actor->health),gained=0;
    if(corpse){
        if(!P_WeakpointFoodEligible(donor,1)||!P_WeakpointFoodConsume)return;
        gained=need;P_WeakpointFoodConsume(donor);
    }else{
        int before=donor->health;
        P_DamageMobj(donor,actor,actor,std::min(need,before));
        gained=std::clamp(before-donor->health,0,std::min(before,need));
    }
    if(actor->health<=0)return;
    actor->health=std::min(actor->info->spawnhealth,actor->health+gained);
    // Donor death/removal may mutate the sidecar table: reacquire, don't retain iterators.
    auto &after=runtime[actor];after.siphonNext=leveltime+35;
    if(after.siphonNext>after.siphonUntil){after.siphonUntil=after.siphonNext=0;after.siphonCooldown=leveltime+700;}
    if(P_CombatVisual)P_CombatVisual(actor,3);
}

static void combatExpansionReset(){P_CombatFireEnd();cosmeticSerial=0;siphonBudgetTic=-1;siphonBudget=0;}

