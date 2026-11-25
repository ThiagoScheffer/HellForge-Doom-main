/* Optional elemental feedback and injury reactions. Included by p_weakpoints.cpp
   to share actor lifecycle/save records; periodic damage uses the normal damage boundary. */
int p_wp_elemental=0,p_wp_reactions=0,p_wp_dot=0,p_wp_dot_damage=0;
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
    return m->player||m->type==MT_POSSESSED||m->type==MT_SHOTGUY||m->type==MT_CHAINGUY||m->type==MT_TROOP||m->type==MT_SERGEANT||m->type==MT_HEAD;
}
bool dotEligible(mobj_t *m){return allowed(m)&&m->info&&((m->flags&MF_COUNTKILL)||m->type==MT_SKULL)&&!(m->flags&(MF_MISSILE|MF_ARENA|MF_ARENAOLD));}
void clearDot(Runtime &r){r.dotUntil=r.dotNext=r.dotKind=r.dotDamage=0;r.dotSource=nullptr;r.dotPlayer=-1;}
bool compatible(mobj_t *m){return P_CombatCompatible&&P_CombatCompatible(m);}
}
void P_CombatConfigure(int elemental,int reactions) {
    p_wp_elemental=elemental!=0;p_wp_reactions=reactions!=0;
    for(auto &entry:runtime){auto &r=entry.second;
        if(!elemental)r.burnUntil=r.burnKind=r.charred=0;
        if(!reactions&&r.staggerUntil>leveltime&&entry.first->state&&entry.first->state->tics>0)entry.first->tics=std::min<int>(entry.first->tics,entry.first->state->tics);
        if(!reactions)r.staggerUntil=r.staggerCooldown=r.baronUntil=r.siphonUntil=r.siphonNext=r.siphonCooldown=0;
    }
    if(!elemental&&!p_wp_dot)P_CombatFireEnd();
}
void P_CombatFireBegin(mobj_t *source,mobj_t *fire) {
    if((!p_wp_elemental&&!p_wp_dot)||!P_WeakpointActive())return;
    fireSource=source;fireInflictor=fire;fireVictims.clear();
}
void P_CombatFireEnd(void){fireSource=fireInflictor=nullptr;fireVictims.clear();}
int P_CombatDamageKind(mobj_t *inflictor,mobj_t *source) {
    if(p_wp_dot_damage)return p_wp_dot_damage;
    if((!p_wp_elemental&&!p_wp_dot)||!P_WeakpointActive()||p_wp_bleed_damage)return 0;
    if(fireSource&&source==fireSource&&(inflictor==fireSource||inflictor==fireInflictor))return 4;
    if(inflictor&&inflictor->type==MT_FATSHOT&&(inflictor->flags&MF_MISSILE))return 1;
    if(inflictor&&inflictor->type==MT_TROOPSHOT&&(inflictor->flags&MF_MISSILE)&&p_wp_dot)return 3;
    auto it=runtime.find(inflictor);
    return source&&source->player&&inflictor&&inflictor->type==MT_PLASMA&&it!=runtime.end()&&it->second.kind==WP_PLASMA_DIRECT?2:0;
}
int P_CombatVisualKind(mobj_t *actor) {
    if(int dot=P_CombatDotKind(actor))return dot;
    if(!p_wp_elemental||!elementalEligible(actor))return 0;
    auto it=runtime.find(actor);return it!=runtime.end()&&(it->second.burnUntil>leveltime||(actor->player&&actor->health<=0))?it->second.burnKind:0;
}
int P_CombatCharred(mobj_t *actor) {
    if(P_HazardLavaCorpse(actor))return 1;
    if(!p_wp_elemental||!elementalEligible(actor)||actor->player||actor->health>0)return 0;
    auto it=runtime.find(actor);return it!=runtime.end()&&it->second.charred;
}
int P_CombatRage(mobj_t *actor) {
    if(!p_wp_reactions||!p_wp_bleeding||!allowed(actor)||actor->type!=MT_BRUISER||P_WeakpointFrenzied(actor))return 0;
    auto it=runtime.find(actor);return it!=runtime.end()&&it->second.baronUntil>leveltime;
}
void P_CombatConfirmed(mobj_t *actor,mobj_t *source,int kind,int actual,int previousBurn) {
    if(actual<=0||!P_WeakpointActive()||!actor)return;
    // previousBurn == -1 is the native survivor notification. Reactions run
    // after the existing player bullet pipeline has escalated bleeding.
    if(!p_wp_dot_damage&&p_wp_reactions&&p_wp_bleeding&&allowed(actor)){
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
    int element=kind==2?2:kind?1:0;
    if(actor->health<=0){
        if(p_wp_elemental&&elementalEligible(actor)&&(element||previousBurn==1||previousBurn==2)){
            auto &r=runtime[actor];r.charred=!actor->player;r.burnKind=element?element:previousBurn;r.burnUntil=leveltime+70;
            if(actor->player&&element!=1){r.burnKind=0;r.burnUntil=0;}
        }
        return;
    }
    if(p_wp_dot_damage||!kind)return;
    // A single Vile attack may report direct and splash damage to the same actor.
    if(kind==4&&fireSource){
        if(std::find(fireVictims.begin(),fireVictims.end(),actor)!=fireVictims.end())return;
        if(fireVictims.size()>=256)return;fireVictims.push_back(actor);
    }
    if(p_wp_dot&&dotEligible(actor)){
        int chance=0,duration=0,damage=0;
        bool human=actor->type==MT_POSSESSED||actor->type==MT_SHOTGUY||actor->type==MT_CHAINGUY;
        if(kind==3&&human){chance=15;duration=210;damage=3;}
        else if(kind==1&&elementalEligible(actor)){chance=10;duration=140;damage=5;}
        else if(kind==2){chance=12;duration=175;damage=4;}
        if(chance){
            if(roll(chance)){
                auto &r=runtime[actor];bool same=r.dotKind==element&&r.dotUntil>=leveltime;
                if(!same)r.dotNext=leveltime+35;
                r.dotUntil=leveltime+duration;r.dotKind=element;r.dotDamage=damage;r.dotSource=source;r.dotPlayer=-1;
                if(source&&source->player)for(int i=0;i<MAXPLAYERS;++i)if(source->player==&players[i])r.dotPlayer=i;
            }
            // A failed damaging ignition must not start a misleading cosmetic fire.
            if(kind!=2||!p_wp_elemental)return;
        }
    }
    if(!p_wp_elemental||!elementalEligible(actor)||kind==3)return;
    kind=element;
    // Render-only ignition uses its own deterministic hash, never P_Random.
    uint32_t h=(++cosmeticSerial)*747796405u+(uint32_t)leveltime*2891336453u;h=(h^(h>>16))*2246822519u;h^=h>>13;
    if(kind==2||h%100<10){auto &r=runtime[actor];r.burnKind=kind;r.burnUntil=leveltime+(kind==1?105:12);}
}
static void combatExpansionTick(mobj_t *actor) {
    if(!p_wp_elemental&&!p_wp_reactions&&!p_wp_dot)return;
    auto it=runtime.find(actor);
    if(!P_WeakpointActive()){if(it!=runtime.end()){clearDot(it->second);it->second.burnUntil=it->second.burnKind=it->second.charred=0;}return;}
    if(it!=runtime.end()&&it->second.dotKind){
        if(!p_wp_dot||!dotEligible(actor)||leveltime>it->second.dotUntil)clearDot(it->second);
        else if(leveltime>=it->second.dotNext&&it->second.dotNext<=it->second.dotUntil){
            int kind=it->second.dotKind,damage=it->second.dotDamage;auto *source=it->second.dotSource;
            if(!source&&it->second.dotPlayer>=0)source=players[it->second.dotPlayer].mo;
            it->second.dotNext+=35;
            p_wp_dot_damage=kind;P_DamageMobj(actor,nullptr,source,damage);p_wp_dot_damage=0;
            if(actor->health<=0)return;
        }
        it=runtime.find(actor);
    }
    if(it!=runtime.end()&&P_CombatVisualKind(actor)&&P_CombatVisual){int visual=P_CombatVisualKind(actor);P_CombatVisual(actor,p_wp_elemental?visual:visual+3);}
    if(!p_wp_reactions||!p_wp_bleeding||!allowed(actor)||actor->type!=MT_VILE||P_WeakpointFeeding(actor))return;
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


void P_CombatConfigureDot(int enabled){
    p_wp_dot=enabled!=0;
    if(!p_wp_dot)for(auto &entry:runtime)clearDot(entry.second);
}
int P_CombatDotKind(mobj_t *actor){
    if(!p_wp_dot||!dotEligible(actor))return 0;
    auto it=runtime.find(actor);return it!=runtime.end()&&it->second.dotUntil>=leveltime?it->second.dotKind:0;
}
