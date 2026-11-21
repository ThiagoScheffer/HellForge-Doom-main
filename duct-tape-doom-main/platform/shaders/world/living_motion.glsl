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

const vec4 marbleEyes[3]=vec4[3](vec4(40,57,85,57),vec4(51,78,78,78),vec4(46,66,80,66));

vec2 marbleEyeMotion(vec2 tile,uint style,float gaze) {
    vec4 eyes=marbleEyes[min(style,2u)];
    float eye=max(featureMask(tile-eyes.xy,vec2(10,7)),featureMask(tile-eyes.zw,vec2(10,7)));
    return vec2(-2.65*gaze,0.28*sin(c.fog.w*1.1))*eye;
}
