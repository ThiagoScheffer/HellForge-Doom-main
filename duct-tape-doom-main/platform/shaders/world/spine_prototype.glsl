// Temporary -spineprototype preview. Offsets are expressed in WAD texels.
LivingShading spinePrototype(uint flags,vec2 uv,vec2 tile,vec2 size,vec2 pixel,float lod,
                            float strength,float cavity,LivingShading result,inout vec3 rgb) {
    float time=c.fog.w;
    vec2 part=tile/size;
    float offset=dot(vWorld,vec3(0.007,0.009,0.006));
    // A pressure wave travels along the ribs. The slower term changes how
    // far a local group contracts before it relaxes.
    float phase=part.x*12.56637+part.y*6.283185-time*2.65+offset;
    float ripple=sin(phase);
    float tension=0.72+0.28*sin(part.x*6.283185+time*0.86+offset);
    float lift=sin(phase-1.2);
    if((flags&LS_MORPH)!=0u&&c.effects.x>0.0&&c.style.x==0.0&&blend==0.0) {
        // The smooth displacement field carries each rib and its adjoining
        // tissue together. It also keeps repeated tile boundaries continuous.
        vec2 shift=vec2(3.1*ripple*tension,1.65*lift)*strength;
        vec4 moved=surfaceTexture(image,uv+shift,pixel,lod);
        rgb=mix(rgb,moved.rgb*(1.0-0.45*cavity*strength),saturate(strength)*smoothstep(0.98,1.0,moved.a));
    }
    // Maroon tissue has much less green than exposed bone. Keep the flush
    // on the tissue and allow runoff to touch the dark gaps beside ribs.
    float red=smoothstep(0.035,0.24,rgb.r-rgb.g*1.55)*smoothstep(0.035,0.14,rgb.r);
    if((flags&LS_PULSE)!=0u) {
        float flush=smoothstep(-0.5,0.9,lift)*tension;
        rgb*=1.0+strength*red*(0.32*flush-0.09);
        rgb.r+=0.09*flush*red*strength;
        result.slope+=vec2(ripple,lift)*(0.13*strength*mix(0.32,1.0,red));
    }
    if((flags&LS_WETNESS)!=0u) {
        // Narrow trails follow gravity. Offset each head to keep the seepage
        // continuous and uneven across the wall. One texel-sized highlight
        // describes the wet lip without lighting the whole stretch of flesh.
        float lanes=part.x*max(1.0,round(size.x/16.0))+0.14*sin(part.y*6.283185+offset);
        float lane=floor(lanes);
        float rivulet=abs(fract(lanes)-0.5);
        float age=fract(part.y-time*0.23+0.35*sin(lane*2.51+offset));
        float trail=(1.0-smoothstep(0.21,0.39,age))*smoothstep(0.01,0.07,age);
        float width=0.07+0.09*trail;
        float seep=trail*(1.0-smoothstep(width,width+0.06,rivulet))*max(red,cavity*0.7)*strength;
        float lip=(1.0-smoothstep(0.025,0.07,abs(age-0.25)))*seep;
        rgb=mix(rgb,rgb*vec3(0.60,0.20,0.23)+vec3(0.09,0.004,0.007),saturate(seep*0.85));
        rgb+=vec3(0.20,0.043,0.053)*lip;
        result.gloss=0.06+0.23*red+0.34*seep;
        result.sharpness=36.0;
        result.finish=saturate(strength);
    }
    return result;
}
