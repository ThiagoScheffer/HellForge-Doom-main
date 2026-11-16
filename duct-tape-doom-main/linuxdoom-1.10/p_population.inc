// Simulation-owned original population. Included by p_encounter.cpp only.
struct Original {
 mapthing_t map{};int id=0,point=0,type=0,resolved=0,sector=0;
 int look=0,ticRoll=0,state=0,retry=0;bool queued=false;
 double extent=0;mobj_t* actor=nullptr;
};
struct PopulationNode {double x0,y0,x1,y1;int left=-1,right=-1,parent=-1,remaining=0;std::vector<int> entries;};
static std::vector<Original> originals;
static std::vector<PopulationNode> populationTree;
static std::vector<int> originalLeaf;
static std::unordered_map<mobj_t*,int> originalActors; // -1: eager original
static std::unordered_set<mobj_t*> liveOriginalActors;
static std::vector<unsigned char> eagerSectors;
static std::array<double,NUMMOBJTYPES> originalExtents;
static bool streaming=false,populationRequested=false,originalsResolved=false,draining=false,deathCredit=false,progressCredit=false,progressPack=false;
static int mapThingId=0,originalRandom=-1,packNext=0,streamPolicy=0,populationPolicy=0,drainCursor=0,packCredits=0;
static int populationLogTic=-1;
static unsigned eagerOriginals=0,urgentOriginals=0,blockedOriginals=0,routineOriginals=0,legacyProxyEager=0;
static double activationMs=0,setupMs=0,ticMs=0;static unsigned populationTics=0;
static std::chrono::steady_clock::time_point populationStart;
static unsigned filteredOriginals=0,requestedInitial=0,initialLive=0,initialDeferred=0,initialOverrun=0;
static std::array<unsigned,10> reasonCounts{};
static const char* populationReasons[]={"hard-boss/miniboss","hard-boss-map","hard-metadata/unknown","hard-unique","hard-patched-definitions","soft-tagged/special-sector","soft-action-boundary","soft-floating/emissive","soft-artwork/initial-obstruction","compatibility-no-occlusion-proof"};
static std::array<int,MAXPLAYERS> queryX,queryY,queryTic,querySector;
static std::vector<std::vector<int>> sectorOriginals,sectorLinks;
static std::vector<unsigned char> regionQueued;
static std::vector<int> pendingRegions;static size_t regionCursor=0,regionEntry=0;
struct QueuedOriginal {double distance;int index;bool operator<(const QueuedOriginal& b)const{return distance==b.distance?index>b.index:distance>b.distance;}};
static std::priority_queue<QueuedOriginal> originalQueue;
struct PopulationQuery {double x=0,y=0,r=0;std::vector<int> stack;int leaf=-1;size_t entry=0;};
static PopulationQuery proximityQuery;
static bool originalMonster(int type){return type>=0&&type<NUMMOBJTYPES&&((mobjinfo[type].flags&MF_COUNTKILL)||type==MT_SKULL);}
static void reviveOriginal(mobj_t* m){if(originalActors.find(m)!=originalActors.end()&&living(m))liveOriginalActors.insert(m);}
static void diedOriginal(mobj_t* m){if(liveOriginalActors.erase(m))deathCredit=true;}
static void captureEagerOriginal(mobj_t* m){if(!populationRequested||!originalMonster(m->type))return;originalActors.emplace(m,-1);reviveOriginal(m);}
static double originalExtent(int type){
 auto& result=originalExtents[type];if(result>=0)return result;
 int state=mobjinfo[type].spawnstate;if(state<=S_NULL||state>=NUMSTATES)return result=0;
 int sprite=states[state].sprite,frame=states[state].frame&FF_FRAMEMASK;
 if(sprite<0||sprite>=numsprites||frame>=sprites[sprite].numframes)return result=0;
 const auto& sf=sprites[sprite].spriteframes[frame];double extent=0;
 for(int r=0;r<8;++r){int lump=sf.lump[r];if(lump<0||lump>=numspritelumps)return result=0;
  int id=firstspritelump+lump;if(W_LumpLength(id)<8)return result=0;
  auto* p=(const unsigned char*)W_CacheLumpNum(id,PU_CACHE);
  int height=p[2]|p[3]<<8;if(height<=0)return result=0;
  extent=std::max({extent,std::abs(double(spriteoffset[lump])/FRACUNIT),std::abs(double(spritewidth[lump]-spriteoffset[lump])/FRACUNIT),std::abs(double(spritetopoffset[lump])/FRACUNIT),double(height)});
 }
 return result=extent+doom_sprite_frustum::shadowReach()+8;
}
static void clearOriginals(){
 originals.clear();populationTree.clear();originalLeaf.clear();originalActors.clear();liveOriginalActors.clear();eagerSectors.clear();originalQueue={};
 streaming=populationRequested=originalsResolved=draining=deathCredit=progressCredit=progressPack=false;originalRandom=-1;packNext=packCredits=drainCursor=0;
 eagerOriginals=urgentOriginals=blockedOriginals=routineOriginals=filteredOriginals=requestedInitial=initialLive=initialDeferred=initialOverrun=legacyProxyEager=0;
 activationMs=setupMs=ticMs=0;populationTics=0;reasonCounts.fill(0);queryTic.fill(-1);querySector.fill(-1);populationLogTic=-1;
 sectorOriginals.clear();sectorLinks.clear();regionQueued.clear();pendingRegions.clear();regionCursor=regionEntry=0;proximityQuery.stack.clear();proximityQuery.leaf=-1;proximityQuery.entry=0;
}
static bool bossDeathMap(){return (gamemode==commercial&&gamemap==7)||(gamemode!=commercial&&(gamemap==8||(gameepisode==4&&gamemap==6)));}
static void beginOriginals(){
 originalExtents.fill(-1);populationRequested=enabled()&&config.stream&&!loading;streaming=populationRequested&&!P_SwarmDefinitionsPatched();streamPolicy=config.streamPolicy;populationPolicy=config.populationPolicy;
 if(!populationRequested)return;populationStart=std::chrono::steady_clock::now();eagerSectors.assign(numsectors,0);
 for(int i=0;i<numsectors;++i)if(sectors[i].tag||sectors[i].special)eagerSectors[i]|=1;
 for(int i=0;i<numlines;++i)if(lines[i].special){if(lines[i].frontsector)eagerSectors[lines[i].frontsector-sectors]|=2;if(lines[i].backsector)eagerSectors[lines[i].backsector-sectors]|=2;}
}
static int deferOriginal(const mapthing_t& map,int type){
 if(!populationRequested||loading||!originalMonster(type))return 0;
 ++filteredOriginals;int hard=-1;auto* p=profile(type);
 if(!streaming)hard=4;
 else if(bossDeathMap())hard=1;
 else if(!p||(uint16_t(map.options)&~31u)||captured>=pointLimit)hard=2;
 else if(p->boss||p->mini||progression(type))hard=0;
 if(hard>=0){if(streaming&&p)++legacyProxyEager;++reasonCounts[hard];++eagerOriginals;return 0;}
 int sector=int(R_PointInSubsector(fixed_t(map.x)*FRACUNIT,fixed_t(map.y)*FRACUNIT)->sector-sectors);
 if(eagerSectors[sector]&1)++reasonCounts[5];if(eagerSectors[sector]&2)++reasonCounts[6];if(mobjinfo[type].flags&MF_FLOAT)++reasonCounts[7];
 mobj_t test;probe(test,type,fixed_t(map.x)*FRACUNIT,fixed_t(map.y)*FRACUNIT);
 double extent=originalExtent(type);bool obstructed=extent<=0||!fits(test);if(obstructed)++reasonCounts[8];
 if(eagerSectors[sector]||(mobjinfo[type].flags&MF_FLOAT)||obstructed)++legacyProxyEager;
 Original o;o.map=map;o.id=mapThingId;o.type=o.resolved=type;o.sector=sector;o.point=int(points.size());o.extent=std::max(extent,double(mobjinfo[type].radius)/FRACUNIT+8);
 // Reserve vanilla initialization rolls in filtered THINGS order. Tick-driven
 // materialization never advances gameplay RNG, including initial selection.
 o.look=P_Random()%MAXPLAYERS;o.ticRoll=states[mobjinfo[type].spawnstate].tics>0?P_Random():0;
 originals.push_back(o);++counts[type];++initialCount;++captured;points.push_back({map,type,sector});if(mobjinfo[type].flags&MF_COUNTKILL)++totalkills;return 1;
}
static int buildPopulationTree(std::vector<int>& ids,int begin,int end,int parent){
 int node=int(populationTree.size());populationTree.emplace_back();
 double x0=1e30,y0=1e30,x1=-1e30,y1=-1e30;
 for(int i=begin;i<end;++i){auto& o=originals[ids[i]];x0=std::min(x0,double(o.map.x)-512.);x1=std::max(x1,double(o.map.x)+512.);y0=std::min(y0,double(o.map.y)-512.);y1=std::max(y1,double(o.map.y)+512.);}
 populationTree[node]={x0,y0,x1,y1,-1,-1,parent,0,{}};
 for(int i=begin;i<end;++i)if(!originals[ids[i]].state)++populationTree[node].remaining;
 // Leaves are 512-unit spatial bins. Balanced subdivision also bounds sparse
 // maps without allocating a dense map-sized grid.
 int bx=originals[ids[begin]].map.x>>9,by=originals[ids[begin]].map.y>>9;bool same=true;
 for(int i=begin+1;i<end;++i)if((originals[ids[i]].map.x>>9)!=bx||(originals[ids[i]].map.y>>9)!=by){same=false;break;}
 if(same){for(int i=begin;i<end;++i){populationTree[node].entries.push_back(ids[i]);originalLeaf[ids[i]]=node;}return node;}
 bool x=x1-x0>=y1-y0;int mid=(begin+end)/2;
 std::nth_element(ids.begin()+begin,ids.begin()+mid,ids.begin()+end,[&](int a,int b){int av=x?originals[a].map.x:originals[a].map.y,bv=x?originals[b].map.x:originals[b].map.y;return av==bv?a<b:av<bv;});
 int left=buildPopulationTree(ids,begin,mid,node),right=buildPopulationTree(ids,mid,end,node);populationTree[node].left=left;populationTree[node].right=right;return node;
}
static void finishOriginalIndex(){
 if(originals.empty())return;
 std::vector<QueuedOriginal> queueStorage;queueStorage.reserve(originals.size());originalQueue=decltype(originalQueue)(std::less<QueuedOriginal>(),std::move(queueStorage));
 originalLeaf.assign(originals.size(),-1);proximityQuery.stack.reserve(originals.size()*2);std::vector<int> ids;ids.reserve(originals.size());
 for(size_t i=0;i<originals.size();++i)ids.push_back(int(i));
 populationTree.clear();populationTree.reserve(ids.size()*2);
 if(!ids.empty())buildPopulationTree(ids,0,int(ids.size()),-1);
}
static void resolveOriginals(){
 if(originalsResolved)return;originalsResolved=true;
 for(auto& o:originals)if(enabled()&&config.monsters&&counts[o.type]>1&&!protectedMonster(o.type)&&roll(100)<config.strength){
  int selected=selectMonster(o.type,false);mobj_t test;probe(test,selected,fixed_t(o.map.x)*FRACUNIT,fixed_t(o.map.y)*FRACUNIT);
  if(fits(test))o.resolved=selected;
  o.extent=std::max(o.extent,originalExtent(o.resolved));
 }
 finishOriginalIndex();
}
static bool materializeOriginal(int index,bool urgent,bool initial=false){
 auto& o=originals[index];if(o.state)return true;if(o.retry>leveltime&&!initial)return false;
 mobj_t test;probe(test,o.resolved,fixed_t(o.map.x)*FRACUNIT,fixed_t(o.map.y)*FRACUNIT);
 if(initial){
  // Native map initialization permits authored overlaps. Runtime activation
  // never does. A replacement that no longer fits falls back to its original.
  if(o.resolved!=o.type&&!fits(test)){o.resolved=o.type;probe(test,o.type,test.x,test.y);}
 }else if(!fits(test)){o.retry=leveltime+TICRATE/4;++blockedOriginals;return false;}
 auto start=std::chrono::steady_clock::now();originalRandom=o.look;
 auto* m=P_SpawnMobj(test.x,test.y,(mobjinfo[o.resolved].flags&MF_SPAWNCEILING)?ONCEILINGZ:ONFLOORZ,(mobjtype_t)o.resolved);originalRandom=-1;
 m->spawnpoint=o.map;m->angle=ANG45*(o.map.angle/45);if(o.map.options&MTF_AMBUSH)m->flags|=MF_AMBUSH;if(m->tics>0)m->tics=1+o.ticRoll%m->tics;
 o.actor=m;o.state=1;o.queued=false;originalActors.emplace(m,index);liveOriginalActors.insert(m);
 for(int node=originalLeaf[index];node>=0;node=populationTree[node].parent)--populationTree[node].remaining;
 if(urgent)++urgentOriginals;else if(!initial)++routineOriginals;
 activationMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return true;
}
template<class Accept,class Visit>static void visitOriginals(int node,const Accept& accept,const Visit& visit){
 if(node<0||populationTree.empty())return;auto& n=populationTree[node];if(!n.remaining||!accept(n))return;
 if(n.left>=0){visitOriginals(n.left,accept,visit);visitOriginals(n.right,accept,visit);}else for(int i:n.entries)if(!originals[i].state)visit(i);
}
static double originalDistance(const Original& o){double best=1e30;for(int p=0;p<MAXPLAYERS;++p)if(playerAlive(p))best=std::min(best,distance2(fixed_t(o.map.x)*FRACUNIT,fixed_t(o.map.y)*FRACUNIT,players[p].mo));return best;}
static void queueOriginal(int i){auto& o=originals[i];if(o.state||o.queued)return;o.queued=true;double score=originalDistance(o);if(score<=double(config.activateDistance)*config.activateDistance)score*=.25;originalQueue.push({score,i});}
static void queueRegion(int sector){if(sector<0||sector>=numsectors||regionQueued[sector])return;regionQueued[sector]=1;pendingRegions.push_back(sector);}
static void preparePopulationRegions(){
 sectorOriginals.assign(numsectors,{});sectorLinks.assign(numsectors,{});regionQueued.assign(numsectors,0);pendingRegions.reserve(numsectors);regionCursor=regionEntry=0;
 for(size_t i=0;i<originals.size();++i)sectorOriginals[originals[i].sector].push_back(int(i));
 std::unordered_map<int,std::vector<int>> tagged;for(int s=0;s<numsectors;++s)if(sectors[s].tag)tagged[sectors[s].tag].push_back(s);
 for(int l=0;l<numlines;++l){auto& line=lines[l];int a=line.frontsector?int(line.frontsector-sectors):-1,b=line.backsector?int(line.backsector-sectors):-1;
  if(a>=0&&b>=0&&a!=b){sectorLinks[a].push_back(b);sectorLinks[b].push_back(a);}
  // Link visible/visited teleport destinations to the authored source holding
  // sectors. Remote closets can enter bounded packs without a full-map scan.
  bool teleport=line.special==39||line.special==97||line.special==125||line.special==126||line.special==174||line.special==195||line.special==207||line.special==208||line.special==209||line.special==210;
  if(teleport){auto t=tagged.find(line.tag);if(t!=tagged.end())for(int destination:t->second){if(a>=0)sectorLinks[destination].push_back(a);if(b>=0)sectorLinks[destination].push_back(b);}}
 }
 for(auto& links:sectorLinks){std::sort(links.begin(),links.end());links.erase(std::unique(links.begin(),links.end()),links.end());}
}
static void planInitialOriginals(){
 if(!populationRequested)return;
 requestedInitial=config.initialBudgetMode?unsigned(std::ceil(filteredOriginals*double(config.initialMonsterPercent)/100)):std::min(filteredOriginals,unsigned(config.initialMonsterCount));requestedInitial=std::min(filteredOriginals,requestedInitial);
 // Singleton stock classes may carry unique encounter roles even without a
 // boss-death action. Protect them rather than invent a scripting proof.
 for(size_t i=0;i<originals.size();++i)if(counts[originals[i].type]<=1){++reasonCounts[3];++eagerOriginals;materializeOriginal(int(i),false,true);}
 std::vector<int> ranked;ranked.reserve(originals.size());for(size_t i=0;i<originals.size();++i)if(!originals[i].state)ranked.push_back(int(i));
 std::stable_sort(ranked.begin(),ranked.end(),[](int a,int b){double da=originalDistance(originals[a]),db=originalDistance(originals[b]);return da==db?originals[a].id<originals[b].id:da<db;});
 size_t capacity=requestedInitial>liveOriginalActors.size()?requestedInitial-liveOriginalActors.size():0;
 for(size_t n=0;n<ranked.size();++n)if(populationPolicy==0||n<capacity){if(populationPolicy==0&&n>=capacity)++reasonCounts[9];materializeOriginal(ranked[n],false,true);}
 initialLive=unsigned(liveOriginalActors.size());initialDeferred=unsigned(originals.size()-std::count_if(originals.begin(),originals.end(),[](const Original& o){return o.state!=0;}));initialOverrun=initialLive>requestedInitial?initialLive-requestedInitial:0;
 preparePopulationRegions();packCredits=0;packNext=leveltime+int(config.packInterval*TICRATE);
 for(int p=0;p<MAXPLAYERS;++p)if(playerAlive(p)){queryX[p]=players[p].mo->x>>(FRACBITS+7);queryY[p]=players[p].mo->y>>(FRACBITS+7);querySector[p]=int(players[p].mo->subsector->sector-sectors);queueRegion(querySector[p]);for(int s:sectorLinks[querySector[p]])queueRegion(s);}
 setupMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-populationStart).count();
 std::fprintf(stderr,"ED initial population: policy %s filtered %u requested %u required-eager %u initial-live %u deferred %u target-overrun %u legacy-proxy-eager %u setup-ms %.3f\n",populationPolicy?"Performance (visible absence/pop-in possible)":"Compatibility (eager without occlusion proof)",filteredOriginals,requestedInitial,eagerOriginals,initialLive,initialDeferred,initialOverrun,legacyProxyEager,setupMs);
 for(int r=0;r<10;++r)if(reasonCounts[r])std::fprintf(stderr,"ED classification %s: %u\n",populationReasons[r],reasonCounts[r]);
}
static void updatePopulationQueue(){
 // At most 128 BVH nodes and 256 descriptor candidates per tic, including
 // large single-bin rooms. A saved queue remains valid; spatial queries restart.
 for(int p=0;p<MAXPLAYERS;++p)if(playerAlive(p)){
  auto* m=players[p].mo;int cx=m->x>>(FRACBITS+7),cy=m->y>>(FRACBITS+7),sector=int(m->subsector->sector-sectors);
  bool moved=queryX[p]!=cx||queryY[p]!=cy||querySector[p]!=sector;
  if(moved){progressCredit=true;queryX[p]=cx;queryY[p]=cy;querySector[p]=sector;queueRegion(sector);for(int s:sectorLinks[sector])queueRegion(s);}
  if((moved||queryTic[p]<0||leveltime-queryTic[p]>=TICRATE)&&proximityQuery.stack.empty()&&proximityQuery.leaf<0){queryTic[p]=leveltime;proximityQuery.x=double(m->x)/FRACUNIT;proximityQuery.y=double(m->y)/FRACUNIT;proximityQuery.r=config.prefetchDistance;proximityQuery.stack.push_back(0);}
 }
 int candidates=256,nodes=128;
 while(candidates>0&&regionCursor<pendingRegions.size()){auto& ids=sectorOriginals[pendingRegions[regionCursor]];while(candidates>0&&regionEntry<ids.size()){queueOriginal(ids[regionEntry++]);--candidates;}if(regionEntry==ids.size()){++regionCursor;regionEntry=0;}}
 while(candidates>0&&(proximityQuery.leaf>=0||!proximityQuery.stack.empty())){
  if(proximityQuery.leaf>=0){auto& ids=populationTree[proximityQuery.leaf].entries;while(candidates>0&&proximityQuery.entry<ids.size()){int i=ids[proximityQuery.entry++];auto& o=originals[i];double dx=o.map.x-proximityQuery.x,dy=o.map.y-proximityQuery.y;if(dx*dx+dy*dy<=proximityQuery.r*proximityQuery.r)queueOriginal(i);--candidates;}if(proximityQuery.entry==ids.size()){proximityQuery.leaf=-1;proximityQuery.entry=0;}continue;}
  if(nodes--<=0)break;int id=proximityQuery.stack.back();proximityQuery.stack.pop_back();auto& n=populationTree[id];if(!n.remaining)continue;
  double dx=std::max({n.x0-proximityQuery.x,0.,proximityQuery.x-n.x1}),dy=std::max({n.y0-proximityQuery.y,0.,proximityQuery.y-n.y1});if(dx*dx+dy*dy>proximityQuery.r*proximityQuery.r)continue;
  if(n.left<0){proximityQuery.leaf=id;proximityQuery.entry=0;}else{proximityQuery.stack.push_back(n.right);proximityQuery.stack.push_back(n.left);}
 }
}
static void tickOriginals(){
 if(!streaming||populationTree.empty()||!populationTree[0].remaining)return;
 auto begin=std::chrono::steady_clock::now();++populationTics;
 if(!enabled())draining=true;
 if(draining){int scanned=0;while(drainCursor<int(originals.size())&&scanned++<256)queueOriginal(drainCursor++);}
 else updatePopulationQueue();
 bool progress=progressCredit;progressCredit=false;
 if(!draining){
  if(streamPolicy!=1&&(progress||deathCredit)){packCredits=std::max(packCredits,int(config.packSize));if(progress)progressPack=true;}
  if(streamPolicy!=0&&leveltime>=packNext){packCredits=std::max(packCredits,int(config.packSize));packNext=leveltime+int(config.packInterval*TICRATE);}
 }
 deathCredit=false;
 int budget=std::min(int(config.spawnBudget),draining?int(config.spawnBudget):packCredits);
 std::array<QueuedOriginal,64> retries;int retryCount=0;
 for(int attempt=0;attempt<budget&&!originalQueue.empty();++attempt){
  if(!draining&&!progressPack&&liveOriginalActors.size()>=std::max(unsigned(config.softLimit),initialLive))break;
  auto q=originalQueue.top();originalQueue.pop();auto& o=originals[q.index];o.queued=false;if(o.state)continue;
  // Revalidate relevance after movement/teleport; old queue membership does not
  // turn timed staging into a global release of distant originals.
  bool relevant=draining||regionQueued[o.sector]||originalDistance(o)<=double(config.prefetchDistance)*config.prefetchDistance;
  if(!relevant)continue;
  if(!materializeOriginal(q.index,false)){o.queued=true;retries[retryCount++]=q;}else if(!draining)--packCredits;
 }
 for(int i=0;i<retryCount;++i)originalQueue.push(retries[i]);
 if(packCredits<=0)progressPack=false;
 ticMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
}
static encounter_population_t populationStats(){
 encounter_population_t s{};s.eager=eagerOriginals;s.urgent=urgentOriginals;s.blocked=blockedOriginals;s.activationMs=activationMs;s.filtered=filteredOriginals;s.requested=requestedInitial;s.requiredEager=eagerOriginals;s.initialLive=initialLive;s.initialDeferred=initialDeferred;s.overrun=initialOverrun;s.liveOriginals=unsigned(liveOriginalActors.size());s.liveAdditional=unsigned(std::max(0,active));s.routine=routineOriginals;s.effectiveSoftTarget=std::max(unsigned(config.softLimit),initialLive);s.populationPolicy=populationPolicy;s.setupMs=setupMs;s.ticMs=ticMs;s.sampledTics=populationTics;
 for(int r=0;r<10;++r)s.reasons[r]=reasonCounts[r];
 for(const auto& o:originals){if(!o.state){++s.deferred;if(o.queued)++s.queued;}else{++s.materialized;if(o.state==1&&living(o.actor))++s.active;}}return s;
}
static void reportOriginals(){
 if(!populationRequested||!config.debug||(populationLogTic>=0&&leveltime-populationLogTic<TICRATE))return;populationLogTic=leveltime;auto p=populationStats();
 std::fprintf(stderr,"ED Population tic %d: requested %u initial-live %u overrun %u deferred %u queued %u live-originals %u live-additional %u living-streamed %u required-eager %u stasis 0 routine %u blocked %u effective-soft %u draining %d activation-ms %.3f tick-ms %.3f\n",leveltime,p.requested,p.initialLive,p.overrun,p.deferred,p.queued,p.liveOriginals,p.liveAdditional,p.active,p.requiredEager,p.routine,p.blocked,p.effectiveSoftTarget,int(draining),activationMs,ticMs);
}
static bool reservedOriginal(const mobj_t& m){
 if(!streaming||populationTree.empty())return false;bool reserved=false;double x=double(m.x)/FRACUNIT,y=double(m.y)/FRACUNIT,r=double(m.radius)/FRACUNIT;
 visitOriginals(0,[&](const PopulationNode& n){return !reserved&&x+r>=n.x0&&x-r<=n.x1&&y+r>=n.y0&&y-r<=n.y1;},[&](int i){auto& o=originals[i];double reach=r+double(mobjinfo[o.resolved].radius)/FRACUNIT;if(std::abs(x-o.map.x)<reach&&std::abs(y-o.map.y)<reach)reserved=true;});return reserved;
}
static void forgetOriginal(mobj_t* m){diedOriginal(m);auto p=originalActors.find(m);if(p!=originalActors.end()){if(p->second>=0){auto& o=originals[p->second];o.state=2;o.actor=nullptr;}originalActors.erase(p);}}
static void respawnOriginal(mobj_t* old,mobj_t* next){auto p=originalActors.find(old);if(p==originalActors.end())return;int i=p->second;liveOriginalActors.erase(old);originalActors.erase(p);if(i>=0){originals[i].actor=next;originals[i].state=1;}originalActors.emplace(next,i);reviveOriginal(next);}
static void populationDouble(std::vector<unsigned char>& out,double value){uint64_t bits;static_assert(sizeof(bits)==sizeof(value));std::memcpy(&bits,&value,sizeof(bits));put(out,uint32_t(bits));put(out,uint32_t(bits>>32));}
static double populationDouble(const unsigned char* p){uint64_t bits=uint64_t(get(p))|(uint64_t(get(p+4))<<32);double value;std::memcpy(&value,&bits,sizeof(bits));return value;}
static void saveOriginals(std::vector<unsigned char>& out,const std::unordered_map<mobj_t*,uint32_t>& ids){
 if(!streaming)return;
 put(out,uint32_t(originals.size()));put(out,streamPolicy);put(out,uint32_t(std::max(0,packNext-leveltime)));put(out,eagerOriginals);put(out,uint32_t(originalQueue.size()));
 for(auto& o:originals){auto a=ids.find(o.actor);put(out,o.id);put(out,o.point);put(out,o.resolved);put(out,o.look);put(out,o.ticRoll);put(out,o.state==1&&a==ids.end()?2:o.state);put(out,a==ids.end()?0:a->second);put(out,uint32_t(std::max(0,o.retry-leveltime)));}
 auto queue=originalQueue;while(!queue.empty()){auto q=queue.top();queue.pop();put(out,q.index);populationDouble(out,q.distance);}
 std::vector<uint32_t> eager;for(auto& a:originalActors)if(a.second<0){auto id=ids.find(a.first);if(id!=ids.end())eager.push_back(id->second);}std::sort(eager.begin(),eager.end());
 const uint32_t meta[]={uint32_t(populationPolicy),filteredOriginals,requestedInitial,eagerOriginals,initialLive,initialDeferred,initialOverrun,uint32_t(draining),uint32_t(drainCursor),uint32_t(packCredits),uint32_t(deathCredit),routineOriginals,urgentOriginals,uint32_t(eager.size()),uint32_t(pendingRegions.size()),uint32_t(regionCursor),uint32_t(regionEntry),uint32_t(proximityQuery.stack.size()),uint32_t(proximityQuery.leaf+1),uint32_t(proximityQuery.entry),populationTics,blockedOriginals,uint32_t(progressPack),legacyProxyEager};
 for(auto v:meta)put(out,v);for(auto v:reasonCounts)put(out,v);
 for(int p=0;p<MAXPLAYERS;++p){put(out,uint32_t(queryX[p]));put(out,uint32_t(queryY[p]));put(out,uint32_t(querySector[p]+1));put(out,queryTic[p]<0?UINT32_MAX:uint32_t(std::max(0,TICRATE-(leveltime-queryTic[p]))));}
 for(double v:{proximityQuery.x,proximityQuery.y,proximityQuery.r,activationMs,setupMs,ticMs})populationDouble(out,v);
 for(auto id:eager)put(out,id);for(auto sector:pendingRegions)put(out,sector);for(auto node:proximityQuery.stack)put(out,node);
}
constexpr size_t populationMetaBytes=(24+10+MAXPLAYERS*4+12)*4;
static bool validateOriginalSave(const unsigned char* p,size_t bytes,int version){
 if(bytes<20)return false;uint32_t n=get(p),queued=get(p+16);size_t base=20+size_t(n)*32+size_t(queued)*12;
 if(n>pointLimit||queued>n||bytes<base||get(p+4)>2||get(p+8)>60*TICRATE)return false;
 if(version==2&&bytes!=base)return false;
 if(version==3){
  if(bytes<base+populationMetaBytes)return false;auto* m=p+base;uint32_t eager=get(m+13*4),regions=get(m+14*4),stack=get(m+17*4);
  if(get(m)>1||get(m+4)>pointLimit||get(m+8)>get(m+4)||get(m+12)>pointLimit||get(m+16)>pointLimit||get(m+20)>pointLimit||get(m+28)>1||get(m+32)>n||get(m+36)>64||get(m+40)>1||get(m+88)>1||eager>pointLimit||regions>uint32_t(numsectors)||get(m+60)>regions||get(m+64)>n||stack>2*n||get(m+72)>2*n||get(m+76)>n)return false;
  if(bytes!=base+populationMetaBytes+size_t(eager+regions+stack)*4)return false;
  auto* q=m+(24+10)*4;for(int i=0;i<MAXPLAYERS;++i,q+=16)if(get(q+8)>uint32_t(numsectors)||(get(q+12)!=UINT32_MAX&&get(q+12)>TICRATE))return false;
  for(int i=0;i<6;++i)if(!std::isfinite(populationDouble(q+i*8)))return false;
  q=m+populationMetaBytes+size_t(eager)*4;std::vector<unsigned char> seenRegions(numsectors,0);
  for(uint32_t i=0;i<regions;++i){uint32_t sector=get(q+i*4);if(sector>=uint32_t(numsectors)||seenRegions[sector]++)return false;}
 }
 std::vector<unsigned char> seen(points.size(),0);
 for(uint32_t i=0;i<n;++i){auto* r=p+20+i*32;uint32_t point=get(r+4),type=get(r+8),state=get(r+20);
  if(point>=points.size()||seen[point]++||type>=NUMMOBJTYPES||!profile(type)||get(r+12)>=MAXPLAYERS||get(r+16)>255||state>2||get(r+28)>TICRATE)return false;
  if(progression(type))return false;
 }
 seen.assign(n,0);for(uint32_t i=0;i<queued;++i){auto* r=p+20+n*32+i*12;uint32_t id=get(r);double distance=populationDouble(r+4);if(id>=n||seen[id]++||!std::isfinite(distance)||distance<0)return false;}
 return true;
}
static bool validateOriginalActorIds(const unsigned char* p,const std::vector<mobj_t*>& actors,int version){
 std::vector<unsigned char> seen(actors.size(),0);
 for(uint32_t i=0;i<get(p);++i){auto* r=p+20+i*32;uint32_t id=get(r+24);if(get(r+20)==1){if(!id||id>actors.size()||seen[id-1]++)return false;auto& point=points[get(r+4)];if(std::memcmp(&actors[id-1]->spawnpoint,&point.map,sizeof(mapthing_t)))return false;}else if(id)return false;}
 if(version==3){auto* m=p+20+get(p)*32+get(p+16)*12;auto* ids=m+populationMetaBytes;for(uint32_t i=0;i<get(m+52);++i){uint32_t id=get(ids+i*4);if(!id||id>actors.size()||seen[id-1]++||!originalMonster(actors[id-1]->type))return false;}}
 return true;
}
static void loadOriginals(const unsigned char* p,const std::vector<mobj_t*>& actors,int version){
 if(!validateOriginalActorIds(p,actors,version)){std::fprintf(stderr,"ED Population: invalid serialized original actor ownership; streaming suspended.\n");return;}
 streaming=populationRequested=supported();originalsResolved=true;streamPolicy=get(p+4);packNext=leveltime+get(p+8);eagerOriginals=get(p+12);originalExtents.fill(-1);
 for(uint32_t i=0;i<get(p);++i){auto* r=p+20+i*32;auto& point=points[get(r+4)];Original o;o.id=get(r);o.point=get(r+4);o.map=point.map;o.type=point.type;o.resolved=get(r+8);o.look=get(r+12);o.ticRoll=get(r+16);o.state=get(r+20);o.retry=leveltime+get(r+28);o.sector=point.sector;o.extent=std::max({originalExtent(o.type),originalExtent(o.resolved),double(mobjinfo[o.type].radius)/FRACUNIT+8});
  uint32_t id=get(r+24);if(o.state==1){o.actor=actors[id-1];originalActors.emplace(o.actor,int(originals.size()));reviveOriginal(o.actor);}originals.push_back(o);
 }finishOriginalIndex();preparePopulationRegions();
 for(uint32_t i=0;i<get(p+16);++i){auto* r=p+20+get(p)*32+i*12;int id=int(get(r));originalQueue.push({populationDouble(r+4),id});if(!originals[id].state)originals[id].queued=true;}
 if(version==2){
  // V2 had no initial-budget policy. Preserve its existing staged actors rather
  // than replan their population from current F4 preferences on load.
  populationPolicy=1;for(auto* m:actors)if(originalMonster(m->type)&&m->spawnpoint.type&&additional.find(m)==additional.end()&&originalActors.find(m)==originalActors.end()){originalActors.emplace(m,-1);reviveOriginal(m);}
  filteredOriginals=unsigned(originalActors.size());for(auto& o:originals)if(!o.state)++filteredOriginals;filteredOriginals=std::max(filteredOriginals,unsigned(points.size()));initialLive=unsigned(liveOriginalActors.size());requestedInitial=initialLive;initialDeferred=unsigned(std::count_if(originals.begin(),originals.end(),[](const Original& o){return !o.state;}));
  std::fprintf(stderr,"ED Population: legacy V2 save retains staged population; restart map to apply initial budget/safety settings.\n");return;
 }
 auto* m=p+20+get(p)*32+get(p+16)*12;
 populationPolicy=get(m);filteredOriginals=get(m+4);requestedInitial=get(m+8);eagerOriginals=get(m+12);initialLive=get(m+16);initialDeferred=get(m+20);initialOverrun=get(m+24);draining=get(m+28)!=0;drainCursor=get(m+32);packCredits=get(m+36);deathCredit=get(m+40)!=0;routineOriginals=get(m+44);urgentOriginals=get(m+48);populationTics=get(m+80);blockedOriginals=get(m+84);progressPack=get(m+88)!=0;legacyProxyEager=get(m+92);
 for(int r=0;r<10;++r)reasonCounts[r]=get(m+(24+r)*4);
 auto* q=m+(24+10)*4;for(int p=0;p<MAXPLAYERS;++p,q+=16){queryX[p]=int32_t(get(q));queryY[p]=int32_t(get(q+4));querySector[p]=int(get(q+8))-1;queryTic[p]=get(q+12)==UINT32_MAX?-1:leveltime-(TICRATE-int(get(q+12)));}
 proximityQuery.x=populationDouble(q);proximityQuery.y=populationDouble(q+8);proximityQuery.r=populationDouble(q+16);activationMs=populationDouble(q+24);setupMs=populationDouble(q+32);ticMs=populationDouble(q+40);
 q=m+populationMetaBytes;for(uint32_t i=0;i<get(m+52);++i,q+=4){auto* actor=actors[get(q)-1];originalActors.emplace(actor,-1);reviveOriginal(actor);}
 for(uint32_t i=0;i<get(m+56);++i,q+=4){int sector=get(q);pendingRegions.push_back(sector);regionQueued[sector]=1;}regionCursor=get(m+60);regionEntry=get(m+64);
 if(regionCursor<pendingRegions.size())regionEntry=std::min(regionEntry,sectorOriginals[pendingRegions[regionCursor]].size());else regionEntry=0;
 for(uint32_t i=0;i<get(m+68);++i,q+=4){int node=get(q);if(node>=0&&node<int(populationTree.size()))proximityQuery.stack.push_back(node);}
 proximityQuery.leaf=int(get(m+72))-1;proximityQuery.entry=get(m+76);
 if(proximityQuery.leaf>=int(populationTree.size()))proximityQuery.leaf=-1;
 if(proximityQuery.leaf>=0)proximityQuery.entry=std::min(proximityQuery.entry,populationTree[proximityQuery.leaf].entries.size());else proximityQuery.entry=0;
}
