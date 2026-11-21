// DOOM: HELLFORGE / Milestone C - three visually separated cloud scales.
// Not full volumetric raymarching. Soft pseudo-volume comes from offset density
// samples in two planes and a sparse third puff scale. Clouds are cosmetic and
// inherit sky colors; there is no new sun lighting or lighting-cache change.

float hellforgeCloudPattern(vec2 uv) {
    float broad=texture(skyCloudPattern,uv).r;
    float detail=texture(skyCloudPattern,uv*2.37+vec2(0.37,0.19)).r;
    float wisps=texture(skyCloudPattern,uv*4.71+vec2(0.61,0.43)).r;
    return broad*0.59+detail*0.29+wisps*0.12;
}

vec3 burningCitySmoke(vec3 ray,float angle,vec3 sky,out float coverage) {
    // Wind stretches the smoke into soft layers that thin above the roofs.
    // Integer yaw repeats keep both layers continuous around the horizon.
    float z=ray.z,t=c.fog.w,u=angle/6.2831853;
    float bank=texture(skyCloudPattern,vec2(u*3.0-t*0.013,z*3.8-t*0.024)).r;
    float drift=texture(skyCloudPattern,vec2(u*5.0-t*0.021,z*7.0-t*0.036)+vec2(0.27,0.41)).r;
    float smoke=smoothstep(0.29,0.78,mix(bank,drift,0.32));
    float aloft=smoothstep(-0.06,0.14,z)*(1.0-smoothstep(0.21,0.58,z));
    coverage=0.11*smoke*aloft;
    float haze=0.075*(1.0-smoothstep(-0.06,0.26,z));
    return mix(sky,vec3(0.33,0.28,0.25),haze+coverage);
}

vec3 hellforgeCloudDecks(vec3 ray,vec3 skyColor,vec3 horizonColor,
                          int profile,out float cloudCover) {
    cloudCover=0.0;
    if(ray.z<=0.025) return skyColor;
    float slopeZ=max(ray.z,0.16);
    vec2 slope=ray.xy/slopeZ;

    // Wind strength fluctuates with the weather tics, but cloud displacement
    // must not be current-speed * total-time (which produced A's reversal).
    // The per-level heading is constant, with monotonic linear advection.
    vec2 heading=c.weather.yz;
    float len=length(heading);
    heading=len>0.001?heading/len:normalize(vec2(68.0,22.0));
    vec2 traveled=heading*(110.0*max(c.fog.w,0.0));

    vec2 upperUV=slope*0.31+(c.eye.xy-traveled*0.25)/14000.0;
    vec2 lowerUV=slope*0.46+(c.eye.xy-traveled*0.90)/5700.0+vec2(0.19,0.63);
    vec2 puffUV=slope*0.93+(c.eye.xy-traveled*0.62)/9200.0+vec2(0.61,0.09);

    float highField=hellforgeCloudPattern(upperUV);
    float lowField=hellforgeCloudPattern(lowerUV);
    // One additional sample adds an apparent soft side to the high cloud bank
    // without lighting calculations or a costly 3D raymarch.
    float highSide=texture(skyCloudPattern,upperUV+vec2(0.019,-0.016)).r;
    float puff=texture(skyCloudPattern,puffUV).r;

    float high=smoothstep(0.455,0.680,highField)*
               smoothstep(0.025,0.21,ray.z)*0.59;
    float low=smoothstep(0.490,0.690,lowField)*
              smoothstep(0.085,0.31,ray.z)*0.74;
    float small=smoothstep(0.665,0.78,puff)*
                smoothstep(0.070,0.28,ray.z)*0.34;

    // Color is derived from the currently visible WAD sky and sampled
    // horizon. This naturally retains blue, brown or infernal red palettes.
    vec3 highTint=mix(skyColor,horizonColor,0.25)*1.12;
    vec3 shadedTint=mix(skyColor,horizonColor,0.44)*0.65;
    if(profile==4) {
        // Hell clouds look like smoke; no independent red emissive source.
        highTint*=vec3(1.06,0.94,0.91);
        shadedTint*=vec3(1.06,0.86,0.83);
    } else if(profile==3) {
        highTint=mix(highTint,vec3(dot(highTint,vec3(0.299,0.587,0.114))),0.22);
    }
    highTint=clamp(highTint,vec3(0),vec3(1));
    shadedTint=clamp(shadedTint,vec3(0),vec3(1));

    // A tiny difference across the same cloud's samples resembles soft
    // volume/self-occlusion, without a real directional light computation.
    float shading=clamp(0.86+(highField-highSide)*0.75,0.70,1.05);
    vec3 result=mix(skyColor,highTint*shading,high);
    result=mix(result,shadedTint,low);
    result=mix(result,mix(highTint,shadedTint,0.45),small*(1.0-low));
    cloudCover=1.0-(1.0-high)*(1.0-low)*(1.0-small);
    return result;
}
