// Curated subjects that share the artwork sampler but have distinct motion.
void livingEye(vec2 p,uint state,float clock,bool morph,bool pulse,float strength,inout vec3 rgb) {
    // Coordinates within the 64x50 switch patch, masked inside its flesh rim.
    if(abs(p.x)>0.20||abs(p.y)>0.14)return;
    float cycle=fract(clock*0.26);
    float blink=smoothstep(0.035,0.11,abs(cycle-0.5));
    float open=state==1u?mix(1.0,blink,float(pulse)):0.66*(1.0-blink)*float(pulse);
    if(state==0u&&open<0.03)return;
    vec2 gaze=morph?vec2(sin(clock*1.7)*0.044,sin(clock*1.1)*0.013):vec2(0);
    float socket=featureMask(p,vec2(0.175,0.088));
    float aperture=featureMask(p,vec2(0.155,0.018+open*0.054));
    vec2 irisAt=(p-gaze)/vec2(0.061,0.061);
    float iris=(1.0-smoothstep(0.72,1.04,length(irisAt)))*aperture;
    float pupil=featureMask(irisAt,vec2(0.47,0.66));
    float glint=featureMask(irisAt-vec2(-0.25,-0.32),vec2(0.24));
    vec3 lid=mix(rgb,vec3(0.35,0.145,0.13),socket*(1.0-aperture));
    vec3 eye=mix(vec3(0.092,0.027,0.024),vec3(0.14,0.36,0.69),iris*(1.0-pupil));
    eye+=vec3(0.30,0.38,0.44)*glint*iris;
    rgb=mix(rgb,mix(lid,eye,aperture),socket*saturate(strength));
}

LivingShading livingHazard(uint profile,uint variant,uint flags,vec2 uv,vec2 tile,vec2 extent,
                          vec2 pixel,float lod,float seed,float strength,inout vec3 rgb) {
    LivingShading result=LivingShading(vec2(0),0.0,0.0,0.0,1.0);
    bool pulse=(flags&LS_PULSE)!=0u,wet=(flags&LS_WETNESS)!=0u;
    bool morph=(flags&LS_MORPH)!=0u&&c.style.x==0.0;
    if(!morph&&!pulse&&(!wet||(profile!=LS_PINNED_BODY&&profile!=LS_BLOOD_FLOW)))return result;
    vec2 part=tile/extent;
    if(profile==LS_EYE_SWITCH) {
        livingEye(part-vec2(0.50,0.53),variant,c.fog.w+seed,morph,pulse,strength,rgb);
        return result;
    }

    float t=c.fog.w,red=livingRed(rgb);
    float mask=1.0,flush=0.0;
    vec2 shift=vec2(0),slope=vec2(0);
    if(profile==LS_PINNED_BODY) {
        bool nailed=variant<2u;
        float torso=variant==1u?0.43:variant==5u?0.63:0.50;
        vec2 body=part-vec2(0.5,torso);
        // Leave chains and the ends of outstretched arms anchored. Dark and
        // warm painted material moves inside the torso's silhouette.
        mask=featureMask(body,vec2(variant==2u?0.25:0.19,nailed?0.28:0.34));
        float stone=nailed?0.0:smoothstep(0.0,0.055,rgb.g-rgb.r);
        mask*=1.0-stone*0.92;
        float phase=t*(nailed?2.35:5.2)+seed+float(variant)*1.37;
        float tense=nailed?sin(phase):pow(0.5+0.5*sin(phase*0.43),3.0)*sin(phase);
        shift=vec2((part.x-0.5)*(-19.0*tense),1.55*cos(phase))*mask;
        flush=0.15*tense*mask;
        slope=vec2(tense,cos(phase))*mask*0.095;
    } else if(profile==LS_TELEPORT) {
        vec2 p=part-0.5;
        float radius=length(p);
        mask=1.0-smoothstep(0.34,0.44,radius);
        float wave=sin(radius*48.0-t*4.5+sin(p.x*12.0+p.y*10.0));
        shift=(vec2(-p.y,p.x)*sin(t*1.5+seed)*6.0+p*wave*3.7)*mask;
        flush=(0.13+0.17*wave)*mask;
        if(pulse)rgb+=mix(vec3(0.095,0.015,0.013),vec3(0.055,0.065,0.12),float(variant==1u))*mask*strength;
    } else if(profile==LS_BLOOD_FLOW) {
        mask=red;
        float wave=sin(part.y*37.699-t*5.3+sin(part.x*18.85)*1.6+seed);
        shift=vec2(0.45*sin(part.y*12.566-t*2.1),-2.6+wave*2.0)*mask;
        flush=(0.035+0.26*wave)*mask;
        slope=vec2(0.075*wave,0.044)*mask;
    } else { // Animated fire and blue plasma wall families.
        float plume=sin(part.x*25.133+sin(part.y*12.566+t*2.5)*0.89-t*2.3+seed);
        float hot=sin(part.y*37.699+t*6.0+plume*1.7);
        float blue=smoothstep(0.02,0.24,rgb.b-rgb.r);
        mask=variant==1u?max(red,blue):max(red,smoothstep(0.12,0.48,rgb.r)*0.72);
        shift=vec2(plume*2.0,(variant==1u?-1.25:1.25)+3.0*hot)*mask;
        flush=(0.14+0.23*hot)*mask;
        slope=vec2(plume,hot)*mask*0.16;
        if(pulse)rgb+=mix(vec3(0.115,0.026,0.004),vec3(0.015,0.044,0.17),blue)*
                            max(hot,0.0)*mask*strength;
    }

    if(morph&&any(notEqual(shift,vec2(0)))) {
        vec2 at=uv+shift*strength;
        vec4 moved=surfaceTexture(image,at,pixel,lod);
        if(blend>0.0)moved=mix(moved,surfaceTexture(nextImage,at,pixel,lod),blend);
        float weight=smoothstep(0.98,1.0,moved.a)*saturate(strength);
        if(profile==LS_BLOOD_FLOW)weight*=red*livingRed(moved.rgb);
        rgb=mix(rgb,moved.rgb,weight);
    }
    if(pulse) {
        rgb*=1.0+strength*flush;
        result.slope=slope*strength;
    }
    if(wet&&(profile==LS_PINNED_BODY||profile==LS_BLOOD_FLOW)&&red>0.0) {
        float seep=livingRunoff(tile,extent,seed,strength,red,rgb);
        result.gloss=0.23+0.41*seep;
        result.sharpness=34.0;
        result.finish=saturate(strength)*red;
    }
    return result;
}
