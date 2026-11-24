const float skyTurns=6.2831853;
const float skyRepeats=4.0;
const float skyHorizon=.70;
const float skyRiseScale=.68;

vec2 skyPlateUV(float angle,float rise,vec2 size) {
    return vec2(-angle/skyTurns*size.x*skyRepeats,
                clamp(skyHorizon-skyRiseScale*rise,.001,.999)*size.y);
}

vec3 skyDepthPalette(float value) {
    return texelFetch(palette,ivec2(int(round(value*255.0)),0),0).rgb;
}

ivec2 skyDepthTexel(ivec2 at,ivec2 size) {
    return ivec2(wrapTexel(ivec2(at.x,0),size).x,clamp(at.y,0,size.y-1));
}

vec4 skyShellMetadata(ivec2 at,ivec2 size) {
    return texelFetch(skyDepthArt,skyDepthTexel(at,size)+ivec2(0,size.y),0);
}

vec3 skyBackdrop(vec2 uv,ivec2 size,bool filtered) {
    if(!filtered)
        return skyDepthPalette(texelFetch(skyDepthArt,skyDepthTexel(ivec2(floor(uv)),size),0).r);
    vec2 p=uv-.5,f=fract(p);
    ivec2 at=ivec2(floor(p));
    vec3 a=skyDepthPalette(texelFetch(skyDepthArt,skyDepthTexel(at,size),0).r);
    vec3 b=skyDepthPalette(texelFetch(skyDepthArt,skyDepthTexel(at+ivec2(1,0),size),0).r);
    vec3 c=skyDepthPalette(texelFetch(skyDepthArt,skyDepthTexel(at+ivec2(0,1),size),0).r);
    vec3 d=skyDepthPalette(texelFetch(skyDepthArt,skyDepthTexel(at+ivec2(1,1),size),0).r);
    return mix(mix(a,b,f.x),mix(c,d,f.x),f.y);
}

vec3 skyDepthEye(vec3 eye,vec3 origin,float nearestRadius) {
    vec3 delta=eye-origin;
    delta.xy/=1.0+length(delta.xy)/(nearestRadius*.0875);
    delta.z/=1.0+abs(delta.z)/(nearestRadius*.025);
    return delta;
}

// The XY intersection decomposes in the horizontal ray basis. The hit has
// the same transverse coordinate as the eye. Its forward coordinate is the
// positive root on a unit cylinder. The bounded eye is always inside it.
vec2 skyShellUV(float angle,float rise,vec3 eyeInRayBasis,float radius,vec2 size) {
    vec3 relative=eyeInRayBasis/radius;
    float travel=sqrt(1.0-relative.y*relative.y)-relative.x;
    return skyPlateUV(angle+asin(relative.y),rise*travel+relative.z,size);
}

struct SkyShellSample {
    vec3 color;
    float coverage;
    float window;
    vec2 uv;
    float side;
};

SkyShellSample skyShell(float angle,float rise,vec3 eyeInRayBasis,vec2 radii,
                        float layer,ivec2 size,bool filtered,bool crisp) {
    vec2 front=skyShellUV(angle,rise,eyeInRayBasis,radii.x,vec2(size));
    SkyShellSample surface=SkyShellSample(vec3(0.0),0.0,0.0,front,0.0);
    vec2 sampleAt=filtered&&crisp?sharpen(front,vec2(.28)):front;
    vec2 p=filtered?sampleAt-.5:sampleAt;
    ivec2 at=ivec2(floor(p));
    vec2 f=fract(p);
    for(int i=0;i<(filtered?4:1);++i) {
        ivec2 offset=ivec2(i&1,i>>1);
        vec2 factor=mix(1.0-f,f,vec2(offset));
        float weight=filtered?factor.x*factor.y:1.0;
        if(crisp&&weight<=0.0)continue;
        vec4 meta=skyShellMetadata(at+offset,size);
        if(abs(meta.g*255.0-layer)>.5)continue;
        surface.coverage+=weight;
        surface.window+=weight*meta.a;
        surface.color+=weight*texel(image,palette,skyDepthTexel(at+offset,size),false).rgb;
    }

    if(surface.coverage<.999&&abs(eyeInRayBasis.y)>.01&&radii.y>0.0) {
        vec2 rear=skyShellUV(angle,rise,eyeInRayBasis,radii.x+radii.y,vec2(size));
        float width=abs(rear.x-front.x);
        if(width>(crisp?.04:.015)) {
            // Only expose lateral surfaces where the rear ray is inside a
            // building and the front ray has crossed its side in longitude.
            // Testing at the rear row excludes bands caused by roof height.
            float x=crisp&&!filtered?floor(rear.x):
                (crisp?sharpen(rear,vec2(.28)).x:rear.x)-.5;
            int row=int(rear.y);
            int column=skyDepthTexel(ivec2(int(floor(front.x)),0),size).x;
            float frontObject=texelFetch(skyDepthArt,ivec2(column,size.y*2),0).r;
            float side=0.0;
            vec3 sideColor=vec3(0.0);
            for(int i=0;i<2;++i) {
                vec4 meta=skyShellMetadata(ivec2(int(floor(x))+i,row),size);
                if(abs(meta.g*255.0-layer)>.5||meta.r==frontObject)continue;
                float weight=(i==0?1.0-fract(x):fract(x))*min(width,1.0);
                side+=weight;
                sideColor+=weight*(skyDepthPalette(meta.b)*vec3(.63,.59,.55)+vec3(.013,.009,.007));
            }
            float behind=1.0-surface.coverage;
            surface.side=side*behind;
            surface.coverage+=surface.side;
            surface.color+=sideColor*behind;
        }
    }
    if(surface.coverage>0.0) {
        surface.color/=surface.coverage;
        surface.window/=surface.coverage;
    }
    return surface;
}

// The final four atlas tiles make one panorama. Longitude wraps across
// the whole turn, while elevation stays within its own tile.
vec4 skyDistantScenery(vec2 uv,ivec2 size) {
    int column=int(mod(floor(uv.x),float(size.x)*skyRepeats));
    int tile=column/size.x;
    int row=clamp(int(floor(uv.y)),0,size.y-1);
    vec2 sampleData=texelFetch(skyDepthArt,ivec2(column%size.x,(2+tile)*size.y+1+row),0).rg;
    return sampleData.y>0.0?vec4(skyDepthPalette(sampleData.x),sampleData.y):vec4(0.0);
}

// The legacy derived RG texture stores a reconstructed background palette index
// and a signed distance to the painted skyline. For cloud-only artwork
// the original indexed texture is bound here and only R is sampled.
vec3 depthColorAt(ivec2 at,ivec2 size) {
    float value=texelFetch(skyDepthArt,wrapTexel(at,size),0).r;
    return skyDepthPalette(value);
}

vec3 depthCloudColor(vec2 uv,ivec2 size) {
    float x=uv.x-0.5;
    ivec2 at=ivec2(int(floor(x)),int(uv.y));
    return mix(depthColorAt(at,size),depthColorAt(at+ivec2(1,0),size),fract(x));
}

vec3 artworkSkyLayers(vec3 painted,vec2 uv,vec2 size,float mode,bool atmospheric,bool deep,
                      out float foreground,out vec2 artworkData) {
    foreground=0.0;
    artworkData=vec2(0.0);
    float roomToMove=1.0;
    if(deep||mode>=2.0) {
        vec2 at=vec2(uv.x,clamp(uv.y,0.5,size.y-0.5));
        vec4 layers=textureLod(skyDepthArt,at/size,0.0);
        if(deep)artworkData=layers.ba;
        if(mode>=2.0) {
            foreground=smoothstep(0.48,0.52,layers.g);
            roomToMove=1.0-smoothstep(0.05,0.41,layers.g);
        }
        if(roomToMove<=0.0) {
            // Use the reconstructed sky at the cutout's lower edge. Opaque
            // foreground pixels are restored after clouds and celestial glow.
            return foreground>0.0&&foreground<1.0?depthCloudColor(uv,ivec2(size)):painted;
        }
    }
    // The city painting contains distant facades above the primary cut.
    // Keep the whole plate fixed and move only the separate smoke banks.
    if(deep&&mode==3.0)return painted;
    float altitude=1.0-uv.y/size.y;
    float t=c.fog.w;
    float drift;
    if(atmospheric) {
        // Ease the painted forms independently of the advected noise decks.
        // The integer longitude repeat wraps with the original panorama.
        float phase=uv.x/size.x*6.2831853+altitude*2.7;
        drift=roomToMove*(0.65*sin(t*0.10)+0.72*sin(phase+t*0.15));
    } else {
        float slow=0.72*sin(t*0.17);
        float mid=1.16*sin(t*0.31+(altitude-0.5)*2.1)-1.16*sin((altitude-0.5)*2.1);
        drift=(slow+mid*(1.0-smoothstep(0.20,0.90,altitude)))*roomToMove;
    }
    // Apply only the difference between the two samples. With zero drift
    // the sky retains its existing palette and filtering exactly.
    vec3 still=depthCloudColor(uv,ivec2(size));
    vec3 moved=depthCloudColor(uv+vec2(drift,0.0),ivec2(size));
    return max(vec3(0.0),painted+(moved-still)*0.62);
}
