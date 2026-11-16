// DOOM: HELLFORGE / Milestone C - distant landscapes.
// Three periodic, depth-separated virtual silhouettes. No meshes, occluders,
// world-state changes or lighting bakes. The whole effect is fragment-only.
// skyEnvironment.x: 0=off, 2=rocky, 3=icy, 4=inferno, 5=industrial city.
const float hfTau=6.28318530718;

float hfHash(float x) { return fract(sin(x*127.1+37.719)*43758.5453123); }

// All frequencies are integers: a ridge joins exactly at +/-PI when the
// viewer pans 360 degrees; different detail scales avoid a repeated sawtooth.
float hfRidge(float az,float seed) {
    float broad=0.48+0.20*sin(az*3.0+seed*1.37)
                    +0.13*sin(az*7.0-seed*2.1);
    float crests=0.22*pow(abs(sin(az*13.0+seed)),2.5)
                   +0.12*abs(sin(az*31.0-seed*3.0));
    return clamp(broad*0.67+crests,0.05,1.0);
}

// Different apparent distances are seen from the same camera. The tangent
// displacement is intentionally mild, unlike nearby DOOM map geometry.
float hfParallaxYaw(vec3 ray,float range) {
    vec2 dir=normalize(ray.xy);
    return atan(dir.y,dir.x)+dot(c.eye.xy,vec2(-dir.y,dir.x))/range;
}

float hfCity(vec2 ignored,float az) {
    float scaled=fract(az/hfTau)*96.0;
    float cell=floor(scaled),u=fract(scaled);
    float r=hfHash(cell+11.0);
    float left=0.05+0.09*hfHash(cell+29.0);
    float right=0.06+0.11*hfHash(cell+47.0);
    float body=step(left,u)*(1.0-step(1.0-right,u));
    float top=0.055+0.155*pow(hfHash(cell+71.0),1.4);
    top*=r<0.15?0.25:r<0.28?0.53:1.0;
    float antenna=step(0.87,r)*step(0.44,u)*step(u,0.48)*0.15;
    float broken=0.025*max(0.0,1.0-abs(u-0.56)*11.0)*step(0.45,r);
    return body*(top+broken)+antenna;
}

float hfFireSpire(float az) {
    float p=fract(az/hfTau)*35.0;
    float cell=floor(p),u=fract(p);
    float center=0.17+0.66*hfHash(cell+19.0);
    float width=0.08+0.06*hfHash(cell+31.0);
    return pow(max(0.0,1.0-abs(u-center)/width),2.2)
          *(0.09+0.24*hfHash(cell+43.0));
}

vec3 hellforgeDistantHorizon(vec3 ray,vec3 sky,vec3 low,int profile) {
    if(profile<2 || profile>5) return sky;
    float horizontal=length(ray.xy);
    if(horizontal<0.7) return sky;
    float rise=ray.z/horizontal;
    if(rise>0.58 || rise< -0.40) return sky;

    float azFar=hfParallaxYaw(ray,88000.0);
    float azMid=hfParallaxYaw(ray,45000.0);
    float azNear=hfParallaxYaw(ray,22000.0);
    float altitudeFar=clamp(c.eye.z/88000.0,-0.03,0.03);
    float altitudeMid=clamp(c.eye.z/45000.0,-0.045,0.045);
    float altitudeNear=clamp(c.eye.z/22000.0,-0.06,0.06);

    float farH=0.045+0.160*hfRidge(azFar,0.77)-altitudeFar;
    float midH=0.026+0.150*hfRidge(azMid,1.98)-altitudeMid;
    float nearH=0.015+0.140*hfRidge(azNear,3.18)-altitudeNear;

    if(profile==2) {  // DOOM E1, DOOM II early: weathered, uneven mountains
        farH+=0.020*pow(abs(sin(azFar*19.0+0.9)),3.0);
        midH+=0.048*pow(abs(sin(azMid*27.0-1.1)),3.0);
    } else if(profile==3) {  // DOOM E2: cold ridges with sharper snow peaks
        farH+=0.082*pow(abs(sin(azFar*17.0+1.4)),3.4);
        midH+=0.045*pow(abs(sin(azMid*23.0)),4.0);
        nearH*=0.78;
    } else if(profile==4) {  // DOOM E3/hell: charred crags and broken spikes
        farH+=0.055*abs(sin(azFar*23.0+1.7));
        midH+=0.063*abs(sin(azMid*19.0+2.8));
        nearH+=hfFireSpire(azNear);
    } else {  // DOOM II later: burnt urban horizon behind hills
        farH*=0.82;
        nearH=max(nearH,hfCity(vec2(0.0),azNear)-altitudeNear);
    }

    // Approximate spatial haze from the painted WAD sky, never a fixed
    // daytime-blue tint. Red Inferno skies stay red; snowy skies stay cold.
    float lum=dot(low,vec3(0.299,0.587,0.114));
    vec3 neutral=vec3(lum);
    vec3 farTint=mix(low,neutral,0.18)*0.88+vec3(0.012);
    vec3 midTint=mix(low,neutral,0.10)*0.58+vec3(0.008);
    vec3 nearTint=low*0.36+vec3(0.005);
    if(profile==3) {
        farTint=mix(farTint,vec3(lum*0.97,lum*1.02,lum*1.07),0.24);
        midTint=mix(midTint,vec3(lum*0.80,lum*0.88,lum*0.98),0.16);
    } else if(profile==4) {
        // Warm only by a ratio of the actual sky palette; not emissive light.
        farTint*=vec3(1.07,0.90,0.85);
        midTint*=vec3(1.09,0.84,0.78);
        nearTint*=vec3(1.08,0.83,0.77);
    }
    farTint=clamp(farTint,0.0,1.0);
    midTint=clamp(midTint,0.0,1.0);
    nearTint=clamp(nearTint,0.0,1.0);

    float edge=max(fwidth(rise)*1.5,0.0023);
    float farMask=1.0-smoothstep(farH-edge,farH+edge,rise);
    float midMask=1.0-smoothstep(midH-edge,midH+edge,rise);
    float nearMask=1.0-smoothstep(nearH-edge,nearH+edge,rise);
    float fadeBelow=smoothstep(-0.36,-0.08,rise);
    vec3 outSky=mix(sky,farTint,farMask*fadeBelow*0.65);
    outSky=mix(outSky,midTint,midMask*fadeBelow*0.78);
    return mix(outSky,nearTint,nearMask*fadeBelow*0.88);
}
