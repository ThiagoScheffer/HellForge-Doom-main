struct LivingShading { vec2 slope; float gloss,sharpness,finish; };
#include "living_motion.glsl"

LivingShading livingSurface(uint profile,uint flags,vec2 uv,vec2 pixel,float lod,float footprint,
                            vec3 normal,vec3 dp1,vec3 dp2,vec2 duv1,vec2 duv2,inout vec3 rgb) {
    LivingShading result=LivingShading(vec2(0),0.0,0.0,0.0);
    if(profile==LS_NONE||(flags&(LS_MORPH|LS_PULSE|LS_WETNESS))==0u)return result;
    float strength=(1.0-smoothstep(200.0,680.0,length(vWorld-c.eye.xyz)))*
                   (1.0-smoothstep(0.75,1.8,footprint));
    if(strength<=0.0)return result;

    vec2 size=vec2(textureSize(image,0));
    vec4 bounds=vec4(frameMean.rgb,cycleMean.r);
    vec2 at=mod(uv,size);
    vec2 extent=bounds.zw-bounds.xy;
    vec2 room=mix(min(at-bounds.xy,bounds.zw-at),vec2(9.0),equal(extent,size));
    strength*=smoothstep(4.5,8.0,min(room.x,room.y));
    if(strength<=0.0)return result;
    strength*=0.15+0.40*float((flags>>LS_INTENSITY_SHIFT)&LS_INTENSITY_MASK);

    vec2 tile=at-bounds.xy;
    float seed=dot(vWorld,vec3(0.007,0.009,0.006));
    bool pulse=(flags&LS_PULSE)!=0u,wet=(flags&LS_WETNESS)!=0u;
    bool morph=(flags&LS_MORPH)!=0u&&c.style.x==0.0&&blend==0.0;
    uint variant=uint(cycleMean.g);
    vec2 shift=vec2(0);
    float flush=0.0,red=livingRed(rgb),wetMask=0.0,gloss=0.0;

    if(profile==LS_DRY_SKIN) {
        if(morph||pulse) {
            float pressure;
            shift=leatherPressure(tile,extent,seed,pressure);
            if(pulse) {
                flush=pressure;
                result.slope=shift*(0.035*strength);
            }
        }
        if(variant==1u) {
            wetMask=red*smoothstep(12.0,19.0,min(tile.y,extent.y-tile.y));
            gloss=0.24*wetMask;
        }
    } else if(profile==LS_TORTURED_FACES) {
        if(morph)shift=trappedFaceMotion(tile/extent*vec2(128,96),seed)*extent/vec2(128,96);
    } else if(profile==LS_MARBLE_FACES) {
        if(morph) {
            vec3 tangent,bitangent;
            textureFrame(normal,dp1,dp2,duv1,duv2,tangent,bitangent);
            float gaze=dot(normalize(c.eye.xyz-vWorld),tangent);
            shift=marbleEyeMotion(tile/extent*128.0,variant,gaze)*extent/128.0;
        }
    } else {
        bool spine=profile==LS_SPINE_WALL;
        bool flesh=profile==LS_HELL_FLESH;
        bool tentacles=profile==LS_TENTACLE_MEAT;
        if(pulse||morph) {
            vec2 part=tile/extent;
            float phase=part.x*12.56637+part.y*6.283185-c.fog.w*(spine?2.65:2.12)+seed;
            float ripple=sin(phase),lift=sin(phase-1.2);
            float tension=0.72+0.28*sin(part.x*6.283185+c.fog.w*0.86+seed);
            vec2 scale=spine?vec2(3.1,1.65):flesh?vec2(2.6,1.9):tentacles?vec2(1.6,1.3):vec2(0.50,0.75)*red;
            shift=vec2(ripple*tension,lift)*scale;
            if(pulse) {
                float tissue=tentacles?max(red,0.44):red;
                flush=(0.32*smoothstep(-0.5,0.9,lift)*tension-0.09)*tissue;
                result.slope=vec2(ripple,lift)*(0.13*strength*mix(0.32,1.0,tissue));
            }
        }
        wetMask=red;
        gloss=tentacles?0.17+0.18*red:0.06+0.23*red;
    }

    if(morph&&any(notEqual(shift,vec2(0)))) {
        vec2 sampleAt=uv+shift*strength;
        vec4 moved=surfaceTexture(image,sampleAt,pixel,lod);
        // Preserve the original sample's alpha and depth in both passes.
        rgb=mix(rgb,moved.rgb,saturate(strength)*smoothstep(0.98,1.0,moved.a));
    }
    if(pulse) {
        rgb*=1.0+strength*flush;
        if(profile==LS_HELL_FLESH||profile==LS_SPINE_WALL)rgb.r+=max(flush,0.0)*strength*0.28;
    }
    if(wet&&gloss>0.0) {
        float runoff=0.0;
        if(wetMask>0.0)runoff=livingRunoff(tile,extent,seed,strength,wetMask,rgb);
        result.gloss=gloss+0.34*runoff;
        result.sharpness=36.0;
        result.finish=saturate(strength);
        // In a torn leather panel the wet finish stays confined to the cut.
        if(profile==LS_DRY_SKIN)result.finish*=wetMask;
    }
    return result;
}
