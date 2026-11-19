/* Session history and bounded presentation events. Not part of saved actor state. */
namespace {
combat_stats_t sessionStats{};
combat_notice_t combatNotices[3]{};
int noticeCount=0,noticeEnabled=0;
bool freshVisit=false,attemptAlive=true;
void expireNotices(){for(int i=0;i<noticeCount;)if(combatNotices[i].expires<=sessionStats.runtimeTics){for(int j=i+1;j<noticeCount;++j)combatNotices[j-1]=combatNotices[j];--noticeCount;}else ++i;}
const char *combatName(int type){
    switch(type){case MT_BRUISER:return "BARON OF HELL";case MT_KNIGHT:return "HELL KNIGHT";case MT_FATSO:return "MANCUBUS";
    case MT_VILE:return "ARCH VILE";case MT_HEAD:return "CACODEMON";case MT_PAIN:return "PAIN ELEMENTAL";case MT_SPIDER:return "SPIDER MASTERMIND";
    case MT_TROOP:return "IMP";case MT_SERGEANT:return "PINKY";default:return "MONSTER";}
}
}
void P_HudConfigure(int enabled){noticeEnabled=enabled!=0;if(!enabled)noticeCount=0;}
void P_HudNotify(combat_notice_kind_t kind,const char *text,unsigned color,int priority,int duration,uint64_t key){
    if(!noticeEnabled||!P_WeakpointActive()||!text||duration<=0)return;
    char normalized[96];std::snprintf(normalized,sizeof(normalized),"%s",text);
    for(char *p=normalized;*p;++p)if(*p>='a'&&*p<='z')*p-=32;text=normalized;
    expireNotices();
    uint64_t until=sessionStats.runtimeTics+(uint64_t)std::clamp(duration,1,350);
    for(int i=0;i<noticeCount;++i){auto &n=combatNotices[i];
        if(n.kind==kind&&(key&&n.key==key||
           (kind!=HUD_SEVERE_CRITICAL&&!std::strncmp(n.text,text,95)&&n.expires+9>=until))){
            n.expires=until;n.priority=std::max(n.priority,priority);return;
        }
    }
    int slot=noticeCount;
    if(slot==3){slot=0;for(int i=1;i<3;++i)if(combatNotices[i].priority<combatNotices[slot].priority||(combatNotices[i].priority==combatNotices[slot].priority&&combatNotices[i].expires<combatNotices[slot].expires))slot=i;
        if(priority<combatNotices[slot].priority)return;
    }else ++noticeCount;
    auto &n=combatNotices[slot];n={};n.kind=kind;n.priority=priority;n.color=color;n.key=key;n.expires=until;
    std::snprintf(n.text,sizeof(n.text),"%s",text);
}
int P_HudNotices(combat_notice_t out[3]){expireNotices();if(!noticeEnabled||!P_WeakpointActive())return 0;for(int i=0;i<noticeCount;++i)out[i]=combatNotices[i];return noticeCount;}
void P_CombatMessage(combat_notice_kind_t kind,mobj_t *actor,uint64_t event){
    if(!noticeEnabled||!actor)return;
    const char *title=kind==HUD_CRITICAL_BLEED?"CRITICAL BLEEDING":kind==HUD_FRENZY?"FRENZY TRIGGERED":kind==HUD_EYE_RAGE?"EYE RAGE":"EYE INJURY";
    char text[96];std::snprintf(text,sizeof(text),"%s - %s",title,combatName(actor->type));
    uint64_t key=(uint64_t)(uintptr_t)actor^(event*1099511628211ull);
    P_HudNotify(kind,text,kind==HUD_CRITICAL_BLEED?0xFFFFD75Au:0xFFB78AFFu,kind==HUD_CRITICAL_BLEED?1:2,88,key);
}
void P_SessionMap(int episode,int map){
    if(!P_WeakpointActive())return;
    if(freshVisit||sessionStats.episode!=episode||sessionStats.map!=map){sessionStats.attempts=1;sessionStats.deaths=0;}
    else ++sessionStats.attempts;
    sessionStats.episode=episode;sessionStats.map=map;sessionStats.streak=sessionStats.attemptTics=0;noticeCount=0;freshVisit=false;attemptAlive=true;
}
void P_SessionNewVisit(void){freshVisit=true;}
void P_SessionTick(int alive){if(!P_WeakpointActive())return;++sessionStats.runtimeTics;if(!alive)attemptAlive=false;if(alive&&attemptAlive)++sessionStats.attemptTics;}
void P_SessionDamage(mobj_t *target,mobj_t *source,int before,int damage){
    if(!P_WeakpointActive()||!target||before<=0||damage<=0)return;
    uint64_t actual=(uint64_t)std::min(before,damage);
    if(target->player){sessionStats.taken+=actual;if(target->health<=0){++sessionStats.deaths;sessionStats.streak=0;attemptAlive=false;}}
    else if(source&&source->player&&((target->flags&MF_COUNTKILL)||target->type==MT_SKULL)){
        sessionStats.dealt+=actual;if(target->health<=0&&attemptAlive)++sessionStats.streak;
    }
}
const combat_stats_t *P_SessionStats(void){return &sessionStats;}
void P_SessionReset(void){sessionStats={};noticeCount=0;freshVisit=false;attemptAlive=true;}
