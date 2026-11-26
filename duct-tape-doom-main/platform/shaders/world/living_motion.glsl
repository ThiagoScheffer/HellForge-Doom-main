// The artwork's red channels distinguish exposed tissue from bone and trim.
float livingRed(vec3 rgb) {
    return smoothstep(0.035,0.24,rgb.r-rgb.g*1.55)*smoothstep(0.035,0.14,rgb.r);
}

float livingRunoff(vec2 tile,vec2 size,float seed,float strength,float tissue,inout vec3 rgb) {
    float count=max(1.0,round(size.x/16.0));
    float lanes=tile.x/size.x*count+0.14*sin(tile.y/size.y*6.283185+seed);
    float lane=mod(floor(lanes),count);
    float age=fract(tile.y/size.y-c.fog.w*0.23+0.35*sin(lane*2.51+seed));
    float trail=(1.0-smoothstep(0.21,0.39,age))*smoothstep(0.01,0.07,age);
    float width=0.07+0.09*trail;
    float seep=trail*(1.0-smoothstep(width,width+0.06,abs(fract(lanes)-0.5)))*tissue*strength;
    float lip=(1.0-smoothstep(0.025,0.07,abs(age-0.25)))*seep;
    rgb=mix(rgb,rgb*vec3(0.60,0.20,0.23)+vec3(0.09,0.004,0.007),saturate(seep*0.85));
    rgb+=vec3(0.20,0.043,0.053)*lip;
    return seep;
}

// Stock leather panels have fixed iron bands along top, bottom and joins.
// Inside each panel a palm and four elongated fingertips push outward.
vec2 leatherPressure(vec2 tile,vec2 size,float seed,out float light) {
    float x=tile.x;
    float edge=min(x,size.x-x);
    float center=size.x*0.5;
    if(size.x==256.0) {
        edge=min(edge,min(abs(x-80.0),abs(x-168.0)));
        center=x<80.0?40.0:x<168.0?124.0:212.0;
    }
    float skin=smoothstep(11.0,16.0,min(edge,min(tile.y,size.y-tile.y)));
    vec2 hand=tile-vec2(center+6.0*sin(seed+center),size.y*0.54+9.0*sin(seed+center*0.07));
    float cycle=sin(c.fog.w*1.54+center*0.073+seed);
    float press=smoothstep(-0.48,0.93,cycle)*skin;
    vec2 palm=(hand-vec2(0,5))/vec2(16,19);
    float bulge=1.0-smoothstep(0.12,1.25,dot(palm,palm));
    vec2 fingers=(hand-vec2(0,-13))/vec2(14,18);
    float tips=(1.0-smoothstep(0.55,1.16,dot(fingers,fingers)))*
               (0.45+0.55*cos(clamp(fingers.x,-1.0,1.0)*12.56637));
    float pressure=max(bulge,0.85*tips)*press;
    light=pressure*(0.43-0.17*palm.y);
    return -hand*pressure*0.27;
}

float featureMask(vec2 delta,vec2 radius) {
    vec2 p=delta/radius;
    return 1.0-smoothstep(0.20,1.0,dot(p,p));
}

// Landmark coordinates refer to the original 128 x 96 artwork.
const vec4 trappedEyes[5]=vec4[5](
    vec4(87,15,102,16),vec4(89,53,104,54),vec4(111,43,124,43),
    vec4(13,44,25,44),vec4(14,75,27,74));
const vec2 trappedMouths[5]=vec2[5](vec2(94,32),vec2(96,64),vec2(117,55),vec2(19,54),vec2(21,86));

vec2 trappedFaceMotion(vec2 tile,float seed) {
    for(int i=0;i<5;++i) {
        vec2 mouth=tile-trappedMouths[i];
        if(abs(mouth.x)>23.0||mouth.y< -26.0||mouth.y>12.0)continue;
        vec4 eyes=trappedEyes[i];
        float eye=max(featureMask(tile-eyes.xy,vec2(7,6)),featureMask(tile-eyes.zw,vec2(7,6)));
        float lips=featureMask(mouth,vec2(13,11));
        if(max(eye,lips)<=0.0)continue;
        float phase=c.fog.w*(1.5+float(i)*0.17)+seed+float(i)*2.28;
        return eye*vec2(1.8*sin(phase*0.72),0.8*cos(phase))+
               vec2(0.0,clamp(mouth.y,-6.0,6.0)*lips*0.57*cos(phase));
    }
    return vec2(0);
}

const vec4 marbleEyes[3]=vec4[3](vec4(45.5,61.0,81.5,61.0),vec4(51.0,77.0,78.0,77.0),
                                 vec4(48.0,64.0,80.0,64.0));
const vec4 skullEyes[5]=vec4[5](vec4(14.5,23.5,21.5,23.5),vec4(14.5,23.5,21.5,23.5),
                                vec4(6.5,17.5,15.5,17.5),vec4(6.5,17.5,15.5,17.5),
                                vec4(20.1,20.0,34.1,20.0));

// Both stock skull sizes paint the same three rows per socket. Reflect
// the right socket into the left so every animated texel stays inside the art.
bool paintedSkullSocket(vec2 tile,bool wide,bool right) {
    ivec2 pixel=ivec2(floor(tile));
    int row=pixel.y-(wide?22:16);
    int column=(right?(wide?35:21)-pixel.x:pixel.x)-(wide?12:4);
    return row>=0&&row<=2&&column>=(row==0?1:0)&&column<=(row==2?3:4);
}

float engravedBlink(float clock,float seed) {
    float cycle=(clock+seed*8.0)/6.3;
    float beat=fract(cycle)*6.3;
    float variation=fract(sin(floor(cycle)*13.137+seed*71.57)*2053.31);
    float dt=beat-(0.7+variation*4.15);
    float blink=smoothstep(-0.10,-0.018,dt)*(1.0-smoothstep(0.029,0.16,dt));
    // Short reflex blink after some of the longer pauses.
    float reflex=smoothstep(0.24,0.31,dt)*(1.0-smoothstep(0.34,0.46,dt));
    return max(blink,variation>0.77?reflex:0.0);
}

float engravedAperture(vec2 p,float open,float edge) {
    float along=p.x/0.87;
    float oval=sqrt(max(0.0,1.0-along*along));
    return smoothstep(-edge,edge,p.y+oval*(0.66*open-0.15))*
           smoothstep(-edge,edge,oval*(0.19+0.17*open)-p.y)*
           smoothstep(0.0,0.21,oval)*smoothstep(0.0,0.19,open);
}

float engravedEyes(bool stone,uint variant,vec2 tile,bool morph,bool pulse,float strength,float footprint,
                   vec3 normal,vec3 dp1,vec3 dp2,vec2 duv1,vec2 duv2,inout vec3 rgb) {
    if(!morph&&!pulse)return 1.0;
    float row=0.0;
    if(!stone&&variant==5u) {
        row=tile.y<42.0?0.0:58.0;
        variant=row==0.0?3u:2u;
    }
    vec4 eyes=stone?marbleEyes[min(variant,2u)]:skullEyes[min(variant,4u)];
    eyes.yw+=row;
    bool right=tile.x>0.5*(eyes.x+eyes.z);
    vec2 center=right?eyes.zw:eyes.xy;
    bool lit=!stone&&(variant==1u||variant==3u||variant==4u);
    bool fixedSocket=!stone&&variant<4u;
    vec2 radius=stone?(variant==2u?vec2(8.3,3.4):vec2(6.4,3.5)):
               vec2(5.7,3.45);
    vec2 p=tile-center;
    float socket=1.0;
    if(fixedSocket) {
        if(!paintedSkullSocket(tile-vec2(0,row),variant<2u,right))return 1.0;
    } else {
        float tilt=stone?(variant==1u?0.18:0.32):0.10;
        p.y-=p.x*tilt*(right?-1.0:1.0);
        if(any(greaterThan(abs(p),radius)))return 1.0;
        float shape=length(p/radius);
        socket=1.0-smoothstep(lit?0.82:0.67,1.0,shape);
        if(socket<=0.0)return 1.0;
    }

    // Invert the texture derivatives to recover world units per texel.
    // Both sockets must use the same world-space center, even at an angle.
    float determinant=duv1.x*duv2.y-duv1.y*duv2.x;
    if(abs(determinant)<1e-8)return 1.0;
    vec3 worldU=(dp1*duv2.y-dp2*duv1.y)/determinant;
    vec3 worldV=(dp2*duv1.x-dp1*duv2.x)/determinant;
    vec2 offset=tile-0.5*(eyes.xy+eyes.zw);
    vec3 origin=vWorld-worldU*offset.x-worldV*offset.y;
    vec3 viewer=fixedSocket&&weaponProbe[0].w>0.0?weaponProbe[0].xyz:c.eye.xyz;
    vec3 toward=viewer-origin;
    vec2 aim=vec2(dot(toward,normalize(worldU)),dot(toward,normalize(worldV)));
    vec2 gaze=morph?clamp(aim/(abs(dot(toward,normal))+13.0),vec2(-0.85),vec2(0.85)):vec2(0);
    ivec3 cell=ivec3(floor(origin*0.125+vec3(0.313,0.423,0.317)));
    float seed=float(tileHash(cell.xy,uint(cell.z))&65535u)/65536.0;
    float close=pulse?engravedBlink(c.fog.w,seed):0.0;
    if(fixedSocket) {
        vec2 at=p-gaze*vec2(1.7,0.75);
        float iris=1.0-smoothstep(0.70,1.22,length(at/vec2(1.48,1.28)));
        float pupil=1.0-smoothstep(0.44,1.04,length(at/vec2(0.58,1.15)));
        float glint=1.0-smoothstep(0.18,0.57,length(at-vec2(-0.48,-0.48)));
        vec3 tint=lit?vec3(0.38,0.075,0.022):vec3(0.22,0.13,0.070);
        vec3 eye=mix(rgb+iris*tint,rgb*0.11,pupil*iris);
        eye+=glint*tint*0.50;
        float exposed=smoothstep(-0.26,0.26,p.y-mix(-1.85,1.85,close));
        float weight=saturate(strength*1.55);
        rgb=mix(rgb,mix(rgb*(lit?0.09:0.72),eye,exposed),weight);
        return lit?mix(1.0,mix(0.05,1.0,exposed),weight):1.0;
    }
    float open=1.0-close;
    float edge=clamp(footprint*0.48,0.16,0.46);
    float along=p.x/(radius.x*0.87);
    float oval=sqrt(max(0.0,1.0-along*along));
    float aperture=engravedAperture(p/radius,open,edge/radius.y);

    vec2 irisAt=(p-gaze*radius*vec2(0.40,0.22))/vec2(radius.y*0.76,radius.y*0.89);
    float ring=length(irisAt);
    float iris=1.0-smoothstep(0.78,1.07,ring);
    float pupil=1.0-smoothstep(0.64,1.1,length(irisAt/vec2(stone?0.39:0.29,0.70)));
    float shine=1.0-smoothstep(0.12,0.36,length(irisAt-vec2(-0.28,-0.32)));
    vec3 irisTint=stone?vec3(0.48,0.49,0.28):lit?vec3(0.80,0.18,0.055):vec3(0.37,0.23,0.105);
    vec3 socketShade=stone?vec3(0.105,0.112,0.069):vec3(0.104,0.059,0.036);
    vec3 globe=mix(socketShade*(0.67+0.33*oval),irisTint*(0.83-0.26*ring),iris);
    globe=mix(globe,vec3(0.026,0.018,0.013),pupil*iris);
    globe+=vec3(0.52,0.44,0.29)*shine*iris*(stone||lit?0.51:0.32);
    vec3 lid=rgb*(stone?0.48:lit?0.12:0.28)+socketShade*(0.39+0.24*oval);
    float weight=socket*saturate(strength*1.55);
    rgb=mix(rgb,mix(lid,globe,aperture),weight);
    // Let the blink dim the baked-in switch indicator through the closed lid.
    return lit?1.0-weight*(0.25+0.75*close):1.0;
}

// The lengthwise contractions travel down different folds at staggered
// speeds. The closed periodic field joins across both texture seams.
vec2 intestineMotion(vec2 part,uint variant,float seed,out float pressure) {
    const float turn=6.2831853;
    vec2 a=part*turn;
    float lane=a.x+0.74*sin(a.y*2.0+float(variant)*0.9)+0.29*sin(a.x+a.y);
    float phase=a.y*2.0+0.82*sin(lane)-c.fog.w*(1.97+float(variant)*0.42)+seed;
    float slow=a.y+lane-c.fog.w*0.73+seed*0.71;
    float swell=0.5+0.5*cos(phase);
    pressure=swell*swell*swell*(0.74+0.26*cos(slow));
    return vec2(sin(lane)*3.6*pressure+0.75*sin(slow),2.45*sin(phase)*swell+1.1*sin(slow));
}
