// Parameters for the curated image profiles. Motion amplitudes are in texels.
struct LivingProfile {
    float speed, morph, pulse, groove, relief, gloss, sharpness;
};
const LivingProfile livingProfiles[6]=LivingProfile[6](
    LivingProfile(0.95,0.32,0.11,0.40,0.26,0.21,30.0), // Fused flesh
    LivingProfile(0.77,0.20,0.09,0.45,0.36,0.13,25.0), // Spine
    LivingProfile(0.84,0.25,0.10,0.50,0.31,0.17,27.0), // Bodies
    LivingProfile(1.20,0.46,0.15,0.44,0.28,0.26,36.0), // Tentacle flat
    LivingProfile(0.49,0.00,0.04,0.33,0.21,0.02,13.0), // Dry skin
    LivingProfile(0.61,0.00,0.06,0.52,0.34,0.07,20.0)  // Faces
);

struct LivingShading { vec2 slope; float gloss,sharpness,finish; };
float livingLuma(vec3 rgb) { return dot(rgb,vec3(0.299,0.587,0.114)); }
#include "spine_prototype.glsl"

LivingShading livingSurface(uint profile,uint flags,vec2 uv,vec2 pixel,float lod,float footprint,inout vec3 rgb) {
    LivingShading result=LivingShading(vec2(0),0.0,0.0,0.0);
    if(profile==LS_NONE||profile>LS_TORTURED_FACES||(flags&(LS_MORPH|LS_PULSE|LS_WETNESS|LS_CAVITIES))==0u)
        return result;
    float strength=(1.0-smoothstep(200.0,680.0,length(vWorld-c.eye.xyz)))*
                   (1.0-smoothstep(0.75,1.8,footprint));
    if(strength<=0.0) return result;
    strength*=0.15+0.40*float((flags>>LS_INTENSITY_SHIFT)&LS_INTENSITY_MASK);

    LivingProfile p=livingProfiles[profile-1u];
    ivec2 size=textureSize(relief,0);
    vec4 bounds=vec4(frameMean.rgb,cycleMean.r);
    vec2 at=mod(uv,vec2(size));
    // Leave room for the neighborhood/filter footprint before a sibling
    // patch begins. Across full-width/height axes the sampler wraps as usual.
    vec2 room=mix(min(at-bounds.xy,bounds.zw-at),vec2(8.0),
                  equal(bounds.zw-bounds.xy,vec2(size)));
    bool spinePreview=profile==LS_SPINE_WALL&&(flags&LS_SPINE_PROTOTYPE)!=0u;
    strength*=spinePreview?smoothstep(4.5,8.0,min(room.x,room.y)):smoothstep(2.25,5.0,min(room.x,room.y));
    if(strength<=0.0) return result;
    vec3 cell=texelFetch(relief,wrapTexel(ivec2(floor(uv)),size),0).rgb;
    vec2 slope=(cell.rg*255.0-128.0)/127.0;
    float contour=max(length(slope),cell.b*0.55);
    float soft=1.0-smoothstep(0.12,0.50,contour);
    float value=livingLuma(rgb);
    float red=smoothstep(0.045,0.25,rgb.r-max(rgb.g,rgb.b));
    float middle=smoothstep(0.04,0.20,value)*(1.0-smoothstep(0.48,0.75,value));
    float tissue=soft*middle*mix(red,1.0,profile==LS_TENTACLE_MEAT?0.52:0.08);
    if(profile==LS_DRY_SKIN||profile==LS_TORTURED_FACES) tissue=soft*middle*0.50;

    float cavity=0.0;
    if((flags&LS_CAVITIES)!=0u) {
        vec4 a=surfaceTexture(image,uv-vec2(1.5),pixel,lod);
        vec4 b=surfaceTexture(image,uv+vec2(1.5),pixel,lod);
        float localMean=0.5*(mix(value,livingLuma(a.rgb),a.a)+mix(value,livingLuma(b.rgb),b.a));
        cavity=smoothstep(0.018,0.18,localMean-value)*smoothstep(0.04,0.28,contour);
        rgb*=1.0-p.groove*cavity*strength;
        result.slope=slope*(p.relief*strength);
    }
    if(spinePreview)return spinePrototype(flags,uv,at,vec2(size),pixel,lod,strength,cavity,result,rgb);

    bool morph=(flags&LS_MORPH)!=0u&&p.morph>0.0&&c.effects.x>0.0&&c.style.x==0.0&&blend==0.0;
    if((morph||(flags&LS_PULSE)!=0u)&&tissue>0.0) {
        // World position offsets nearby instances. Integer UV frequencies
        // keep the directional tentacle wave continuous at tile boundaries.
        vec2 phase=vWorld.xy*vec2(0.024,0.031)+vWorld.z*vec2(0.018,-0.027);
        phase+=c.fog.w*p.speed*vec2(1.0,-0.71);
        if(profile==LS_TENTACLE_MEAT) phase+=uv/vec2(size)*vec2(6.283185,12.566371);
        vec2 wave=sin(phase+0.45*sin(phase.yx*vec2(1.0,-1.0)));
        if(morph) {
            vec2 shift=vec2(wave.x,wave.y*(profile==LS_SPINE_WALL?0.40:0.72))*p.morph*strength;
            vec4 moved=surfaceTexture(image,uv+shift,pixel,lod);
            // RGB only, with a coverage guard at any masked edge. The
            // original sample still controls discard and depth in both passes.
            rgb=mix(rgb,moved.rgb*(1.0-p.groove*cavity*strength),tissue*smoothstep(0.98,1.0,moved.a));
        }
        if((flags&LS_PULSE)!=0u) {
            float breath=dot(wave,vec2(0.63,0.37));
            rgb*=1.0+p.pulse*breath*tissue*strength;
            if(profile==LS_HELL_FLESH||profile==LS_TORTURED_FACES)
                rgb.r+=p.pulse*max(breath,0.0)*tissue*strength*0.15;
            result.slope+=wave*(p.pulse*0.5*tissue*strength);
        }
    }
    if((flags&LS_WETNESS)!=0u) {
        result.gloss=p.gloss*mix(0.18,1.0,tissue)*(1.0-0.7*cavity);
        result.sharpness=p.sharpness;
        result.finish=min(strength,1.0);
    }
    return result;
}
