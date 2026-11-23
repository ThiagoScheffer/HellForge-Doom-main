// The derived RG texture stores a reconstructed background palette index
// and a signed distance to the painted skyline. For cloud-only artwork
// the original indexed texture is bound here and only R is sampled.
vec3 depthColorAt(ivec2 at,ivec2 size) {
    float value=texelFetch(skyDepthArt,wrapTexel(at,size),0).r;
    return texelFetch(palette,ivec2(int(round(value*255.0)),0),0).rgb;
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
