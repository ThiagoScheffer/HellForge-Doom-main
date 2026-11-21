#include "living_surfaces.h"

float skyHeat(vec3 rgb) {
    return smoothstep(0.035,0.25,rgb.r-max(rgb.g,rgb.b));
}

vec3 immersedSky(vec3 rgb,vec2 uv,vec2 size,vec3 ray,bool filtered) {
    uint mode=uint(skyEnvironment.y),flags=uint(c.map.w);
    bool morph=(flags&LS_MORPH)!=0u,pulse=(flags&LS_PULSE)!=0u;
    if(mode==0u||(flags&LS_ENABLED)==0u||(!morph&&!pulse)||c.effects.z>0.0)return rgb;
    float strength=0.24+0.21*float((flags>>LS_INTENSITY_SHIFT)&LS_INTENSITY_MASK);
    float y=uv.y/size.y,t=c.fog.w;
    vec2 p=vec2(uv.x/size.x,y);
    float value=dot(rgb,vec3(0.299,0.587,0.114));
    float red=skyHeat(rgb);
    float backdrop=mode==1u?1.0-smoothstep(0.64,0.92,y):
        mode==2u?smoothstep(0.29,0.64,value):red;
    // Preserve the printed skyline/ridgeline and move the painted sky on a
    // much farther apparent plane. Both animated cloud signals wrap in yaw.
    if(morph&&backdrop>0.0) {
        vec2 drift=mode==1u?vec2(t*1.6,0.7*sin(p.x*12.56637+t*0.54)):
            mode==2u?vec2(t*0.9,2.2*sin(t*0.19+p.x*6.283185)):
            vec2(t*0.64,2.1*sin(p.x*12.56637-t*0.91));
        drift+=c.eye.xy/4800.0;
        vec2 at=vec2(uv.x+drift.x,clamp(uv.y+drift.y,0.5,size.y-0.5));
        vec3 moved=indexed(image,palette,at,filtered,true).rgb;
        if(mode==3u)backdrop*=skyHeat(moved);
        if(mode==2u)backdrop*=smoothstep(0.28,0.58,dot(moved,vec3(0.299,0.587,0.114)));
        rgb=mix(rgb,moved,strength*backdrop*(mode==2u?0.43:0.64));
        if(mode!=2u) {
            float nearer=texture(skyCloudPattern,p*vec2(4,2.2)-vec2(t*0.020,t*0.007)+ray.xy*0.35).r;
            float farther=texture(skyCloudPattern,p*vec2(2,1.4)-vec2(t*0.008,t*0.014)).r;
            float smoke=smoothstep(0.44,0.72,nearer*0.70+farther*0.30);
            vec3 veil=mode==1u?vec3(0.37,0.22,0.105):vec3(0.31,0.025,0.035);
            rgb=mix(rgb,veil,smoke*strength*backdrop*0.33);
        }
    }
    if(pulse&&mode>=2u) {
        float flame=0.5+0.5*sin(p.x*113.097+sin(p.x*43.982-t*2.1)*1.8-y*16.0+t*5.4);
        if(mode==2u) {
            float skyline=smoothstep(0.38,0.76,y)*(red*0.62+smoothstep(0.29,0.68,value)*0.20);
            rgb+=vec3(0.52,0.20,0.045)*flame*skyline*strength;
        } else {
            rgb+=vec3(0.31,0.047,0.022)*flame*red*strength;
            float ridge=smoothstep(0.37,0.79,y)*(1.0-red)*strength;
            rgb=mix(rgb,rgb*vec3(0.87,0.98,1.20)+vec3(0.017,0.035,0.062),ridge*0.65);
        }
    }
    return rgb;
}
