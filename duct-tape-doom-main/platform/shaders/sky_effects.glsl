// Source offsets use the same azimuth and tangent-of-elevation space as
// scenery. Keep the signed angular distance continuous at the panorama seam.
float skyEffectAngle(float angle) {
    return angle-skyTurns*floor(angle/skyTurns+.5);
}

float skyEffectNoise(vec2 uv,float phase) {
    return textureLod(skyCloudPattern,uv+vec2(phase,phase*.37),0.0).r;
}

float skySmokeDensity(SkyFireSource source,vec2 local,float time,float wind) {
    if(source.smoke.z<=0.0)return 0.0;
    float h=local.y/source.smoke.y;
    if(h<=0.0||h>=1.0)return 0.0;

    float phase=source.motion.z, turbulence=source.motion.y;
    float x=local.x/source.smoke.x;
    // Wind sweeps the top farther than the base. The slower lateral roll
    // changes the silhouette as new billows emerge.
    float lean=wind*source.motion.x*h*(.14+.60*h);
    float roll=sin(h*8.4-time*.55+phase*skyTurns)*turbulence*h*.45;
    float width=mix(.36,.96,smoothstep(0.0,.79,h));
    float across=(x-lean-roll)/width;
    if(abs(across)>1.32)return 0.0;

    float rising=h*.95-time*source.smoke.w*.089;
    float broad=skyEffectNoise(vec2(x*.47+rising*.35,rising-x*.27),phase);
    vec2 rollField=vec2(x*.91+(broad-.5)*.38+rising*.61,rising*2.31-x*.56);
    float detail=skyEffectNoise(rollField,phase+.31);
    float mass=mix(broad,detail,.28);
    float edge=abs(across)+(mass-.56)*(.86+turbulence);
    float envelope=1.0-smoothstep(.37,1.04,edge);
    float billows=smoothstep(.29+smoothstep(.46,.98,h)*.11,.65,mass);
    float vertical=smoothstep(0.0,.095,h)*(1.0-smoothstep(.63,1.0,h));
    return clamp((.40+.72*billows)*envelope*vertical*source.smoke.z,0.0,1.0);
}

SkyLayerSample skyFlame(SkyFireSource source,vec2 local,float time) {
    SkyLayerSample flame=SkyLayerSample(vec3(0.0),0.0);
    if(source.flame.z<=0.0)return flame;
    vec2 q=local/source.flame.xy;
    if(abs(q.x)>1.0||q.y<-.07||q.y>1.0)return flame;

    float phase=source.motion.z;
    float flickerTime=time*source.flame.w;
    float n=skyEffectNoise(vec2(q.x*.93,q.y*.75-flickerTime*.33),phase);
    float h=max(q.y,0.0);
    float sway=(n-.5)*.38*h;
    float width=mix(.88,.09,h);
    float edge=abs(q.x-sway)/width;
    // Uneven tongues grow out of the base at different speeds. The
    // changing tip height is driven by the rising field across each tongue.
    float height=.27+.64*smoothstep(.22,.77,n);
    float mask=(1.0-smoothstep(height-.24,height+.09,h))
              *(1.0-smoothstep(.43,1.0,edge))*smoothstep(-.07,.05,q.y);
    float flicker=.94+.06*sin(flickerTime*4.5+phase*skyTurns);
    float core=(1.0-smoothstep(.05,.65,h))*(1.0-smoothstep(.12,.75,edge))*(.64+.36*n);
    flame.color=mix(fireFlameOuter.rgb,fireFlameCore.rgb,core);
    flame.color*=1.0-.39*smoothstep(.40,1.0,h);
    flame.alpha=clamp(mask*flicker*source.flame.z,0.0,1.0);
    return flame;
}

struct SkyFireLight {
    float halo,scatter;
};

SkyFireLight skyFireIllumination(SkyFireSource source,vec2 local,float density,float time) {
    SkyFireLight light=SkyFireLight(0.0,0.0);
    if(source.illumination.y<=0.0)return light;
    vec2 q=local/source.illumination.x;
    if(abs(q.x)>1.12||q.y<-.23||(q.y>=2.0&&density==0.0))return light;
    // The field is broader sideways and rises through the smoke. The
    // negative side falls off rapidly beneath the fire's origin.
    float h=max(q.y,0.0);
    float lateral=1.0-smoothstep(.09,1.0,abs(q.x-.12*min(h,1.0)));
    float above=smoothstep(-.23,0.0,q.y);
    float falloff=exp2(-h*source.illumination.z*.32);
    float flicker=.95+.05*sin(time*source.flame.w*4.5+source.motion.z*skyTurns);
    float power=source.illumination.y*flicker*above*lateral*falloff;
    light.halo=clamp(power*.34*(1.0-smoothstep(.40,2.0,h)),0.0,.45);
    float edge=4.0*density*(1.0-density);
    float transmission=exp2(-density*3.45);
    light.scatter=clamp(power*transmission*(.32+2.90*edge),0.0,1.0);
    return light;
}

struct SkyEffects {
    vec4 layers[4]; // premultiplied color and coverage, back to front
#if defined(SKY_EFFECTS_DEBUG) && SKY_EFFECTS_DEBUG < 6
    vec3 debug;
#endif
};

SkyEffects sampleSkyEffects(float angle,float rise,vec2 axis,vec3 opacity,vec3 haze,
                            float cloudCoverage,float cloudTransmission) {
    SkyEffects effects;
    for(int i=0;i<4;++i)effects.layers[i]=vec4(0.0);
#if defined(SKY_EFFECTS_DEBUG) && SKY_EFFECTS_DEBUG < 6
    effects.debug=vec3(0.0);
#endif
    if(fireHeader.x==0.0)return effects;
    vec2 across=vec2(-axis.y,axis.x);
    float nearest=fireHeader.y;
    if(depthShells.x>0.0)nearest=min(nearest,min(depthShells.x,depthShells.y));
    vec3 delta=skyDepthEye(c.eye.xyz,depthOrigin.xyz,nearest);
    vec3 eye=vec3(dot(delta.xy,axis),dot(delta.xy,across),delta.z);
    float time=c.fog.w;
    for(int i=0;i<int(fireHeader.x);++i) {
        SkyFireSource source=fireSources[i];
        int slot=int(source.anchor.w);
#if !defined(SKY_EFFECTS_DEBUG) || SKY_EFFECTS_DEBUG == 6
        if((slot<3&&opacity.z>=1.0)||(slot<2&&opacity.y>=1.0)||(slot==0&&opacity.x>=1.0))continue;
#endif
        float angleOffset=skyEffectAngle(angle-source.anchor.x);
        if(abs(angleOffset)>.095+source.motion.w)continue;
        vec2 shell=skyShellDirection(angleOffset,rise,eye,source.anchor.z);
        vec2 local=shell-vec2(0.0,source.anchor.y);
#if !defined(SKY_EFFECTS_DEBUG) || SKY_EFFECTS_DEBUG == 6
        if(local.y<min(-.07*source.flame.y,-.23*source.illumination.x)||
           local.y>source.illumination.w)continue;
#endif
        float wind=dot(atmosphereWind.xy,vec2(-sin(source.anchor.x),cos(source.anchor.x)));
        wind*=fireHeader.z/63.0;
        float density=skySmokeDensity(source,local,time,wind);
        float h=clamp(local.y/max(source.smoke.y,.001),0.0,1.0);
        float upperBlend=smoothstep(.38,1.0,h)*fireHeader.w;
        vec3 smoke=mix(fireSmokeDark.rgb,haze,upperBlend*(.55+.45*cloudCoverage));
        float smokeAlpha=density*(1.0-upperBlend*(.28+.25*(1.0-cloudTransmission)));
        SkyLayerSample flame=skyFlame(source,local,time);
        SkyFireLight light=skyFireIllumination(source,local,density,time);
        smoke=mix(smoke,fireSmokeWarm.rgb,light.scatter);
        vec4 glow=vec4(fireSmokeWarm.rgb*light.halo,light.halo);
        vec4 sampleLayer=vec4(smoke*smokeAlpha,smokeAlpha)+glow*(1.0-smokeAlpha);
        sampleLayer=vec4(flame.color*flame.alpha,flame.alpha)+sampleLayer*(1.0-flame.alpha);
        effects.layers[slot]=sampleLayer+effects.layers[slot]*(1.0-sampleLayer.a);
#if defined(SKY_EFFECTS_DEBUG)
    #if SKY_EFFECTS_DEBUG == 1
        vec2 marker=abs(local);
        if((marker.x<.003&&marker.y<.029)||(marker.x<.020&&marker.y<.004))
            effects.debug=max(effects.debug,i==0?vec3(1.0,.45,.10):vec3(.30,.90,1.0));
    #elif SKY_EFFECTS_DEBUG == 2
        effects.debug=max(effects.debug,vec3(flame.alpha));
    #elif SKY_EFFECTS_DEBUG == 3
        effects.debug=max(effects.debug,vec3(density));
    #elif SKY_EFFECTS_DEBUG == 4
        effects.debug=max(effects.debug,vec3(light.scatter,light.halo,0.0));
    #elif SKY_EFFECTS_DEBUG == 5
        const vec3 slotColor[4]=vec3[4](vec3(.40,.50,1.0),vec3(1.0,.41,.10),
                                        vec3(.20,1.0,.50),vec3(1.0,.35,1.0));
        effects.debug=max(effects.debug,sampleLayer.a*slotColor[slot]);
    #endif
#endif
    }
    return effects;
}

vec3 skyEffectOver(vec3 background,vec4 effect) {
    return effect.rgb+background*(1.0-effect.a);
}
