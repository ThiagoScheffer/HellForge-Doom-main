// Exercise the real wound capture and drawing path without a WAD or GPU.
// The synthetic animation moves a textured body part inside a fixed outline.
#include "../platform/scene3d/internal.h"
#include "../platform/scene3d/gore_policy.h"
#include <cstdlib>
#include <fstream>
#include <iterator>
extern "C" {
#include "p_local.h"
}
namespace doom3d { int criticalThing(mobj_t*,critical_hit_t*); }

static void check(bool ok,const char *message) {
    if(!ok) {std::fprintf(stderr,"%s\n",message);std::exit(1);}
}

constexpr int firstTestSpriteLump=100;
constexpr int testSpriteCount=3;
static lumpinfo_t testLumps[firstTestSpriteLump+testSpriteCount]={};

extern "C" {
state_t states[NUMSTATES]={};
int firstspritelump=firstTestSpriteLump;
int numspritelumps=testSpriteCount;
int numsprites=NUMSPRITES;
int leveltime=0;
int p_critical_trace=0;
spritedef_t *sprites=nullptr;
unsigned r_levelserial=1;
lumpinfo_t *lumpinfo=testLumps;
int M_CheckParm(char*) {return 0;}
void (*R_GoreShotEnd)(mobj_t*,uint64_t)=nullptr;
}

namespace doom3d {
Settings settings;
unsigned levelSerial=1;
static std::array<Image,testSpriteCount> spriteImages;
static unsigned bodyQuadCalls=0;
static std::vector<GoreOutcome> goreBursts;
GoreBurstReport goreBurst(const mobj_t&,float,float,float,float,float,GoreOutcome outcome,uint64_t) {
    goreBursts.push_back(outcome);return {};
}
const std::vector<byte> &bloodPalette() {
    static const std::vector<byte> shades={16,32,48,64,80,96,112,128,144};
    return shades;
}
Image &lumpImage(int lump,bool flat) {
    check(!flat&&lump>=firstspritelump&&lump<firstspritelump+(int)spriteImages.size(),
          "Wounds must capture a valid sprite patch");
    return spriteImages[(size_t)(lump-firstspritelump)];
}
void quad(std::vector<Vertex> &out,Vertex a,Vertex b,Vertex c,Vertex d) {
    out.insert(out.end(),{a,b,c,a,c,d});
}
// A small deforming surface with known UVs. Capture must use the same
// triangle path as a drawn body, including lateral bend, depth and lift.
void bodyQuads(std::vector<Vertex> &out,const BodyPose &pose,const Vertex &base,float x,float y,float feet,float rx,float ry,
               float left,float right,int imageTop,int rows,float u0,float u1,const std::vector<byte> *holes) {
    check(!holes,"Impact capture must intersect the uncut body surface");
    ++bodyQuadCalls;
    auto corner=[&](float across,float u,float v) {
        float height=imageTop-v,shift=pose.lean[0]*height,ahead=pose.whip[0]*height;
        Vertex vertex=base;
        vertex.x=x+rx*(across+shift)-ry*ahead;
        vertex.y=y+ry*(across+shift)+rx*ahead;
        vertex.z=feet+height*(1-pose.squash);
        vertex.u=u;vertex.v=v;
        return vertex;
    };
    for(int row=0;row<rows;++row)
        quad(out,corner(left,u0,(float)row),corner(right,u1,(float)row),
             corner(right,u1,(float)row+1),corner(left,u0,(float)row+1));
}
}

using namespace doom3d;
static spritedef_t spriteDefs[NUMSPRITES]={};
static spriteframe_t frames[3]={};
static mobjinfo_t monsterInfo={};

static fixed_t fixed(float value) {return (fixed_t)std::lround(value*FRACUNIT);}

static void paint(Image &image,int x,int y,byte color) {
    size_t i=((size_t)y*image.width+x)*2;
    image.pixels[i]=color;image.pixels[i+1]=255;
}
static void mark(Image &image,int x,int y) {
    for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx)
        paint(image,x+dx,y+dy,(byte)(70+(dy+2)*5+dx+2));
}
static Image image(int markX,int markY,int width=36,int height=46,int left=18,int top=46) {
    Image out;
    out.width=width;out.height=height;out.left=left;out.top=top;
    out.pixels.assign((size_t)out.width*out.height*2,0);
    for(int y=0;y<out.height;++y)for(int x=0;x<out.width;++x)paint(out,x,y,20);
    mark(out,markX,markY);
    return out;
}
static mobj_t reset(bool flipped=false) {
    woundsShutdown();
    settings.wounds=1;
    bodyQuadCalls=0;
    leveltime=0;
    spriteImages[0]=image(24,20);
    spriteImages[1]=image(27,23);
    spriteImages[2]=image(21,17);
    for(int frame=0;frame<3;++frame) {
        frames[frame]={};
        for(int rotation=0;rotation<8;++rotation) {
            frames[frame].lump[rotation]=(short)frame;
            frames[frame].flip[rotation]=flipped;
        }
    }
    spriteDefs[SPR_POSS]={3,frames};
    sprites=spriteDefs;
    states[1].sprite=SPR_POSS;states[1].frame=0;
    monsterInfo={};monsterInfo.flags=MF_COUNTKILL;
    monsterInfo.spawnstate=(statenum_t)1;
    mobj_t monster={};
    monster.x=fixed(100);monster.y=fixed(200);monster.z=fixed(16);
    monster.height=fixed(46);monster.radius=fixed(16);
    monster.sprite=SPR_POSS;monster.frame=0;monster.angle=0;
    monster.info=&monsterInfo;monster.flags=MF_COUNTKILL;monster.health=100;
    monster.type=MT_POSSESSED;
    return monster;
}
static void hit(mobj_t &monster,float x,float y,float z,float dx,float dy,float slope=0,const mobj_t *shooter=nullptr) {
    woundHit(&monster,fixed(x),fixed(y),fixed(z),fixed(dx),fixed(dy),fixed(slope),1,shooter);
}
static std::vector<byte> mask(const mobj_t &monster,const Image &im,bool flip,float rx,float ry) {
    std::vector<byte> out;
    check(woundMask(monster,im,flip,rx,ry,out),"A shot viewed along its path must leave a wound");
    return out;
}
static void at(const std::vector<byte> &actual,const Image &im,float u,float v,const char *message) {
    check(actual.size()==(size_t)im.width*im.height,"The wound mask must cover the drawn patch");
    for(int y=0;y<im.height;++y)for(int x=0;x<im.width;++x) {
        float dx=x+0.5f-u,dy=y+0.5f-v;
        size_t i=(size_t)y*im.width+x;
        byte state=actual[i];bool opaque=im.pixels[i*2+1]!=0;
        check(state<=5,"Wound masks must contain only defined materials");
        check((state==1)==(opaque&&std::hypot(dx,dy)<=0.8f),message);
        check(opaque||state==0,"Blood must remain on opaque body texels");
        if(state)check(std::abs(dx)<=0.8f+2.6f&&dy>=-(0.8f+2.6f)&&dy<=0.8f+4.1f,
                       "Blood and its short smear must stay close to the impact");
    }
}
static void translated(const std::vector<byte> &actual,const std::vector<byte> &before,int width,int height,int dx,int dy) {
    for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
        int sx=x-dx,sy=y-dy;
        byte expected=sx>=0&&sx<width&&sy>=0&&sy<height?before[(size_t)sy*width+sx]:0;
        check(actual[(size_t)y*width+x]==expected,
              "The complete blood pattern must follow its animated body part without changing");
    }
}
static void woundMaterials() {
    Vertex base={};base.mode=MODE_SPRITE|MODE_RELIT|MODE_GLOWING_EYES;
    base.lampTint=0xff92817au;
    // One row contains every material, including an intact body texel and
    // a hole. These pass through the same run drawing as an animated body.
    const std::vector<byte> states={0,1,2,3,4,5};
    std::vector<Vertex> vertices;
    settings.bloodShine=1;
    maskedRow(vertices,states,(int)states.size(),0,false,[&](int edge,bool bottom) {
        Vertex v=base;v.u=(float)edge;v.v=bottom?1.0f:0.0f;return v;
    });
    check(vertices.size()==30,"Wound drawing must skip holes and preserve the other materials");
    unsigned darkest=255,brightest=0;
    for(size_t i=0;i<vertices.size();++i) {
        const Vertex &v=vertices[i];
        int state=(int)(i/6);if(state)++state;
        check(v.lampTint==base.lampTint,"Blood must retain the body's scene lighting tint");
        if(!state) {check(v.mode==base.mode,"Intact body texels must retain their material");continue;}
        check((v.mode&MODE_WOUND)!=0&&(v.mode&MODE_GLOWING_EYES)==0,
              "Blood texels must use their blood material without glowing as eyes");
        unsigned color=(v.mode>>MODE_WOUND_PALETTE_SHIFT)&255u;
        check(std::find(bloodPalette().begin(),bloodPalette().end(),color)!=bloodPalette().end(),
              "Blood colors must come from the game's blood palette");
        if(state==2)darkest=color;if(state==4)brightest=color;
        check(((v.mode&MODE_WOUND_STAIN)!=0)==(state==5),
              "Only the outer stain must blend blood into the body paint");
        check(((v.mode&MODE_WET_SPRITE)!=0)==(state==3||state==4),
              "Only the red rim and bright lip must receive wet blood highlights");
    }
    check(brightest>darkest,"The bright blood lip must use a brighter shade than its recessed edge");
    Vertex repainted=base;
    woundVertex(repainted,5);woundVertex(repainted,3);
    check((repainted.mode&MODE_WOUND_STAIN)==0&&(repainted.mode&MODE_WET_SPRITE)!=0,
          "Repainting a stain as a red rim must replace its previous material");
    settings.bloodShine=0;
    for(byte state=2;state<=5;++state) {
        Vertex v=base;v.mode|=MODE_WET_SPRITE;
        woundVertex(v,state);
        check(!(v.mode&MODE_WET_SPRITE),"Disabling blood shine must remove every wound highlight");
    }
}

static void criticalParts() {
    mobj_t monster=reset(),viewer={};viewer.y=fixed(199.5f);
    monster.flags|=MF_SHOOTABLE;
    settings.criticalParts=1;settings.accelerated=1;settings.wounds=0;
    auto &im=spriteImages[0];
    // A narrow head above broad shoulders; no hard-coded sprite height.
    for(int y=0;y<10;++y)for(int x=0;x<36;++x)
        im.pixels[((size_t)y*36+x)*2+1]=(x>=14&&x<22)?255:0;
    for(int y=3;y<9;++y)for(int x=30;x<33;++x)im.pixels[((size_t)y*36+x)*2+1]=255;
    auto surface=[&](bool flip=false,const BodyPose *pose=nullptr) {
        woundView(&monster,&viewer,im,flip,100,200,16,62,16,0,-1,pose);
    };
    surface();
    auto shot=[&](float y,float z) {
        critical_hit_t hit;
        check(criticalBegin(&viewer,0,fixed(y),fixed(z),fixed(256),0,0,fixed(256))==1,"Critical setting must opt in");
        check(criticalThing(&monster,&hit)==1,"Human sprites must use pixel collision");
        return hit;
    };
    for(auto type:{MT_POSSESSED,MT_SHOTGUY,MT_CHAINGUY,MT_WOLFSS}) {
        monster.type=type;
        auto head=shot(199.5f,57.5f);
        check(head.multiplier==2&&head.fraction==fixed(100.f/256),"All four human profiles must classify head pixels at the exact distance");
    }
    check(shot(199.5f,35.5f).multiplier==1,"Body pixels must keep normal damage");
    check(shot(190.5f,57.5f).fraction<0,"Transparent pixels beside the head must miss");
    auto arm=shot(185.5f,56.5f);
    check(arm.fraction>=0&&arm.multiplier==1,"A raised arm at head height must remain a body hit");
    check(shot(182.5f,35.5f).fraction>=0,"Visible shoulder pixels outside movement radius must hit");
    monster.height=fixed(32);
    check(shot(199.5f,57.5f).multiplier==2,"Head pixels above actor collision height must remain hittable");
    surface(true);
    check(shot(199.5f,57.5f).multiplier==2,"Mirrored heads must classify in authored sprite coordinates");
    surface();
    criticalVoxelModel=[](const mobj_t&){return true;};
    critical_hit_t excluded;
    check(criticalThing(&monster,&excluded)==0,"Voxel models must use original collision");criticalVoxelModel=nullptr;
    for(auto type:{MT_HEAD,MT_TROOP,MT_CYBORG,MT_SKULL,MT_BARREL}) {
        monster.type=type;check(criticalThing(&monster,&excluded)==0,"Non-human types must retain ordinary collision");
    }
    monster.type=MT_POSSESSED;
    settings.wounds=1;
    auto head=shot(199.5f,57.5f);p_critical_trace=1;
    // A different effect location cannot move the hole off the classified texel.
    woundHit(&monster,0,0,0,fixed(1),0,0,1,&viewer);p_critical_trace=0;
    at(mask(monster,im,false,0,-1),im,18.5f,4.5f,"Wounds must reuse the classified impact");
    check(shot(199.5f,57.5f).multiplier==2,"Existing cosmetic holes must not allow penetration");
    BodyPose pose{};pose.lean[0]=0.2f;surface(false,&pose);
    check(shot(191.2f,57.5f).multiplier==2,"Bent head pixels must follow the posed surface");
    check(shot(199.5f,57.5f).fraction<0,"The old location of a bent head must miss");
    surface();
    // Move the actual ray origin independently from the viewer (corner peek).
    check(criticalBegin(&viewer,fixed(40),fixed(199.5f),fixed(57.5f),fixed(256),0,0,fixed(256))==1,"Peek ray begins");
    check(criticalThing(&monster,&head)&&head.fraction==fixed(60.f/256),"Peek origin must set the correct intersection distance");
    settings.enhancedWeakPoints=1;P_WeakpointConfigure(1,0,0);
    for(auto type:{MT_TROOP,MT_UNDEAD,MT_VILE,MT_KNIGHT,MT_BRUISER}) {
        monster.type=type;
        auto demonHead=shot(199.5f,57.5f);
        check(demonHead.percent==200,"Additional humanoid heads must use the shared ballistic pipeline");
        check(shot(199.5f,35.5f).percent==100,"Additional humanoid torsos must keep normal damage");
    }
    settings.enhancedWeakPoints=0;P_WeakpointConfigure(0,0,0);
    settings.criticalParts=0;
    check(!criticalBegin(&viewer,0,0,0,FRACUNIT,0,0,FRACUNIT),"Default off restores the ordinary path");
    woundsShutdown();
    std::puts("Critical sprite classification checks passed");
}


static void gorePolicy() {
    using O=GoreOutcome;
    using R=GoreReason;
    GoreDecision shot;
    shot.type=MT_POSSESSED;shot.headEnabled=true;shot.legacyStock=true;shot.effectiveDamage=20;
    shot.contacts=4;shot.spriteContacts=3;shot.legacyHeads=2;
    shot.hits[(unsigned)GoreRegion::Chest]=1;
    check(goreCandidate(shot).outcome==O::HeadBurst,"M1 uses all admitted contacts for the head majority");
    ++shot.contacts;
    check(goreCandidate(shot).outcome==O::HumanFlesh,"A human torso kill keeps ordinary corpse art");
    shot.extreme=true;shot.contacts=4;
    check(goreCandidate(shot).outcome==O::HumanFlesh,"Extreme human death bypasses the head cut");

    shot={};shot.type=MT_HEAD;shot.bodyEnabled=true;shot.maxHP=400;shot.postHP=200;
    shot.effectiveDamage=20;shot.spriteContacts=1;
    check(goreCandidate(shot).reason==R::Opaque,"A single Cacodemon contact requires 30 damage");
    ++shot.spriteContacts;
    check(goreCandidate(shot).outcome==O::CacoViscera,"Two contacts qualify at half HP and 20 health lost");
    shot.already=true;
    check(goreCandidate(shot).reason==R::Already,"Living Cacodemon ejections occur once");
    shot.postHP=0;shot.effectiveDamage=25;shot.spriteContacts=1;
    check(goreCandidate(shot).outcome==O::CacoViscera,"A separate one-contact fatal accent can follow the living burst");
    shot.distance=129;
    check(goreCandidate(shot).reason==R::Range,"Cacodemon gore stops beyond 128 units");

    for(auto type:{MT_POSSESSED,MT_SHOTGUY,MT_TROOP,MT_FATSO}) {
        shot={};shot.type=type;shot.bodyEnabled=true;shot.maxHP=60;shot.postHP=30;
        shot.spriteContacts=1;shot.hits[(unsigned)GoreRegion::Chest]=1;shot.effectiveDamage=5;
        check(goreCandidate(shot).outcome==O::ChestCavity,"One torso pellet qualifies at half health");
        ++shot.postHP;
        check(goreCandidate(shot).reason==R::Health,"Above-threshold HP blocks a living injury");
        shot.maxHP=0;
        check(goreCandidate(shot).reason==R::Health,"An invalid maximum HP blocks a living injury");
    }
    shot.type=MT_TROOP;
    shot.postHP=0;
    check(goreCandidate(shot).outcome==O::ImpTissue,"Fatal Imp body hits select tissue without a torso mask");
    shot.type=MT_FATSO;
    check(goreCandidate(shot).reason==R::Damage,"The Mancubus retains its fatal severity gate");
    shot.effectiveDamage=35;
    check(goreCandidate(shot).outcome==O::MancubusHeavy,"Fatal Mancubus hits select heavy tissue without a torso mask");
    shot.type=MT_SERGEANT;shot.postHP=45;shot.maxHP=100;shot.effectiveDamage=20;
    check(goreCandidate(shot).outcome==O::PinkyViscera,"Pinky selects only viscera");
    shot.type=MT_SHADOWS;
    check(goreCandidate(shot).outcome==O::None,"Spectres are excluded");
    shot.type=MT_UNDEAD;shot.effectiveDamage=30;
    check(goreCandidate(shot).outcome==O::RevenantBone,"A substantial surviving Revenant rib hit selects only bone");
}

static void goreShot(mobj_t &monster,float u,float v,int damage,int pellets,float distance=96,
                     bool remove=false,bool changeLevel=false) {
    const Image &image=spriteImages[0];
    mobj_t shooter={};
    player_t player={};
    shooter.player=&player;shooter.x=monster.x;shooter.y=monster.y-fixed(distance);shooter.angle=ANG90;
    const size_t previous=goreBursts.size();
    P_WeakpointBeginShot(&shooter,wp_supershotgun);
    for(int pellet=0;pellet<pellets&&monster.health>0;++pellet) {
        goreCapture(&monster,&shooter,monster.x+fixed(u-image.left),monster.y-fixed(10),
                    monster.z+fixed(image.top-v),0,FRACUNIT,0);
        monster.health-=damage;
        if(monster.health<=0)forgetGore(&monster,1);
        goreAfterHit(&monster,&shooter,WP_SHOTGUN_PELLET,monster.health,0,damage);
    }
    check(goreBursts.size()==previous,"Any major effect must wait for discharge finalization");
    const auto discharge=P_WeakpointDischarge(&shooter);
    P_WeakpointEndShot();
    if(remove)forgetWounds(&monster);
    if(changeLevel)++r_levelserial;
    R_GoreShotEnd(&shooter,discharge);
    R_GoreShotEnd(&shooter,discharge);
    if(changeLevel)--r_levelserial;
    check(goreBursts.size()<=previous+1,"A discharge cannot emit twice for one victim");
}
static void configureGore() {
    settings.accelerated=1;settings.goreLevel=1;settings.blood=1;
    settings.goreBodyDamage=1;settings.goreFragments=1;settings.wounds=0;
    totalkills=199;P_WeakpointConfigure(0,0,0);R_GoreShotEnd=goreFinishShot;
    goreBursts.clear();
}

static Image iwadSprite(const std::vector<byte> &wad,const char *name) {
    auto read16=[&](size_t at){return unsigned(wad.at(at))|(unsigned(wad.at(at+1))<<8);};
    auto read32=[&](size_t at){return read16(at)|(read16(at+2)<<16);};
    const unsigned directory=read32(8), entries=read32(4);
    for(unsigned i=0;i<entries;++i) {
        const size_t entry=directory+16ull*i;
        check(entry+16<=wad.size(),"IWAD directory must fit in the file");
        if(std::strncmp((const char*)wad.data()+entry+8,name,8))continue;
        const unsigned offset=read32(entry);
        const int width=read16(offset),height=read16(offset+2);
        Image img;
        img.width=width;img.height=height;img.left=(int16_t)read16(offset+4);img.top=(int16_t)read16(offset+6);
        img.pixels.assign((size_t)width*height*2,0);
        for(int x=0;x<width;++x) {
            size_t post=offset+read32(offset+8+x*4);
            while(wad.at(post)!=255) {
                const int first=wad.at(post),count=wad.at(post+1);
                for(int y=first;y<first+count&&y<height;++y) {
                    const size_t at=((size_t)y*width+x)*2;
                    img.pixels[at]=wad.at(post+3+y-first);img.pixels[at+1]=255;
                }
                post+=count+4;
            }
        }
        return img;
    }
    check(false,"Requested stock sprite must be present in the test IWAD");
    return {};
}

static void goreDischargeFinalization(const std::vector<byte> &wad) {
    using O=GoreOutcome;
    Settings previous=settings;
    auto monster=reset();
    configureGore();
    if(!wad.empty())spriteImages[0]=iwadSprite(wad,"HEADA1");
    monster.type=MT_HEAD;monsterInfo.spawnhealth=400;
    const float u=float(spriteImages[0].width)*.5f,v=float(spriteImages[0].height)*.49f;
    monster.health=220;
    goreShot(monster,u,v,10,2,128);
    check(goreBursts==std::vector<O>{O::CacoViscera},"A severe near-death Cacodemon hit emits while alive");
    goreShot(monster,u,v,10,2);
    check(goreBursts.size()==1,"A second severe living shot cannot repeat the ejection");
    monster.health=24;
    goreShot(monster,u,v,25,1);
    check(goreBursts==std::vector<O>{O::CacoViscera,O::CacoViscera},"A separate permissive fatal burst follows the living burst");
    std::vector<byte> mask;
    check(!woundMask(monster,spriteImages[0],false,1,0,mask),"Viscera never applies a corpse mask");

    for(int scenario=0;scenario<7;++scenario) {
        forgetWounds(&monster);goreBursts.clear();monster.health=24;
        totalkills=scenario==0?200:199;
        settings.goreLevel=scenario==4?0:1;
        settings.blood=scenario==5?0:1;
        settings.goreBodyDamage=scenario==6?0:1;
        goreShot(monster,u,v,25,1,scenario==1?129:96,scenario==2,scenario==3);
        check(goreBursts.empty(),"Population, range, removal, levels and options gate the fatal effect");
    }
    if(!wad.empty())std::puts("Doom II Cacodemon sprite discharge checks passed");
    R_GoreShotEnd=nullptr;totalkills=0;settings=previous;
}

static void goreStockLiveMasks(const std::vector<byte> &wad) {
    using O=GoreOutcome;
    Settings previous=settings;
    struct Fixture {
        mobjtype_t type;
        int health;
        const char *front,*quarter;
        float u,v;
        O fatal;
    };
    const Fixture fixtures[]={
        {MT_POSSESSED,20,"POSSA1","POSSA2A8",16,14,O::HumanFlesh},
        {MT_SHOTGUY,30,"SPOSA1","SPOSA2A8",16,14,O::HumanFlesh},
        {MT_TROOP,60,"TROOA1","TROOA2A8",20,19,O::ImpTissue},
        {MT_FATSO,600,"FATTB1","FATTB2B8",43,28,O::MancubusHeavy},
    };
    for(const auto &fixture:fixtures) {
        auto monster=reset();
        configureGore();
        monster.type=fixture.type;
        monsterInfo.spawnhealth=fixture.health;
        monster.health=fixture.health/2+10;
        spriteImages[0]=iwadSprite(wad,fixture.front);
        spriteImages[1]=iwadSprite(wad,fixture.quarter);
        spriteImages[2]=spriteImages[0];
        spriteImages[2].pixels[0]^=1;
        const float u=fixture.u,v=fixture.v;
        goreShot(monster,u,v,5,1);
        check(goreBursts.empty(),"A torso hit above half health has no major wound");
        goreShot(monster,u,v,5,1);
        check(goreBursts==std::vector<O>{O::ChestCavity},"Stock low-HP torso hit creates one living chest injury");
        std::vector<byte> mask;
        check(woundMask(monster,spriteImages[0],false,1,0,mask),"Living cavity applies independently of Wound Holes");
        check(std::find(mask.begin(),mask.end(),1)!=mask.end()&&std::find(mask.begin(),mask.end(),3)!=mask.end(),
              "Living cavity cuts a small opening with a bloodied rim");
        check(woundMask(monster,spriteImages[1],true,1,0,mask),"Verified mirrored quarter view carries the injury");
        check(!woundMask(monster,spriteImages[2],false,1,0,mask),"Replacement pixels cannot inherit a stock overlay");
        WoundSpot spot{};
        check(woundSpots(&monster,&spot,1,true)==1&&spot.critical,"Each verified living cavity contributes a budgeted drip");
        goreShot(monster,u,v,5,1);
        check(goreBursts.size()==1,"Living cavity emits once per life");
        goreShot(monster,u,v,monster.health,1);
        check(goreBursts.size()==2&&goreBursts.back()==fixture.fatal,
              "Fatal effects have independent particle presentation");
        check(!woundMask(monster,spriteImages[0],false,1,0,mask),"A fatal discharge clears the living overlay");
        monster.health=25;
        check(!woundMask(monster,spriteImages[0],false,1,0,mask),"Living overlay is gone after death even if HP returns");
    }
    R_GoreShotEnd=nullptr;totalkills=0;settings=previous;
    std::puts("Doom II half-health stock living mask checks passed for all four monsters");
}

static void goreStockPinky(const std::vector<byte> &wad) {
    const Settings previous=settings;
    auto monster=reset();
    configureGore();
    spriteImages[0]=iwadSprite(wad,"SARGA1");
    monster.type=MT_SERGEANT;monster.health=80;monsterInfo.spawnhealth=150;
    goreShot(monster,20,10,20,1);
    check(goreBursts==std::vector<GoreOutcome>{GoreOutcome::PinkyViscera},"The exposed frontal Pinky torso qualifies");
    std::vector<byte> mask;
    check(!woundMask(monster,spriteImages[0],false,1,0,mask),"Surviving Pinky retains its silhouette");
    goreShot(monster,20,10,30,2);
    check(goreBursts.size()==2&&goreBursts.back()==GoreOutcome::PinkyViscera,"Pinky gets one stronger fatal accent");
    check(!woundMask(monster,spriteImages[0],false,1,0,mask),"Fatal Pinky retains its silhouette");
    monster.type=MT_SHADOWS;monster.health=80;
    goreShot(monster,20,10,20,1);
    check(goreBursts.size()==2,"Stock Spectre sprite matches never enter the Gore policy");
    R_GoreShotEnd=nullptr;totalkills=0;settings=previous;
    std::puts("Doom II Pinky and Spectre discharge checks passed");
}

int main(int argc,char **argv) {
    std::vector<byte> wad;
    if(argc>1) {
        std::ifstream file(argv[1],std::ios::binary);
        wad.assign(std::istreambuf_iterator<char>(file),{});
        check(wad.size()>=12&&!std::memcmp(wad.data(),"IWAD",4),"Expected a readable test IWAD");
    }
    gorePolicy();
    goreDischargeFinalization(wad);
    if(!wad.empty()) {
        goreStockLiveMasks(wad);
        goreStockPinky(wad);
    }
    criticalParts();
    woundMaterials();
    mobj_t monster=reset();
    // The callback is the blood/puff point before the cylinder. Project it
    // forward onto the sprite plane at x=100, keeping the lateral impact.
    hit(monster,74,193.5f,41.5f,1,0);
    auto bloody=mask(monster,spriteImages[0],false,0,-1);
    at(bloody,spriteImages[0],24.5f,20.5f,
       "Front shots must cut the aimed texel rather than the body center");
    bool red=false,stain=false,trail=false;
    for(int y=0;y<46;++y)for(int x=0;x<36;++x) {
        byte state=bloody[(size_t)y*36+x];
        red=red||state==3||state==4;stain=stain||state==5;
        trail=trail||(state>=3&&y-20>2.5f&&std::abs(x-24)<=1);
    }
    check(red&&stain,"A puncture must include red blood and a surrounding stain");
    check(trail,"A bloody puncture must leave a short downward smear");
    auto mirrored=mask(monster,spriteImages[0],true,0,-1);
    for(int y=0;y<46;++y)for(int x=0;x<36;++x)
        check(bloody[(size_t)y*36+x]==mirrored[(size_t)y*36+35-x],
              "Mirroring the body must mirror the entire blood pattern with it");
    spriteImages[0].pixels[((size_t)22*36+24)*2+1]=0;
    at(mask(monster,spriteImages[0],false,0,-1),spriteImages[0],24.5f,20.5f,
       "Clipping a blood mark to transparent artwork must preserve the puncture");
    std::vector<byte> closed;
    check(!woundMask(monster,spriteImages[0],false,1,0,closed),
          "A tunnel viewed perpendicular to the shot must close");

    monster=reset();
    hit(monster,74,193.75f,41.25f,1,0);
    at(mask(monster,spriteImages[0],false,0,-1),spriteImages[0],24.25f,20.75f,
       "The original hit frame must preserve the impact's fractional texel position");

    monster=reset();
    hit(monster,106.5f,174,41.5f,0,1);
    at(mask(monster,spriteImages[0],false,1,0),spriteImages[0],24.5f,20.5f,
       "Side shots must use the shot's sprite plane and remain visible from that view");

    monster=reset();
    hit(monster,74,193.5f,35,1,0,0.25f);
    at(mask(monster,spriteImages[0],false,0,-1),spriteImages[0],24.5f,20.5f,
       "The shot slope must advance the puff height to the body plane");

    // Correspondence always starts at the impact art, so returning to a
    // previous animation frame cannot accumulate drift.
    monster=reset();
    hit(monster,74,193.5f,41.5f,1,0);
    auto original=mask(monster,spriteImages[0],false,0,-1);
    monster.frame=1;
    auto animated=mask(monster,spriteImages[1],false,0,-1);
    at(animated,spriteImages[1],27.5f,23.5f,
       "An animated body part must carry the wound to its new texel");
    translated(animated,original,36,46,3,3);
    monster.frame=2;
    animated=mask(monster,spriteImages[2],false,0,-1);
    at(animated,spriteImages[2],21.5f,17.5f,
       "Another animation frame must map directly from the original impact");
    translated(animated,original,36,46,-3,-3);
    monster.frame=0;
    leveltime+=37;
    check(mask(monster,spriteImages[0],false,0,-1)==original,
          "Blood must not flicker with time or change after an animation cycle");
    at(mask(monster,spriteImages[0],false,0,-1),spriteImages[0],24.5f,20.5f,
       "Returning to the hit frame must restore its exact original anchor");
    monster.x+=fixed(30);monster.y-=fixed(11);monster.z+=fixed(5);
    at(mask(monster,spriteImages[0],false,0,-1),spriteImages[0],24.5f,20.5f,
       "Walking must keep the wound attached to the monster");
    monster.angle=ANG90;
    at(mask(monster,spriteImages[0],false,1,0),spriteImages[0],24.5f,20.5f,
       "Turning must rotate the wound's tunnel with the monster");

    monster=reset();
    hit(monster,74,193.5f,41.5f,1,0);
    spriteImages[1]=image(30,24,42,50,24,50);
    monster.frame=1;
    at(mask(monster,spriteImages[1],false,0,-1),spriteImages[1],30.5f,24.5f,
       "Animation patches with different sizes and offsets must retain the attached artwork");

    monster=reset(true);
    spriteImages[0]=image(11,20);spriteImages[1]=image(14,23);
    hit(monster,74,193.5f,41.5f,1,0);
    at(mask(monster,spriteImages[0],true,0,-1),spriteImages[0],11.5f,20.5f,
       "Mirrored impact frames must anchor in the authored texture coordinates");
    monster.frame=1;
    at(mask(monster,spriteImages[1],true,0,-1),spriteImages[1],14.5f,23.5f,
       "Mirrored animations must carry the same authored wound patch");

    forgetWounds(&monster);
    check(!woundMask(monster,spriteImages[1],true,0,-1,closed),
          "Removing a monster must discard its wounds");
    hit(monster,74,191.5f,36.5f,1,0);
    at(mask(monster,spriteImages[1],true,0,-1),spriteImages[1],9.5f,25.5f,
       "Reusing a removed monster's address must not inherit its old wounds");
    monster=reset();hit(monster,74,193.5f,41.5f,1,0);
    woundsShutdown();
    check(!woundMask(monster,spriteImages[0],false,0,-1,closed),
          "Scene shutdown must discard all wounds");
    settings.wounds=0;
    hit(monster,74,193.5f,41.5f,1,0);
    settings.wounds=1;
    check(!woundMask(monster,spriteImages[0],false,0,-1,closed),
          "Disabled wounds must not record later-visible impacts");

    // The game can already have moved to a new tic or sprite after the
    // frame the player aimed at. Capture the remembered rendered surface.
    mobj_t viewer={};
    viewer.x=fixed(0);viewer.y=fixed(194.5f);viewer.angle=0;
    monster=reset();
    woundView(&monster,&viewer,spriteImages[1],false,104,204,20,68,22,0,-1);
    hit(monster,74,194.5f,44.5f,1,0,0,&viewer);
    at(mask(monster,spriteImages[1],false,0,-1),spriteImages[1],27.5f,23.5f,
       "Impact capture must use the remembered image, interpolated position and drawn vertical bounds");
    check(bodyQuadCalls==0,"An undeformed remembered billboard needs no body reconstruction");

    monster=reset();
    BodyPose pose={};pose.lean[0]=0.2f;pose.whip[0]=0.1f;pose.squash=0.2f;pose.lift=3;
    viewer.y=fixed(190);
    woundView(&monster,&viewer,spriteImages[1],false,104,204,23,69,23,0,-1,&pose);
    pose={}; // A remembered surface owns its pose rather than this pointer.
    hit(monster,74,190,41,1,0,0,&viewer);
    at(mask(monster,spriteImages[1],false,0,-1),spriteImages[1],27.5f,23.5f,
       "A flinching surface must map its visible triangle back to the struck source texel");
    check(bodyQuadCalls==1,"A posed hit must reconstruct the remembered body once on demand");

    monster=reset();viewer.y=fixed(194.5f);
    woundView(&monster,&viewer,spriteImages[1],false,70,204,20,68,22,0,-1);
    hit(monster,74,194.5f,44.5f,1,0,0,&viewer);
    at(mask(monster,spriteImages[1],false,0,-1),spriteImages[1],27.5f,23.5f,
       "A visible surface in front of the puff must still be struck by the shooter's ray");

    monster=reset();viewer.y=fixed(200.5f);
    woundView(&monster,&viewer,spriteImages[1],true,104,204,20,68,22,0,-1);
    hit(monster,74,200.5f,44.5f,1,0,0,&viewer);
    at(mask(monster,spriteImages[1],true,0,-1),spriteImages[1],14.5f,23.5f,
       "Remembered mirrored surfaces must invert their UVs when capturing the hit");

    monster=reset();viewer.y=fixed(194.5f);
    spriteImages[1].pixels[((size_t)23*spriteImages[1].width+27)*2+1]=0;
    woundView(&monster,&viewer,spriteImages[1],false,104,204,20,68,22,0,-1);
    hit(monster,74,194.5f,44.5f,1,0,0,&viewer);
    check(!woundMask(monster,spriteImages[0],false,0,-1,closed),
          "A ray through transparent remembered art must not cut a different body texel");

    monster=reset();
    mobj_t otherViewer=viewer;
    woundView(&monster,&viewer,spriteImages[1],false,104,204,20,68,22,0,-1);
    hit(monster,74,193.5f,41.5f,1,0,0,&otherViewer);
    at(mask(monster,spriteImages[0],false,0,-1),spriteImages[0],24.5f,20.5f,
       "Another shooter's hit must not use the local viewer's remembered surface");

    monster=reset();
    woundView(&monster,&viewer,spriteImages[1],false,104,204,20,68,22,0,-1);
    leveltime+=3;
    hit(monster,74,193.5f,41.5f,1,0,0,&viewer);
    at(mask(monster,spriteImages[0],false,0,-1),spriteImages[0],24.5f,20.5f,
       "A stale remembered view must fall back to the current body");

    monster=reset();
    woundView(&monster,&viewer,spriteImages[1],false,104,204,20,68,22,0,-1);
    forgetWounds(&monster);
    hit(monster,74,193.5f,41.5f,1,0,0,&viewer);
    at(mask(monster,spriteImages[0],false,0,-1),spriteImages[0],24.5f,20.5f,
       "Removing a monster must also discard its remembered rendered surface");

    monster=reset();
    player_t player={};viewer.player=&player;viewer.y=fixed(143.5f);
    hit(monster,74,180.5f,41.5f,1,0.5f,0,&viewer);
    at(mask(monster,spriteImages[0],false,0,-1),spriteImages[0],24.5f,20.5f,
       "A spread pellet must intersect the player's camera plane rather than rotate its own billboard");

    // The sprite rotation comes from the viewer-to-body center bearing.
    // Here the center is at 22 degrees and the actual ray is at 24, on
    // opposite sides of the 22.5-degree sprite-rotation boundary.
    monster=reset();
    frames[0].rotate=true;
    for(int rotation=0;rotation<8;++rotation)frames[0].lump[rotation]=2;
    frames[0].lump[4]=0;
    std::fill(spriteImages[2].pixels.begin(),spriteImages[2].pixels.end(),0);
    float rayAngle=24*doomPi/180,centerAngle=22*doomPi/180;
    float dx=std::cos(rayAngle),dy=std::sin(rayAngle);
    float viewerY=200-100*std::tan(centerAngle);
    viewer.y=fixed(viewerY);
    viewer.angle=(angle_t)std::llround(24*(4294967296.0/360));
    float distance=100*dx+(200-viewerY)*dy;
    float impactX=distance*dx,impactY=viewerY+distance*dy;
    float across=(impactX-100)*dy-(impactY-200)*dx;
    hit(monster,impactX-26*dx,impactY-26*dy,41.5f,dx,dy,0,&viewer);
    at(mask(monster,spriteImages[0],false,dy,-dx),spriteImages[0],18+across,20.5f,
       "Off-center rays must use the rotation selected from the player's position");

    // Nearby pellets form one larger attached hole centered between hits,
    // rather than growing a hole that never reaches the second impact.
    monster=reset();
    woundHit(&monster,fixed(74),fixed(193.5f),fixed(41.5f),fixed(1),0,0,15);
    woundHit(&monster,fixed(74),fixed(190.5f),fixed(41.5f),fixed(1),0,0,15);
    auto merged=mask(monster,spriteImages[0],false,0,-1);
    check(merged[20*36+24]==1&&merged[20*36+27]==1,
          "A merged pellet hole must include both original impact texels");
    leveltime+=37;
    check(mask(monster,spriteImages[0],false,0,-1)==merged,
          "A merged wound's blood pattern must remain stable after the shot");
    return 0;
}
