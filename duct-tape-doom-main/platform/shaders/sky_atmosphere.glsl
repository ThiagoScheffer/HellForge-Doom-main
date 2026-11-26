struct SkyAtmosphereResult {
    vec3 color;
    float cloudCoverage;
    float transmission;
};

vec3 skyAtmosphereBase(vec3 ray) {
    float elevation=smoothstep(-.20-atmosphereWind.z,.97,ray.z);
    return mix(atmosphereHorizon.rgb,atmosphereZenith.rgb,elevation);
}

bool projectNearCloud(vec3 eye,vec3 ray,vec2 drift,out vec2 uv,out float visibility) {
    vec3 relativeEye=eye-depthOrigin.xyz;
    float height=atmosphereProjection.x-relativeEye.z;
    if(ray.z<=.07||height<=0.0)return false;

    float distance=height/ray.z;
    visibility=smoothstep(.07,.32,ray.z)*(1.0-smoothstep(14000.0,31000.0,distance))
               *smoothstep(0.0,360.0,height);
    uv=(relativeEye.xy+distance*ray.xy-drift)/atmosphereProjection.y;
    return visibility>.0001;
}

float atmosphereField(vec2 uv,float time) {
    float broad=textureLod(skyCloudPattern,uv,0.0).r;
    // Bend the smaller field with the broad one to break up repeated
    // contours. Integer longitude coefficients preserve the panorama wrap.
    vec2 detail=vec2(3.0*uv.x-uv.y,uv.x+3.0*uv.y)+vec2(.31,.17)
               +(broad-.5)*vec2(.74,-.37)+time*vec2(.0007,-.00045);
    return mix(broad,textureLod(skyCloudPattern,detail,0.0).r,.35);
}

float atmosphereDensity(float field) {
    float threshold=mix(.61,.39,atmosphereHorizon.w);
    return smoothstep(threshold-.12,threshold+.15,field);
}

vec2 projectFarCloud(float angle,float elevation,vec2 drift) {
    // Four integer repeats in azimuth. Only scale the elevation axis so
    // a profile can tune bank size without breaking the panorama wrap.
    float latitude=(elevation*1.35+elevation*elevation*.23)*atmosphereProjection.z;
    return vec2(-angle/skyTurns*4.0,latitude)+vec2(.17,.36)-drift*.00004;
}

vec3 atmosphereCloudColor(vec2 uv,vec2 towardLight,float field,vec3 stormLight) {
    float neighbor=textureLod(skyCloudPattern,uv+.072*towardLight,0.0).r;
    // The broad-field gradient is a cheap directional shadow cue. Taper
    // highlights into the core to keep the original dark cloud palette.
    float lit=clamp(.52+3.5*(field-neighbor),0.0,1.0);
    vec3 light=mix(atmosphereMiddle.rgb,atmosphereLight.rgb,.16+.84*lit);
    vec3 core=mix(atmosphereDark.rgb,atmosphereMiddle.rgb,.09+.44*lit);
    float shadow=smoothstep(.44,.78,field)*(.93-.44*lit);
    return mix(light,core,shadow)+stormLight*(.38+.62*lit);
}

SkyAtmosphereResult renderSkyAtmosphere(vec3 behind,vec3 ray,float angle,vec3 light,vec3 stormLight) {
    float time=c.fog.w;
    vec2 drift=atmosphereWind.xy*(atmosphereLight.w*time);
    float elevation=ray.z;
    float farVisibility=smoothstep(-.55,-.06-atmosphereWind.z,elevation)*(1.0-smoothstep(.79,1.0,elevation));
    float farAlpha=0.0;
    vec3 farColor=vec3(0.0);
    if(farVisibility>.0001&&atmosphereMiddle.w>0.0) {
        vec2 farUV=projectFarCloud(angle,elevation,drift);
        float field=atmosphereField(farUV,time*.6);
        float density=atmosphereDensity(field);
        farAlpha=density*farVisibility*atmosphereMiddle.w;
        if(farAlpha>.0001) {
            vec2 toward=vec2(dot(light.xy,vec2(sin(angle),-cos(angle))),
                             light.z-elevation*dot(light,ray));
            vec3 cloud=atmosphereCloudColor(farUV,toward,field,stormLight);
            farColor=mix(skyAtmosphereBase(ray),cloud,.47+.26*smoothstep(0.0,.46,elevation));
        }
    }
    vec2 uv;
    float visibility;
    float nearAlpha=0.0;
    vec3 nearColor=vec3(0.0);
    if(atmosphereDark.w>0.0&&projectNearCloud(c.eye.xyz,ray,drift,uv,visibility)) {
        float field=atmosphereField(uv,time);
        float density=atmosphereDensity(field);
        nearAlpha=density*visibility*atmosphereDark.w;
        if(nearAlpha>.0001)nearColor=atmosphereCloudColor(uv,light.xy,field,stormLight);
    }
    float transmission=(1.0-farAlpha)*(1.0-nearAlpha);
    vec3 color=behind*transmission+farColor*farAlpha*(1.0-nearAlpha)+nearColor*nearAlpha;
    return SkyAtmosphereResult(color,1.0-transmission,transmission);
}
