// DOOM: HELLFORGE / Milestone C - three visually separated cloud scales.
// Not full volumetric raymarching. Soft pseudo-volume comes from offset density
// samples in two planes and a sparse third puff scale. Clouds are cosmetic and
// inherit sky colors; there is no new sun lighting or lighting-cache change.

#include "sky_artwork.h"

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
// Extend the existing decks with the same periodic noise texture. The
// painted sky seeds both density and tint before celestial effects go in.
vec3 atmosphericCloudDecks(vec3 ray,float longitude,vec3 sky,vec3 art,int profile,
                           vec3 key,vec3 eclipseCenter,vec3 stormLight,bool deep,
                           vec2 artworkData,out float cover) {
    cover=0.0;
    bool city=profile==SKY_ART_BURNING_CITY;
    float height=ray.z;
    float visible=smoothstep(-0.06,city?0.12:0.24,height)*(1.0-smoothstep(0.73,0.91,height));
    if(visible<=0.0)return sky;

    float t=c.fog.w;
    vec2 wind=abs(c.weather.y)+abs(c.weather.z)>0.001?normalize(c.weather.yz):vec2(0.95,0.31);
    float along=dot(wind,vec2(0.95,0.31));
    vec2 farUV=vec2(longitude*3.0-along*t*0.004,height*1.8+t*0.001);
    vec2 midUV=vec2(longitude*5.0+along*t*0.007,height*2.8)+vec2(0.19,0.63);
    vec2 nearUV=vec2(longitude*8.0-along*t*0.010,height*4.2)+vec2(0.61,0.09);
    if(city&&deep) {
        // Let the more distant, broader field overtake the rising column.
        // Bend the two smoke banks at different rates over stationary fires.
        float bend=sin(longitude*18.849556+height*5.0-t*.14);
        midUV.x+=bend*(.05+.12*height);
        nearUV.x+=sin(longitude*31.41593-height*3.0+t*.10)*(.03+.18*height);
        midUV.y-=t*.013;
        nearUV.y-=t*.020;
    } else if(city) {
        // Sampling downward advects the smoke upward. The near bank rises
        // a little faster and thins above the painted fires.
        midUV.y-=t*0.016;
        nearUV.y-=t*0.026;
    } else if(profile==SKY_ART_INFERNO||profile==SKY_ART_BLOOD_HELL) {
        midUV.y+=sin(longitude*12.566371+t*0.19)*0.065;
        nearUV.y+=sin(longitude*18.849556-t*0.14)*0.085;
    } else if(profile==SKY_ART_BROWN_CLOUDS) {
        midUV.y+=sin(longitude*18.849556+t*0.14)*0.075;
    }

    float farField=textureLod(skyCloudPattern,farUV,0.0).r;
    float middle=textureLod(skyCloudPattern,midUV,0.0).r;
    float detail=textureLod(skyCloudPattern,nearUV,0.0).r;
    float fine=smoothstep(0.08,0.30,height)*smoothstep(0.8,1.5,c.fx2.x);
    float luma=dot(art,vec3(0.299,0.587,0.114));
    float warm=max(0.0,art.r-art.b);
    float painted=smoothstep(0.14,0.58,luma);
    vec3 tint=vec3(0.37,0.42,0.51);
    vec3 illumination=vec3(0.76,0.80,0.89);
    float thickness=0.92;
    switch(profile) {
        case SKY_ART_RED_MOUNTAINS:
            tint=vec3(.35,.11,.13); illumination=vec3(.81,.34,.30); thickness=1.16;
            painted=smoothstep(.10,.57,luma*.55+warm*.65);
            break;
        case SKY_ART_INFERNO:
        case SKY_ART_BLOOD_HELL:
            tint=vec3(.29,.063,.080); illumination=vec3(.71,.21,.19); thickness=1.12;
            painted=smoothstep(.10,.56,luma*.50+warm*.65);
            break;
        case SKY_ART_ORANGE_CLOUDS:
            tint=vec3(.44,.24,.12); illumination=vec3(.92,.58,.30); thickness=1.14;
            break;
        case SKY_ART_BROWN_CLOUDS:
            tint=vec3(.36,.30,.26); illumination=vec3(.78,.67,.53); thickness=1.06;
            break;
        case SKY_ART_BURNING_CITY:
            tint=vec3(.31,.26,.24); illumination=vec3(.91,.40,.14);
            painted=smoothstep(.11,.60,luma*.65+warm*.60);
            break;
    }

    // Artwork carries the large shapes. Broad fields overlap their centers
    // softly, while the weaker upper and near banks may drift independently.
    float density=0.59*middle+0.27*farField+0.14*mix(0.5,detail,fine);
    float bias=mix(-0.074,0.10,painted);
    if(deep) {
        if(city) {
            float pooled=artworkData.y*(1.0-smoothstep(.27,.64,height));
            bias=mix(-.14,.19,max(.58*painted,pooled));
            density=mix(density,mix(middle,farField,smoothstep(.25,.62,height)),.22);
        } else if(profile==SKY_ART_BROWN_CLOUDS) {
            bias=mix(-.17,.14,artworkData.x);
            density=mix(density,.62*farField+.38*middle,.25);
            thickness=1.15;
        } else {
            bias=mix(-.15,.12,artworkData.x);
            thickness=1.21;
        }
    }
    float core=smoothstep(0.38,0.76,density+bias);
    float far=0.25*smoothstep(0.44,0.80,farField+bias*0.72);
    float mid=0.71*core;
    float near=0.19*smoothstep(0.51,0.83,detail*0.64+middle*0.36+bias)*fine;
    if(city) {
        float smokeBand=1.0-smoothstep(0.25,0.65,height);
        far*=(deep?.85:.32)*smokeBand;
        mid*=(deep?.88:.58)*smokeBand;
        near*=(deep?.96:.70)*smokeBand;
    }
    far*=visible;
    mid=clamp(mid*visible*thickness,0.0,1.0);
    near*=visible;
    cover=1.0-(1.0-far)*(1.0-mid)*(1.0-near);
    if(cover<0.002)return sky;

    // Project the light onto the panorama and probe the middle field
    // twice toward it. The soft shadow is stronger where both probes
    // encounter dense cloud, with a highlight on a locally exposed edge.
    vec3 across=vec3(-ray.y,ray.x,0.0)/max(length(ray.xy),0.0001);
    vec2 toward=vec2(dot(key,across),key.z-height*dot(key,ray));
    vec2 offset=vec2(0.20,0.18)*toward;
    float front=textureLod(skyCloudPattern,midUV+offset,0.0).r;
    float deeper=textureLod(skyCloudPattern,midUV+offset*2.0,0.0).r;
    float shadow=smoothstep(0.37,0.78,0.50*front+0.32*deeper+0.18*middle+bias);
    float exposed=smoothstep(0.015,0.22,middle-front)*(1.0-shadow);
    float sun=max(c.sun.w,0.0)*(0.18+0.82*smoothstep(-0.12,0.82,dot(ray,key)));
    vec2 fire=vec2(0.0);
    if(city)fire=deep?deepCityFire(longitude,artworkData.y,core,height):vec2(painted,0.0);
    float heat=fire.x*(1.0-smoothstep(0.14,0.62,height));
    float corona=smoothstep(0.961,0.995,dot(ray,eclipseCenter));
    vec3 pigment=mix(art,tint,city?0.56:0.34);
    // Thin red vapor crossing the disc should read as a cloud, without
    // projecting the detailed painted skulls onto the eclipse itself.
    pigment=mix(pigment,tint,0.63*corona);
    vec3 lit=pigment*(1.02-0.37*shadow)+illumination*(0.032+exposed*(0.13+0.10*sun));
    lit+=illumination*heat*(0.05+0.12*exposed)
        +c.sun.rgb*(sun*0.12*exposed)
        +stormLight*(0.46+0.54*exposed)
        +vec3(.19,.023,.017)*(corona*(.11+.15*exposed));
    if(deep) {
        if(city) {
            vec3 ember=vec3(.92,.32,.065);
            sky+=ember*(heat*.15+fire.y*.20)*smoothstep(.002,.12,cover);
            lit+=ember*(heat*(.13+.19*exposed)+fire.y*.27);
        } else if(profile==SKY_ART_BROWN_CLOUDS) {
            lit*=1.0-.24*shadow;
            lit+=illumination*exposed*.15+stormLight*core*.52;
        } else {
            float veil=(1.0-smoothstep(.07,.55,height))*(.16+.28*farField)*smoothstep(.002,.14,cover);
            sky=mix(sky,vec3(.26,.025,.043),veil*(1.0-.85*corona));
            lit+=vec3(.15,.018,.024)*(farField*.34+exposed*.26);
        }
    }
    sky=mix(sky,mix(pigment,lit,0.44),far);
    sky=mix(sky,lit,mid);
    return mix(sky,lit*0.83,near);
}
