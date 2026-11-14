/* Optional survival AI. Sidecar records and the existing save trailer own all
   state; native hooks below preserve this fork's collision/animation helpers. */
namespace {
int feedBudgetTic=-1,feedBudget=0;
bool predator(mobj_t *actor){return actor&&(actor->type==MT_HEAD||actor->type==MT_SERGEANT||(actor->type==MT_PAIN&&P_WeakpointBlind(actor)));}
double foodDistance(mobj_t *a,mobj_t *b){double x=(double)a->x-b->x,y=(double)a->y-b->y;return std::hypot(x,y)/FRACUNIT;}
bool foodThreat(mobj_t *actor) {
    if(P_WeakpointBlind(actor))return false;
    if(!p_wp_balance.feedThreat)return false;
    for(int i=0;i<MAXPLAYERS;++i){auto *mo=players[i].mo;
        if(mo&&mo->health>0&&foodDistance(actor,mo)<=p_wp_balance.feedThreat&&P_CheckSight(actor,mo))return true;}
    return false;
}
bool foodValid(mobj_t *actor,mobj_t *food) {
    if(P_WeakpointBlind(actor)&&food&&food->health>0)return false;
    if(!food||food==actor||!P_WeakpointFoodEligible(food,food->health<=0))return false;
    return foodDistance(actor,food)<=p_wp_balance.feedRadius&&P_CheckSight(actor,food);
}
bool foodTouch(mobj_t *actor,mobj_t *food) {
    if(!foodValid(actor,food))return false;
    double reach=std::max(24.,((double)actor->radius+food->radius)/FRACUNIT+12.);
    if(foodDistance(actor,food)>reach||!P_WeakpointFoodPath||!P_WeakpointFoodPath(actor,food))return false;
    if(food->health<=0)return std::abs((int64_t)actor->z-food->floorz)<=24LL*FRACUNIT;
    return actor->z<food->z+food->height&&actor->z+actor->height>food->z;
}
void feedHeal(mobj_t *actor,int amount) {
    int ceiling=(int)((int64_t)actor->info->spawnhealth*p_wp_balance.feedStop/100);
    actor->health=std::min(actor->info->spawnhealth,std::max(actor->health,(int)std::min<int64_t>(ceiling,(int64_t)actor->health+amount)));
}
mobj_t *findFood(mobj_t *actor) {
    MP_AUTO(MP_FEED);
    MP_COLLECT;
    struct Candidate {mobj_t *actor;double distance;bool corpse;};
    std::vector<Candidate> choices;int visited=0;
    for(auto *t=thinkercap.next;t!=&thinkercap&&visited<256;t=t->next,++visited){
        if(t->function.acp1!=(actionf_p1)P_MobjThinker)continue;
        MP_LOCAL_COUNT(MP_FEED_CANDIDATE,1);auto *other=(mobj_t*)t;if(foodValid(actor,other))choices.push_back({other,foodDistance(actor,other),other->health<=0});
    }
    std::stable_sort(choices.begin(),choices.end(),[](const auto &a,const auto &b){return a.corpse!=b.corpse?a.corpse>b.corpse:a.distance<b.distance;});
    int tested=0;for(const auto &choice:choices){if(++tested>8)break;if(P_WeakpointFoodPath&&P_WeakpointFoodPath(actor,choice.actor))return choice.actor;}
    return nullptr;
}
}
void P_WeakpointFeedingReset(void){feedBudgetTic=-1;feedBudget=0;}
int P_WeakpointFoodEligible(mobj_t *food,int corpse) {
    if(!food||food->player||!food->info||!(food->info->flags&MF_COUNTKILL)||
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
    r.feedPhase=0;r.feedTarget=r.feedPrevious=nullptr;r.feedUntil=0;
}
void P_WeakpointConfigureFeeding(int enabled) {
    if(p_wp_feeding==enabled)return;p_wp_feeding=enabled;
    if(!enabled)for(auto &entry:runtime){if(P_WeakpointBlind(entry.first))continue;P_WeakpointStopFeeding(entry.first);entry.second.feedCooldown=entry.second.feedSearch=0;}
}
void P_WeakpointFeedingHurt(mobj_t *actor){if(p_wp_feeding||P_WeakpointBlind(actor))P_WeakpointStopFeeding(actor);}
void P_WeakpointFeedingTick(mobj_t *actor) {
    if(!predator(actor)||!allowed(actor)||!actor->info)return;
    auto found=runtime.find(actor);
    bool busy=found!=runtime.end()&&found->second.feedPhase;
    bool blind=P_WeakpointBlind(actor)!=0;
    if(busy) {
        if((!p_wp_feeding&&!blind)||P_WeakpointFrenzied(actor)||foodThreat(actor)||(!blind&&actor->health*100LL>=actor->info->spawnhealth*(int64_t)p_wp_balance.feedStop)||
            leveltime>=found->second.feedUntil){P_WeakpointStopFeeding(actor);return;}
        if(found->second.feedTarget&&!foodValid(actor,found->second.feedTarget)){P_WeakpointStopFeeding(actor);return;}
        if(found->second.feedTarget)actor->target=found->second.feedTarget;
        return;
    }
    if((!p_wp_feeding&&!blind)||P_WeakpointFrenzied(actor)||(!blind&&actor->health*100LL>actor->info->spawnhealth*(int64_t)p_wp_balance.feedStart)||
        foodThreat(actor)||!P_WeakpointFoodPath||!P_WeakpointFoodAnimate)return;
    auto &r=runtime[actor];if(leveltime<r.feedCooldown||leveltime<r.feedSearch)return;
    if(feedBudgetTic!=leveltime){feedBudgetTic=leveltime;feedBudget=0;}
    if(feedBudget>=4)return;++feedBudget;r.feedSearch=leveltime+p_wp_balance.feedSearch;
    auto *food=findFood(actor);if(!food)return;
    r.feedPrevious=actor->target;r.feedTarget=food;r.feedPhase=1;r.feedUntil=leveltime+p_wp_balance.feedPursuit;
    actor->target=food;actor->threshold=100;
    if(P_WeakpointFoodWake)P_WeakpointFoodWake(actor);
}
int P_WeakpointFeedingChase(mobj_t *actor) {
    if(!p_wp_feeding&&!P_WeakpointBlind(actor))return 0;
    P_WeakpointFeedingTick(actor);if(!P_WeakpointFeeding(actor))return 0;
    auto &r=runtime[actor];auto *food=r.feedTarget;
    if(r.feedPhase==3) {
        if(food&&food->health<=0&&foodTouch(actor,food)&&P_WeakpointFoodEligible(food,1)&&P_WeakpointFoodConsume) {
            runtime[food].noEat=true;int amount=(int)((int64_t)actor->info->spawnhealth*p_wp_balance.feedCorpseHeal/100);
            P_WeakpointFoodConsume(food);feedHeal(actor,std::max(1,amount));
            if(P_WeakpointBlind(actor)){
                // The corpse was actually consumed; searching/touching alone cannot cure.
                r.blind=false;r.blindFire=0;P_WeakpointStopFeeding(actor);actor->target=nullptr;actor->threshold=0;return 1;
            }
        }
        r.feedPhase=1;r.feedTarget=nullptr;
        if(actor->health*100LL>=actor->info->spawnhealth*(int64_t)p_wp_balance.feedStop){P_WeakpointStopFeeding(actor);return 1;}
    }
    if(!r.feedTarget){
        auto *next=findFood(actor);if(!next){P_WeakpointStopFeeding(actor);return 1;}
        r.feedTarget=next;r.feedUntil=leveltime+p_wp_balance.feedPursuit;food=next;
    }
    food=r.feedTarget;actor->target=food;
    if(foodTouch(actor,food)){
        r.feedPhase=2;r.feedUntil=leveltime+p_wp_balance.feedPursuit;
        P_WeakpointFoodAnimate(actor);return 1;
    }
    if(r.feedPhase==2){P_WeakpointStopFeeding(actor);return 1;}
    if(P_WeakpointFoodMove)P_WeakpointFoodMove(actor);
    return 1;
}
int P_WeakpointFeedingAttack(mobj_t *actor) {
    if(!P_WeakpointFeeding(actor))return 0;
    auto found=runtime.find(actor);if(found==runtime.end())return 0;
    if(found->second.feedPhase!=2)return 1;
    auto *food=found->second.feedTarget;
    if(!food||!foodTouch(actor,food)){P_WeakpointStopFeeding(actor);return 1;}
    found->second.feedPhase=3;
    if(food->health>0) {
        int before=food->health;P_DamageMobj(food,actor,actor,p_wp_balance.feedBite);
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
