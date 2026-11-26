#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
// Far-plane sky in uncovered pixels; early depth skips fog behind geometry.
// SURFACE: sky ceilings and the open band above lower sky neighbors, drawn
// into depth so the level beyond them stays hidden as in vanilla.
layout(early_fragment_tests) in;
layout(set=2,binding=0) uniform sampler2D image;
layout(set=2,binding=1) uniform sampler2D palette;
// Milestone A: share the existing deterministic cloud noise texture with mist.
layout(set=2,binding=2) uniform sampler2D skyCloudPattern;
layout(set=2,binding=3) uniform sampler2D skyDepthArt;
layout(std140,set=3,binding=0) uniform Camera CAMERA_BLOCK c;
layout(std140,set=3,binding=1) uniform Lights FLASH_BLOCK;
layout(std140,set=3,binding=2) uniform Fog FOG_BLOCK;
struct SkyFireSource {
    vec4 anchor,flame,smoke,motion,illumination;
};
// Runtime F4 mode, passed on sky fragment uniform slot 3 for all sky passes.
layout(std140,set=3,binding=3) uniform HellforgeSky {
    vec4 skyEnvironment;
    float atmosphericArtwork;
    float deepSkyEnabled;
    float eclipseRidge;
    vec4 depthOrigin; // xyz reference, w normalized top of extracted artwork
    vec4 depthShells; // middle/near radii, then middle/near thickness
    vec4 atmosphereHorizon; // RGB, estimated cloud coverage
    vec4 atmosphereZenith; // RGB, atmosphere kind (zero uses legacy)
    vec4 atmosphereDark; // RGB, near opacity
    vec4 atmosphereMiddle; // RGB, far opacity
    vec4 atmosphereLight; // RGB, wind speed in map units per second
    vec4 atmosphereProjection; // near height, near scale, far scale
    vec4 atmosphereWind; // fixed per-level wind heading in XY
    vec4 sceneryProjection; // far radius, normalized skyline top, style, backdrop opacity
    vec4 fireHeader; // count, nearest radius (without scenery), wind speed, haze blend
    vec4 fireSmokeDark,fireSmokeWarm,fireFlameOuter,fireFlameCore;
    SkyFireSource fireSources[4];
};
layout(location=0) out vec4 outColor;
#include "indexed.glsl"
// The flashlight beam ends at geometry; through the sky it would gather its
// whole length of fog and seem to light the sky itself.
#define NO_BEAM_FOG
#define NO_LOCAL_FOG
#include "lighting.glsl"
#include "sky_storm.glsl"
#include "sky_city.glsl"
#include "sky_clouds.glsl"
#include "sky_horizon.glsl"
#include "sky_immersion.glsl"
#include "sky_eclipse.glsl"
#include "sky_depth.glsl"
#include "sky_atmosphere.glsl"
vec3 skyAir(vec3 color,vec3 low,float amount) {
    float luma=dot(color,vec3(0.299,0.587,0.114));
    return mix(color,mix(vec3(luma),low,0.6),amount);
}
vec3 skySunDirection() {
    return vec3(cos(c.fx2.z)*cos(c.outdoor.w),sin(c.fx2.z)*cos(c.outdoor.w),sin(c.outdoor.w));
}
vec3 skyVisibleSun(vec3 color,vec3 ray,float transmission) {
    float a=acos(clamp(dot(ray,skySunDirection()),-1.0,1.0));
    float disc=1.0-smoothstep(0.016,0.024,a),glow=exp(-a*a/0.012)*0.45+exp(-a/0.35)*0.12;
    float amount=saturate(disc*0.75+glow)*c.fx2.w*transmission;
    return mix(color,max(color,c.sun.rgb),saturate(disc*c.fx2.w*transmission))+c.sun.rgb*amount*(1.0-disc*0.5);
}
vec4 paintedSky(vec2 uv,vec2 size,bool filtered) {
    // Light distance softening: blend in a 2x2 texel box so the sky sits a
    // touch softer than the crisp walls in front of it. Taps stay inside the
    // texture rows so the top and bottom don't wrap into each other.
    vec4 color=indexed(image,palette,uv,filtered,true);
    vec3 soft=vec3(0);
    for(int i=0;i<4;++i) {
        vec2 o=vec2(i&1,i>>1)-0.5;
        soft+=indexed(image,palette,vec2(uv.x+o.x,clamp(uv.y+o.y,0.5,size.y-0.5)),filtered,true).rgb;
    }
    color.rgb=mix(color.rgb,soft/4.0,0.6);
    return color;
}
vec3 skyForegroundAir(vec3 color,vec3 low,float air,float v,float height) {
    color=skyAir(color,low,air);
    color=mix(color,low,smoothstep(0.88,1.02,v));
    if(c.materials.z>0.0&&c.effects.z==0.0)
        color=mix(color,low,0.55*(1.0-smoothstep(-0.05,0.3,height)));
    return color;
}
struct SkyLayerSample {
    vec3 color;
    float alpha;
};
#include "sky_effects.glsl"

vec3 skyCompositeOver(vec3 behind,SkyLayerSample foreground) {
    return mix(behind,foreground.color,clamp(foreground.alpha,0.0,1.0));
}

struct SkyScenerySamples {
    SkyShellSample middle;
    SkyShellSample near;
    SkyLayerSample far;
};

SkyScenerySamples sampleSkyScenery(bool visible,float angle,float rise,vec2 axis,vec2 uv,
                                   float v,ivec2 size,bool filtered) {
    SkyShellSample empty=SkyShellSample(vec3(0.0),0.0,0.0,uv,0.0);
    SkyScenerySamples scenery=SkyScenerySamples(empty,empty,SkyLayerSample(vec3(0.0),0.0));
    bool farVisible=sceneryProjection.x>0.0&&v>=sceneryProjection.y-.032;
    if(!visible&&!farVisible)return scenery;
    vec3 delta=skyDepthEye(c.eye.xyz,depthOrigin.xyz,min(depthShells.x,depthShells.y));
    vec3 eyeInRayBasis=vec3(dot(delta.xy,axis),dot(delta.xy,vec2(-axis.y,axis.x)),delta.z);
    bool crisp=sceneryProjection.x>0.0;
    if(visible) {
        scenery.near=skyShell(angle,rise,eyeInRayBasis,depthShells.yw,2.0,size,filtered,crisp);
        if(scenery.near.coverage<.999)
            scenery.middle=skyShell(angle,rise,eyeInRayBasis,depthShells.xz,1.0,size,filtered,crisp);
    }
    bool needFar=scenery.near.coverage<1.0&&scenery.middle.coverage<1.0;
#if defined(SKY_COMPOSITION_DEBUG) && SKY_COMPOSITION_DEBUG == 4
    needFar=true;
#endif
    if(farVisible&&needFar) {
        vec2 farUV=skyShellUV(angle,rise,eyeInRayBasis,sceneryProjection.x,vec2(size));
        vec4 distant=skyDistantScenery(farUV,size);
        scenery.far=SkyLayerSample(distant.rgb,distant.a);
    }
    return scenery;
}

vec3 sceneryAir(vec3 facade,vec3 horizon,float haze,float row) {
    float distance=haze+(1.0-haze)*smoothstep(.91,1.09,row);
    vec3 subdued=mix(facade,vec3(dot(facade,vec3(.299,.587,.114))),distance*.24);
    return mix(subdued,horizon*.56,distance);
}

vec3 composeSkyScenery(vec3 background,SkyScenerySamples scenery,vec3 horizon,float row,
                       SkyEffects effects,ivec2 size) {
    bool mountains=sceneryProjection.z==1.0;
    background=skyEffectOver(background,effects.layers[0]);
    if(scenery.far.alpha>0.0) {
        scenery.far.color=sceneryAir(scenery.far.color,horizon,mountains?.69:.58,row);
        background=skyCompositeOver(background,scenery.far);
    }
    background=skyEffectOver(background,effects.layers[1]);
    if(scenery.middle.coverage>0.0) {
        vec3 paint=scenery.middle.color;
        paint+=mountains?skyRockSeep(scenery.middle.uv,size,scenery.middle.window)*.72
                        :vec3(.085,.042,.014)*scenery.middle.window;
        vec3 facade=sceneryAir(paint,horizon,mountains?.34:.26,row);
        background=skyCompositeOver(background,SkyLayerSample(facade,scenery.middle.coverage));
    }
    background=skyEffectOver(background,effects.layers[2]);
    if(scenery.near.coverage>0.0) {
        vec3 paint=scenery.near.color;
        paint+=mountains?skyRockSeep(scenery.near.uv,size,scenery.near.window)
                        :vec3(.15,.074,.025)*scenery.near.window;
        vec3 facade=sceneryAir(paint,horizon,mountains?.045:.06,row);
        background=skyCompositeOver(background,SkyLayerSample(facade,scenery.near.coverage));
    }
    return skyEffectOver(background,effects.layers[3]);
}

void main() {
    vec2 size=vec2(textureSize(image,0));
#ifdef SURFACE
    vec3 ray=normalize(vWorld-c.eye.xyz);
#else
    vec2 screen=vUV*2.0-1.0;
    vec3 ray=normalize(c.forward.xyz+c.right.xyz*screen.x/c.projection.x-c.up.xyz*screen.y/c.projection.y);
#endif
    // Both the world-anchored plate and the depth shells use this same
    // perspective ray and the same cylindrical artwork convention.
    float horizontal=max(length(ray.xy),1e-4);
    float angle=atan(ray.y,ray.x);
    float rise=ray.z/horizontal;
    float v=skyHorizon-rise*skyRiseScale;
    vec2 uv=skyPlateUV(angle,rise,size);
    bool filtered=c.effects.x>0.0;
    float depthMode=skyEnvironment.w;
    int cloudProfile=int(atmosphericArtwork);
    bool deep=deepSkyEnabled>0.0;
    bool deepCity=deep&&cloudProfile==SKY_ART_BURNING_CITY;
    bool useAtmosphere=atmosphereZenith.w>0.0;
    bool deepStorm=useAtmosphere||(deep&&cloudProfile==SKY_ART_BROWN_CLOUDS);
    bool depthScene=depthShells.x>0.0;
    bool composedScenery=sceneryProjection.x>0.0;
    bool mountains=sceneryProjection.z==1.0;
    // Keep the cutoff outside the maximum vertical shift and filter reach.
    bool depthBand=depthScene&&v>=depthOrigin.w-.09;
    SkyScenerySamples scenery=sampleSkyScenery(depthBand,angle,rise,ray.xy/horizontal,uv,v,ivec2(size),filtered);
    SkyShellSample middle=scenery.middle,near=scenery.near;
    vec4 color;
    vec2 artworkData=vec2(0.0);
    float foregroundMask=near.coverage+(1.0-near.coverage)*middle.coverage;
    bool obscured=composedScenery&&(foregroundMask>=1.0||scenery.far.alpha>=1.0);
#if defined(SKY_COMPOSITION_DEBUG) && SKY_COMPOSITION_DEBUG == 1
    if(composedScenery)obscured=false;
#endif
    vec3 foreground=vec3(0.0);
    vec3 cloudArt,low;
    float air=0.12+0.33*(1.0-smoothstep(0.0,0.6,rise));
    if(useAtmosphere) {
        color=vec4(skyAtmosphereBase(ray),1.0);
        low=atmosphereHorizon.rgb;
        cloudArt=color.rgb;
    } else {
        if(depthScene) {
            vec2 at=vec2(uv.x,clamp(uv.y,.5,size.y-.5));
            artworkData=textureLod(skyDepthArt,at/vec2(textureSize(skyDepthArt,0)),0.0).ba;
            if(depthBand) {
#ifdef SKY_DEPTH_DEBUG
                // 1: solid mask, 2: object IDs, 3: depth, 4: backdrop, 5: sides.
                color=vec4(skyBackdrop(uv,ivec2(size),filtered),1.0);
                if(SKY_DEPTH_DEBUG==1)color.rgb=vec3(foregroundMask);
                if(SKY_DEPTH_DEBUG==2) {
                    vec2 faceUV=near.coverage>middle.coverage?near.uv:middle.uv;
                    float id=skyShellMetadata(ivec2(floor(faceUV)),ivec2(size)).r*255.0;
                    color.rgb=fract(id*vec3(.37,.59,.81))*foregroundMask;
                }
                if(SKY_DEPTH_DEBUG==3)color.rgb=vec3(near.coverage,0.0,middle.coverage);
                if(SKY_DEPTH_DEBUG==5)color.rgb=vec3(near.side,0.0,middle.side);
                outColor=color;
                return;
#else
                color=vec4(0.0,0.0,0.0,1.0);
                if(near.coverage<1.0)color.rgb=skyBackdrop(uv,ivec2(size),filtered);
#endif
            } else color=paintedSky(uv,size,filtered);
        } else {
            color=paintedSky(uv,size,filtered);
            foreground=color.rgb;
            if(depthMode>0.0)color.rgb=artworkSkyLayers(color.rgb,uv,size,depthMode,cloudProfile>0,deep,foregroundMask,artworkData);
            else color.rgb=immersedSky(color.rgb,uv,size,ray,filtered);
        }
        cloudArt=color.rgb;
        // The sky's own low-row color, the tint of air at the horizon.
        low=vec3(0);
        for(int i=0;i<8;++i)
            low+=indexed(image,palette,vec2((float(i)+0.5)*size.x/8.0,size.y*0.92),false,true).rgb;
        low/=8.0;
        // Aerial perspective: contrast and saturation fall toward the horizon,
        // pulled toward a grey-tinted mix of that low color. Restrained overall.
        color.rgb=skyAir(color.rgb,low,air);
        if(deepCity&&foregroundMask<1.0) {
            const vec3 farHaze=vec3(.40,.31,.23);
            float distance=artworkData.x*(1.0-foregroundMask);
            color.rgb=mix(color.rgb,farHaze,distance*.30);
        }
        // High-altitude haze in the sky's own upper-row color hides the clamped,
        // stretched top rows. It begins just above the frame top when looking
        // level and is opaque a little past the painted edge.
        float high=deep?1.0-smoothstep(-0.13,0.28,v):smoothstep(0.12,-0.08,v);
        if(high>0.0) {
            vec3 cap=vec3(0);
            for(int i=0;i<8;++i)
                for(int j=0;j<2;++j)
                    cap+=indexed(image,palette,vec2((float(i)+0.5)*size.x/8.0,size.y*(0.03+0.08*float(j))),false,true).rgb;
            cap/=16.0;
            if(deep) {
                // The cylindrical painting runs out overhead. Blend into a
                // broad wind-driven field on the horizontal plane, where all
                // azimuths meet continuously at the zenith.
                vec2 overhead=ray.xy*1.37+vec2(.17,.39)+c.fog.w*vec2(-.005,.002);
                float field=textureLod(skyCloudPattern,overhead,0.0).r;
                cap*=mix(.50,1.08,smoothstep(.16,.82,field));
            }
            color.rgb=mix(color.rgb,cap,high);
            if(cloudProfile>0)cloudArt=mix(cloudArt,cap,high);
        }
        // Past the painted bottom edge, fade into the low color the same way so
        // the clamped bottom row doesn't streak when looking down from a ledge.
        color.rgb=mix(color.rgb,low,smoothstep(0.88,1.02,v));
        // Haze toward the horizon in the sky's own low-row color, so distant
        // silhouettes meet a soft band rather than a hard painted edge.
        if(c.materials.z>0.0&&c.effects.z==0.0)
            color.rgb=mix(color.rgb,low,0.55*(1.0-smoothstep(-0.05,0.3,ray.z)));
        if(foregroundMask>0.0&&!depthScene)foreground=skyForegroundAir(foreground,low,air,v,ray.z);
    }
    // D1/2: negative style.w encodes episode/profile and themed-lightning
    // switch without adding a Camera field. Inferno [-3,-1], Deimos [-5,-3].
    // E1+: non-overlapping 2-wide ranges, reserved through Doom II SKY3.
    // Old Inferno [-3,-1] and Deimos [-5,-3] retain identical decoding.
    bool bloodHell=c.style.w<=-9.0;
    bool city=c.style.w<=-7.0&&!bloodHell;
    bool starport=c.style.w<=-5.0&&!city&&!bloodHell;
    bool deimos=c.style.w<=-3.0&&!starport&&!city&&!bloodHell;
    bool inferno=c.style.w<=-1.0&&!deimos&&!starport&&!city&&!bloodHell;
    bool crimson=(inferno&&c.style.w>-2.0)||(bloodHell&&c.style.w>-10.0);
    bool coldElectrical=deimos&&c.style.w>-4.0;
    float boltSeed=inferno?((crimson?-1.0:-2.0)-c.style.w):
        deimos?((coldElectrical?-3.0:-4.0)-c.style.w):
        starport?-5.0-c.style.w:city?-7.0-c.style.w:
        bloodHell?((crimson?-9.0:-10.0)-c.style.w):c.style.w;
    // E1 only: dusk sand scattering close to the low horizon, leaving
    // the recognisable painted Starport cloud shapes visible.
    if(starport) {
        float dust=1.0-smoothstep(-0.15,0.75,rise);
        color.rgb=mix(color.rgb,vec3(.42,.265,.155),.21*dust);
    }
    // Lightning is behind the three virtual horizon layers and clouds.
    // The skyline therefore masks the bolt rather than being lit on top.
    if(!obscured&&c.style.z>0.0) {
        float stroke=hellforgeBoltMask(angle,rise,c.style.y,boltSeed);
        float halo=hellforgeBoltHalo(angle,rise,c.style.y);
        // Clearer bolt cores without increasing the full-sky flash amplitude.
        // Both core and halo remain behind the distant horizon/cloud layers.
        vec3 core=bloodHell&&crimson?vec3(1.65,.18,.30):
                  crimson?vec3(1.55,0.32,0.18):
                  coldElectrical?vec3(1.14,1.55,1.74):vec3(1.12,1.32,1.57);
        color.rgb=mix(color.rgb,max(color.rgb,core),
                      clamp(stroke*c.style.z*2.55+halo*c.style.z*0.15,0.0,0.95));
    }
    // Milestone C: three virtual horizon layers compose behind the
    // sky cloud decks. No map geometry or additional GPU drawing is created.
    // Shared with far-plane, sky surfaces, transferred skies and reflections.
    int hfMode=int(skyEnvironment.x+0.5);
    bool eclipse=skyEnvironment.z>0.0;
    // The eclipse sits behind the existing skyline and cloud decks.
    if(eclipse)color.rgb=hellBloodEclipse(ray,color.rgb,eclipseRidge>0.0);
    bool citySmoke=city||hfMode==5||depthMode==3.0||(skyEnvironment.y==2.0&&c.effects.z==0.0&&
        (uint(c.map.w)&(LS_ENABLED|LS_MORPH))==(LS_ENABLED|LS_MORPH));
    float cloudCover=0.0;
    float cloudTransmission=1.0;
    if(useAtmosphere) {
        if(!obscured) {
            // Celestial light enters the atmosphere from behind both layers.
            if(!eclipse&&c.fx2.w>0.0)color.rgb=skyVisibleSun(color.rgb,ray,1.0);
            vec3 key=mountains&&eclipse?bloodEclipseCenter(eclipseRidge>0.0):
                c.sun.w>0.0?skySunDirection():vec3(.56,-.45,.69);
            vec3 stormLight=vec3(0.0);
            if(c.weather.w>0.0) {
                float halo=hellforgeBoltHalo(angle,rise,c.style.y);
                stormLight=vec3(.51,.55,.68)*c.weather.w*(.06+.54*halo);
            }
            SkyAtmosphereResult atmosphere=renderSkyAtmosphere(color.rgb,ray,angle,key,stormLight);
            color.rgb=atmosphere.color;
            cloudCover=atmosphere.cloudCoverage;
            cloudTransmission=atmosphere.transmission;
        }
    } else if(cloudProfile>0&&(foregroundMask<1.0||(deepCity&&(depthScene?middle.coverage>0.0:artworkData.x>.25)))) {
        vec3 eclipseCenter=eclipse?bloodEclipseCenter(eclipseRidge>0.0):vec3(0.0);
        vec3 key=deepCity?vec3(0.0,0.0,-1.0):
                 eclipse?eclipseCenter:c.sun.w>0.0?skySunDirection():vec3(0.56,-0.45,0.69);
        vec3 stormLight=vec3(0.0);
        if(c.weather.w>0.0) {
            // Scatter near the active strike. The existing broad flash
            // later in the shader still handles diffuse sky illumination.
            float halo=hellforgeBoltHalo(angle,rise,c.style.y);
            vec3 scatter=crimson?vec3(.37,.065,.075):
                         coldElectrical?vec3(.20,.33,.41):vec3(.18,.23,.34);
            stormLight=scatter*(c.weather.w*halo*0.70);
        }
        // City metadata changes from warm smoke to painted panes at
        // the cutout. Pane values only reach the foreground composite.
        vec2 cloudData=artworkData;
        if(deepCity&&!depthScene)cloudData.y*=1.0-foregroundMask;
        color.rgb=atmosphericCloudDecks(ray,angle/6.2831853,color.rgb,cloudArt,cloudProfile,
                                        key,eclipseCenter,stormLight,deep,cloudData,cloudCover);
    } else if(hfMode>=2 && hfMode<=5 && foregroundMask<1.0) {
        if(depthMode==0.0)color.rgb=hellforgeDistantHorizon(ray,color.rgb,low,hfMode);
        if(hfMode!=5&&depthMode!=3.0)color.rgb=hellforgeCloudDecks(ray,color.rgb,low,hfMode,cloudCover);
        // E1 Starport: existing clouds become slightly heavier at sundown.
        // No new layers or extra texture lookups are required.
        if(starport)color.rgb=mix(color.rgb,vec3(.23,.185,.14),
                                  .24*clamp(cloudCover,0.0,1.0));
        if(bloodHell)color.rgb=mix(color.rgb,vec3(.25,.10,.14),
                                   .22*clamp(cloudCover,0.0,1.0));
        // Electrical glow scatters through the soft cloud layers; the
        // bolt itself stays beneath the silhouettes and cloud shading.
        if(c.weather.w>0.0) {
            float halo=hellforgeBoltHalo(angle,rise,c.style.y);
            vec3 scatter=bloodHell&&crimson?vec3(.39,.045,.11):
                         crimson?vec3(0.37,0.065,0.04):
                         coldElectrical?vec3(0.20,0.33,0.41):vec3(0.18,0.23,0.34);
            color.rgb+=scatter*(c.weather.w*cloudCover*(0.12+0.34*halo));
        }
    }
    if(!useAtmosphere&&cloudProfile==0&&citySmoke&&foregroundMask<1.0) {
        float smokeCover;
        color.rgb=burningCitySmoke(ray,angle,color.rgb,smokeCover);
        cloudCover=1.0-(1.0-cloudCover)*(1.0-smokeCover);
    }
    // The sun at the baked yaw and the height the sky paints it (c.outdoor.w;
    // bakeSun in scene3d/bake_sun.cpp, 40 degrees for DOOM): a small
    // disc and a wide glow in its tint. Smooth, like the softened sky around
    // it; a dither here shimmers over a large part of the sky.
    if(!useAtmosphere&&!eclipse&&c.fx2.w>0.0&&foregroundMask<1.0) {
        // The visible sun sits behind moving cloud layers, while baked
        // world illumination remains unchanged in this prototype.
        color.rgb=skyVisibleSun(color.rgb,ray,1.0-0.80*cloudCover);
    }
    // Resolve the shells back to front, over the existing smoke and celestial layers.
    if(composedScenery||(fireHeader.x>0.0&&!depthScene)) {
#if defined(SKY_COMPOSITION_DEBUG) && SKY_COMPOSITION_DEBUG != 5
        if(SKY_COMPOSITION_DEBUG==2)color.rgb=vec3(foregroundMask);
        if(SKY_COMPOSITION_DEBUG==3)color.rgb=vec3(near.coverage,middle.coverage,scenery.far.alpha);
        if(SKY_COMPOSITION_DEBUG==4)color.rgb=skyCompositeOver(vec3(0.0),scenery.far);
        if(SKY_COMPOSITION_DEBUG==6)color.rgb=vec3(near.side,0.0,middle.side);
        outColor=color;
        return;
#endif
        if(sceneryProjection.w>0.0&&!obscured&&v>.015) {
            vec4 wall=skySceneryBackdrop(uv,ivec2(size));
            float opacity=wall.a*sceneryProjection.w*smoothstep(.015,.48,v);
            // The translucent painting frames the eclipse. Leave the disc
            // and immediate rim readable through its thinner central air.
            if(eclipse) {
                float chordSquared=2.0-2.0*dot(ray,bloodEclipseCenter(eclipseRidge>0.0));
                opacity*=mix(.23,1.0,smoothstep(.019,.052,chordSquared));
            }
            color.rgb=skyCompositeOver(color.rgb,SkyLayerSample(wall.rgb*.71,opacity));
        }
        vec3 opacity=vec3(scenery.far.alpha,middle.coverage,near.coverage);
        SkyEffects effects=sampleSkyEffects(angle,rise,ray.xy/horizontal,opacity,low,cloudCover,cloudTransmission);
#if defined(SKY_EFFECTS_DEBUG) && SKY_EFFECTS_DEBUG < 6
        outColor=vec4((SKY_EFFECTS_DEBUG==1?color.rgb*.26:vec3(0.0))+effects.debug,1.0);
        return;
#endif
        color.rgb=composeSkyScenery(color.rgb,scenery,low,v,effects,ivec2(size));
    } else if(depthBand) {
        for(int layer=0;layer<2;++layer) {
            SkyShellSample surface=layer==0?middle:near;
            if(surface.coverage<=0.0)continue;
            vec3 facade=skyForegroundAir(surface.color,low,air,v,ray.z);
            if(deepCity&&v<1.02) {
                vec2 at=vec2(mod(surface.uv.x,size.x*skyRepeats),surface.uv.y);
                vec3 lit=deepCityFacade(facade,at,layer==0?.64:.15,surface.window,cloudCover,rise);
                facade=mix(lit,facade,smoothstep(.88,1.02,v));
            }
            color.rgb=skyCompositeOver(color.rgb,SkyLayerSample(facade,surface.coverage));
        }
    } else if(foregroundMask>0.0) {
        if(deepCity&&v<1.02) {
            vec2 at=vec2(mod(uv.x,size.x*4.0),uv.y);
            vec3 facade=deepCityFacade(foreground,at,artworkData.x,artworkData.y,
                                       cloudCover,rise);
            foreground=mix(facade,foreground,smoothstep(.88,1.02,v));
        }
        color.rgb=skyCompositeOver(color.rgb,SkyLayerSample(foreground,foregroundMask));
    }
    // Far rain: weather.cpp drops rain only in the map's open sectors near
    // the eye, and the sky (ceilings and the band over lower neighbors
    // included) hides everything past it, so the rain would end at the
    // courtyard walls and the sky ceiling. Past them it falls on in the sky
    // itself: two sheets of streaks one Doom pixel wide, about 900 and 1800
    // units out, world-anchored like the sky, falling at the rain's speed
    // and slanted by its wind, stepping at the tic rate. Pale, the far sheet
    // fainter, fading out toward the zenith.
    if(c.weather.x>0.0) {
        float pixel=160.0*c.projection.x; // Doom pixels per radian
        float tic=floor(c.fog.w*35.0)/35.0;
        vec2 across=vec2(-sin(angle),cos(angle));
        float slant=dot(c.weather.yz,across)/520.0;
        float drops=0.0;
        for(int layer=0;layer<2;++layer) {
            float far=layer==0?900.0:1800.0;
            vec2 cellAt=floor(vec2(angle,rise)*pixel);
            float column=floor(cellAt.x+slant*cellAt.y+0.5)+float(layer)*7919.0;
            float seed=fract(sin(column*12.9898+float(layer)*78.233)*43758.5453);
            if(seed>=0.45*c.weather.x) continue;
            float period=36.0+28.0*fract(seed*91.7),streak=3.0+4.0*fract(seed*37.1);
            float along=cellAt.y+520.0/far*pixel*tic+seed*period*11.0;
            if(mod(along,period)<streak) drops=max(drops,layer==0?0.42:0.22);
        }
        drops*=1.0-smoothstep(0.9,1.8,rise);
        // Existing lightning briefly catches droplets at the horizon.
        // Does not brighten indoor/world surfaces or change thunder timing.
        drops*=1.0+0.30*clamp(c.weather.w,0.0,0.7);
        color.rgb=mix(color.rgb,max(color.rgb*1.5,low*1.3+0.05),drops);
    }
    // E3: distant crimson droplets beyond the map's sky boundary. This is
    // purely the sky color; it never invokes water rain or wet-surface code.
    if(bloodHell&&c.effects.z==0.0) {
        float tic=floor(c.fog.w*35.0)/35.0;
        vec2 cellAt=floor(vec2(angle,rise)*(160.0*c.projection.x));
        float column=cellAt.x+floor(cellAt.y*.055);
        float seed=fract(sin(column*12.9898+37.719)*43758.5453);
        if(seed<.32) {
            float along=cellAt.y+tic*(36.0+seed*18.0)+seed*197.0;
            float period=44.0+21.0*seed;
            float drop=1.0-step(3.8+seed*3.0,mod(along,period));
            drop*=(1.0-smoothstep(.85,1.65,rise));
            color.rgb=mix(color.rgb,vec3(.57,.075,.105),.36*drop);
        }
    }
    // An episode whose outdoors lie dark (Game_Mood) dims its sky with them,
    // and its hour can tint it.
    color.rgb*=c.mood.x*c.mood.yzw;
    // Inferno's smoky horizon tint preserves authored WAD art while making
    // the air read dry and volcanic; independent of rain and Immersive Sky.
    if(inferno) {
        float band=(1.0-smoothstep(-0.08,0.55,ray.z))*0.22;
        color.rgb=mix(color.rgb,vec3(0.31,0.10,0.065),band);
    } else if(bloodHell&&!mountains) {
        // E3: a cold blood-grey horizon veil over the existing skull sky art.
        float band=(1.0-smoothstep(-.16,.80,ray.z))*.58;
        color.rgb=mix(color.rgb,vec3(.31,.17,.23),band);
    } else if(deimos) {
        // D2.1: a cold, opaque-looking whiteout over the far horizon.
        // c.haze.w is already gated by the user's Fog setting; this affects
        // sky pixels only, not sealed indoor geometry or baked lighting.
        float snow=clamp(c.haze.w,0.0,1.0);
        // Match near-field frost visibility to the corresponding world-space
        // extinction: Heavy/Whiteout bleach the horizon, Light stays readable.
        float band=(1.0-smoothstep(-0.18,0.83,ray.z))*(0.11+0.80*snow);
        vec3 frost=vec3(0.43,0.47,0.58);
        color.rgb=mix(color.rgb,frost,band);
        float bleach=(1.0-smoothstep(0.18,0.98,ray.z))*(0.06+0.27*snow);
        float gray=dot(color.rgb,vec3(0.299,0.587,0.114));
        color.rgb=mix(color.rgb,vec3(gray)*vec3(0.94,0.99,1.08),bleach);
    }
    // Brief diffuse electrical illumination of the visible sky only.
    // No full-screen flash: underground rooms and closed ceilings stay dark.
    if(c.weather.w>0.0&&!deepStorm) {
        float pulse=clamp(c.weather.w,0.0,0.75);
        vec3 diffuse=crimson?vec3(0.85,0.23,0.14):
                     coldElectrical?vec3(0.73,0.86,1.01):vec3(0.60,0.72,0.96);
        color.rgb=mix(color.rgb,max(color.rgb,diffuse),pulse);
    }
    vec4 fog=fogAlong(c.eye.xyz+ray*2000.0);
    color.rgb=(color.rgb*fog.a+fog.rgb)*c.fx.x;
    // Mirror pass (world.frag): the sky is far, but stays a faint reflection.
    color.a=(c.water.z==0.0&&c.water.w>0.0)?0.35:1.0;
    outColor=color;
}
