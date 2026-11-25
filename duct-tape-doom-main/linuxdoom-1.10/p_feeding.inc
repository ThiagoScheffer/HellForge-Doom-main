/* Optional survival AI. Sidecar records and the existing save trailer own all
   state; native hooks below preserve this fork's collision/animation helpers. */
namespace {
int feedBudgetTic=-1,feedBudget=0;
bool predator(mobj_t *actor){return actor&&(actor->type==MT_HEAD||actor->type==MT_SERGEANT||actor->type==MT_VILE||(actor->type==MT_PAIN&&P_WeakpointBlind(actor)));}
double foodDistance(mobj_t *a,mobj_t *b){double x=(double)a->x-b->x,y=(double)a->y-b->y;return std::hypot(x,y)/FRACUNIT;}
mobj_t* foodDanger(mobj_t* actor,double radius=-1){
    if(P_WeakpointBlind(actor)||!p_wp_balance.feedThreat)return nullptr;
    mobj_t* nearest=nullptr;double range=radius<0?p_wp_balance.feedThreat:radius;
    auto test=[&](mobj_t* danger){if(!danger||danger==actor||danger->health<=0)return;double d=foodDistance(actor,danger);if(d<=range&&P_CheckSight(actor,danger)){nearest=danger;range=d;}};
    for(int i=0;i<MAXPLAYERS;++i)test(players[i].mo);
    auto it=runtime.find(actor);if(it!=runtime.end())test(it->second.feedDanger);return nearest;
}
bool foodThreat(mobj_t* actor){return foodDanger(actor)!=nullptr;}
bool foodFor(mobj_t* actor,mobj_t* food){
    if(actor->type!=MT_VILE)return P_WeakpointFoodEligible(food,food&&food->health<=0)!=0;
    if(!food||food==actor||food->player||food->health<=0||!(food->flags&MF_SHOOTABLE)||!food->info||!(food->info->flags&MF_COUNTKILL)||(food->flags&MF_NOBLOOD)||(P_WeakpointTargetAllowed&&!P_WeakpointTargetAllowed(food)))return false;
    switch(food->type){case MT_POSSESSED:case MT_SHOTGUY:case MT_CHAINGUY:case MT_WOLFSS:case MT_TROOP:case MT_SERGEANT:case MT_SHADOWS:case MT_HEAD:case MT_UNDEAD:case MT_KNIGHT:case MT_FATSO:case MT_PAIN:return true;default:return false;}
}
bool foodValid(mobj_t *actor,mobj_t *food) {
    if(P_WeakpointBlind(actor)&&food&&food->health>0)return false;
    if(!food||food==actor||!foodFor(actor,food))return false;
    return foodDistance(actor,food)<=p_wp_balance.feedRadius&&P_CheckSight(actor,food);
}
bool foodTouch(mobj_t *actor,mobj_t *food) {
    if(!foodValid(actor,food))return false;
    double reach=actor->type==MT_VILE?p_wp_balance.feedVileReach:std::max(24.,((double)actor->radius+food->radius)/FRACUNIT+12.);
    if(foodDistance(actor,food)>reach||!P_WeakpointFoodPath||!P_WeakpointFoodPath(actor,food))return false;
    if(food->health<=0)return std::abs((int64_t)actor->z-food->floorz)<=24LL*FRACUNIT;
    return actor->z<food->z+food->height&&actor->z+actor->height>food->z;
}
void feedHeal(mobj_t *actor,int amount) {
    int ceiling=(int)((int64_t)actor->info->spawnhealth*p_wp_balance.feedStop/100);
    int before=actor->health;
    actor->health=std::min(actor->info->spawnhealth,std::max(actor->health,(int)std::min<int64_t>(ceiling,(int64_t)actor->health+amount)));
    if(actor->health>before)runtime[actor].feedAttemptUntil=leveltime+p_wp_balance.feedWindow;
    if(actor->type==MT_VILE&&actor->health>before&&P_CombatVisual)P_CombatVisual(actor,3);
}
mobj_t *findFood(mobj_t *actor) {
    MP_AUTO(MP_FEED);
    MP_COLLECT;
    struct Candidate {mobj_t *actor;double distance;bool corpse;};
    std::vector<Candidate> choices;choices.reserve(16);int visited=0;
    auto* danger=foodDanger(actor,std::max(p_wp_balance.feedRadius,p_wp_balance.feedThreat));
    for(auto *t=thinkercap.next;t!=&thinkercap&&visited<256;t=t->next,++visited){
        if(t->function.acp1!=(actionf_p1)P_MobjThinker)continue;
        MP_LOCAL_COUNT(MP_FEED_CANDIDATE,1);auto *other=(mobj_t*)t;if(foodValid(actor,other)&&(!danger||foodDistance(other,danger)>p_wp_balance.feedThreat))choices.push_back({other,foodDistance(actor,other)+(danger?std::max(0.,foodDistance(actor,danger)-foodDistance(other,danger))*2:0),other->health<=0});
    }
    std::stable_sort(choices.begin(),choices.end(),[](const auto &a,const auto &b){return a.corpse!=b.corpse?a.corpse>b.corpse:a.distance<b.distance;});
    int tested=0;for(const auto &choice:choices){if(++tested>8)break;if(P_WeakpointFoodPath&&P_WeakpointFoodPath(actor,choice.actor))return choice.actor;}
    return nullptr;
}
}
void P_WeakpointFeedingReset(void){feedBudgetTic=-1;feedBudget=0;}
int P_WeakpointFoodEligible(mobj_t *food,int corpse) {
    if(!food||(corpse&&P_HazardAcidCorpse(food))||food->player||!food->info||!(food->info->flags&MF_COUNTKILL)||
        (P_WeakpointTargetAllowed&&!P_WeakpointTargetAllowed(food)))return 0;
    auto found=runtime.find(food);if(found!=runtime.end()&&found->second.noEat)return 0;
    if(!corpse)return food->health>0&&(food->flags&MF_SHOOTABLE)&&
        (food->type==MT_TROOP||food->type==MT_POSSESSED||food->type==MT_SHOTGUY||food->type==MT_CHAINGUY);
    if(food->health>0||!(food->flags&MF_CORPSE)||(food->flags&(MF_SHOOTABLE|MF_FLOAT|MF_NOBLOOD))||food->tics!=-1||
        std::abs((int64_t)food->z-food->floorz)>8LL*FRACUNIT)return 0;
    switch(food->type){
    case MT_POSSESSED:case MT_SHOTGUY:case MT_CHAINGUY:case MT_WOLFSS:case MT_TROOP:case MT_SERGEANT:case MT_SHADOWS:
    case MT_KNIGHT:case MT_FATSO:case MT_UNDEAD:case MT_VILE:case MT_HEAD:case MT_PAIN:return 1;
    default:return 0; // Bosses, mechanical actors, Lost Souls and decorative bodies.
    }
}
int P_WeakpointFeeding(mobj_t *actor) {
    auto it=runtime.find(actor);return (p_wp_feeding||P_WeakpointBlind(actor))&&allowed(actor)&&!P_WeakpointFrenzied(actor)&&it!=runtime.end()&&it->second.feedPhase>0;
}
void P_WeakpointStopFeeding(mobj_t *actor) {
    auto it=runtime.find(actor);if(it==runtime.end())return;auto &r=it->second;
    if(r.feedPhase){
        if(!P_WeakpointFrenzied(actor)){actor->target=r.feedPrevious&&r.feedPrevious->health>0&&(r.feedPrevious->flags&MF_SHOOTABLE)?r.feedPrevious:nullptr;actor->threshold=0;}
        r.feedCooldown=leveltime+p_wp_balance.feedCooldown;
    }
    r.feedPhase=0;r.feedTarget=r.feedPrevious=r.feedDanger=nullptr;r.feedUntil=r.feedAttemptUntil=0;
}
void P_WeakpointConfigureFeeding(int enabled) {
    if(p_wp_feeding==enabled)return;p_wp_feeding=enabled;
    if(!enabled)for(auto &entry:runtime){if(P_WeakpointBlind(entry.first))continue;P_WeakpointStopFeeding(entry.first);entry.second.feedCooldown=entry.second.feedSearch=0;}
}
void P_WeakpointFeedingHurtFrom(mobj_t* actor,mobj_t* source){
    if((!p_wp_feeding&&!P_WeakpointBlind(actor))||!predator(actor)||!allowed(actor))return;
    auto found=runtime.find(actor);if(found==runtime.end()||!found->second.feedPhase)return;
    auto& r=found->second;if(source&&source!=actor&&source->health>0)r.feedDanger=source;
    r.feedTarget=nullptr;r.feedPhase=4;r.feedSearch=std::max(r.feedSearch,leveltime+7);actor->target=nullptr;actor->threshold=0;
}
void P_WeakpointFeedingHurt(mobj_t* actor){P_WeakpointFeedingHurtFrom(actor,nullptr);}
static void feedFailure(mobj_t* actor){
    auto it=runtime.find(actor);if(it==runtime.end())return;auto* danger=it->second.feedDanger;bool blind=P_WeakpointBlind(actor)!=0;
    P_WeakpointStopFeeding(actor);auto& r=runtime[actor];r.feedCooldown=leveltime+p_wp_balance.feedFailureCooldown;
    if(!blind){if(danger&&danger->health>0&&(danger->flags&MF_SHOOTABLE))actor->target=danger;if(actor->target)actor->threshold=100;actor->flags|=MF_JUSTHIT;if(P_WeakpointFoodWake)P_WeakpointFoodWake(actor);}
}
void P_WeakpointFeedingTick(mobj_t *actor) {
    if(!predator(actor)||!allowed(actor)||!actor->info)return;
    auto found=runtime.find(actor);bool busy=found!=runtime.end()&&found->second.feedPhase;bool blind=P_WeakpointBlind(actor)!=0;
    if(busy){
        auto& r=found->second;
        if((!p_wp_feeding&&!blind)||P_WeakpointFrenzied(actor)||(!blind&&actor->health*100LL>=actor->info->spawnhealth*(int64_t)p_wp_balance.feedStop)){P_WeakpointStopFeeding(actor);return;}
        if(leveltime>=r.feedAttemptUntil){feedFailure(actor);return;}
        if(foodThreat(actor)){r.feedPhase=4;r.feedTarget=nullptr;actor->target=nullptr;return;}
        if(r.feedPhase!=4&&(leveltime>=r.feedUntil||!foodValid(actor,r.feedTarget))){r.feedPhase=4;r.feedTarget=nullptr;actor->target=nullptr;}
    }else{
        int start=actor->type==MT_VILE?p_wp_balance.feedVileStart:p_wp_balance.feedStart;
        if((!p_wp_feeding&&!blind)||P_WeakpointFrenzied(actor)||(!blind&&actor->health*100LL>actor->info->spawnhealth*(int64_t)start)||!P_WeakpointFoodPath||!P_WeakpointFoodAnimate)return;
        if(actor->type==MT_VILE&&P_CombatCompatible&&!P_CombatCompatible(actor))return;
        auto& r=runtime[actor];if(leveltime<r.feedCooldown||(blind&&leveltime<r.feedSearch))return;
        r.feedPrevious=actor->target;r.feedDanger=actor->target;r.feedPhase=4;r.feedAttemptUntil=leveltime+p_wp_balance.feedWindow;actor->target=nullptr;actor->threshold=0;
        if(P_WeakpointFoodWake)P_WeakpointFoodWake(actor);
    }
    auto& r=runtime[actor];if(r.feedPhase!=4){if(r.feedTarget)actor->target=r.feedTarget;return;}
    if(leveltime<r.feedSearch)return;
    if(feedBudgetTic!=leveltime){feedBudgetTic=leveltime;feedBudget=0;}if(feedBudget>=4)return;++feedBudget;r.feedSearch=leveltime+p_wp_balance.feedSearch;
    if(foodThreat(actor))return;
    auto* food=findFood(actor);if(!food){if(blind){r.feedPhase=0;r.feedAttemptUntil=0;}return;}
    r.feedTarget=food;r.feedPhase=1;r.feedUntil=leveltime+p_wp_balance.feedPursuit;actor->target=food;actor->threshold=100;
}
int P_WeakpointFeedingChase(mobj_t *actor) {
    if(!p_wp_feeding&&!P_WeakpointBlind(actor))return 0;
    P_WeakpointFeedingTick(actor);if(!P_WeakpointFeeding(actor))return 0;
    auto &r=runtime[actor];auto *food=r.feedTarget;
    if(r.feedPhase==4){if(P_WeakpointFoodRetreat)P_WeakpointFoodRetreat(actor,foodDanger(actor));return 1;}
    if(r.feedPhase==3) {
        if(food&&food->health<=0&&foodTouch(actor,food)&&P_WeakpointFoodEligible(food,1)&&P_WeakpointFoodConsume) {
            runtime[food].noEat=true;int amount=(int)((int64_t)actor->info->spawnhealth*p_wp_balance.feedCorpseHeal/100);
            P_WeakpointFoodConsume(food);feedHeal(actor,std::max(1,amount));
            if(P_WeakpointBlind(actor)){
                // The corpse was actually consumed; searching/touching alone cannot cure.
                r.blind=false;r.blindFire=0;P_WeakpointStopFeeding(actor);actor->target=nullptr;actor->threshold=0;return 1;
            }
        }
        r.feedPhase=1; // Reuse surviving prey; new searches use the shared budget.
        if(actor->health*100LL>=actor->info->spawnhealth*(int64_t)p_wp_balance.feedStop){P_WeakpointStopFeeding(actor);return 1;}
    }
    if(!r.feedTarget){
        r.feedPhase=4;actor->target=nullptr;return 1;
    }
    food=r.feedTarget;actor->target=food;
    if(foodTouch(actor,food)){
        r.feedPhase=2;r.feedUntil=leveltime+p_wp_balance.feedPursuit;
        P_WeakpointFoodAnimate(actor);return 1;
    }
    if(r.feedPhase==2){r.feedPhase=4;r.feedTarget=nullptr;actor->target=nullptr;return 1;}
    if(P_WeakpointFoodMove)P_WeakpointFoodMove(actor);
    return 1;
}
int P_WeakpointFeedingAttack(mobj_t *actor) {
    if(!P_WeakpointFeeding(actor))return 0;
    auto found=runtime.find(actor);if(found==runtime.end())return 0;
    if(found->second.feedPhase!=2)return 1;
    auto *food=found->second.feedTarget;
    if(!food||!foodTouch(actor,food)||foodThreat(actor)){found->second.feedPhase=4;found->second.feedTarget=nullptr;actor->target=nullptr;return 1;}
    found->second.feedPhase=3;
    if(food->health>0) {
        int before=food->health;P_DamageMobj(food,actor,actor,actor->type==MT_VILE?p_wp_balance.feedVileDamage:p_wp_balance.feedBite);
        int damaged=std::clamp(before-food->health,0,before);
        if(actor->health<=0)return 1;
        int recovery=std::min(damaged,(int)((int64_t)actor->info->spawnhealth*p_wp_balance.feedLiveHeal/100));
        feedHeal(actor,recovery);
        if(food->health<=0&&recovery>0)runtime[food].noEat=true;
        found=runtime.find(actor);if(found!=runtime.end()) {
            // A living bite never also consumes its resulting corpse.
            found->second.feedTarget=food->health>0?food:nullptr;
        }
    }
    return 1;
}
