#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
layout(set=2,binding=0) uniform sampler2D image;
layout(set=2,binding=1) uniform sampler2D bloom;
layout(set=2,binding=2) uniform sampler2D hud;
layout(set=2,binding=3) uniform sampler3D paletteLUT;
layout(std140,set=3,binding=0) uniform HudRect { vec4 hudRect; vec4 hudDock; };
layout(std140,set=3,binding=1) uniform Camera CAMERA_BLOCK c;
layout(set=2,binding=4) uniform sampler2D photoDepth;
layout(std140,set=3,binding=2) uniform Photo {
    vec4 photo;
    vec4 shutter;
    vec4 chromatic; // Displacement at 1080p, coverage (0-1), outer radius, reserved.
    vec4 sceneRect; // Active viewport bounds in image pixels.
    vec4 overlayRects[3];
};
layout(location=0) out vec4 outColor;
float photoZ(vec2 uv){float d=textureLod(photoDepth,uv,0).r;return c.projection.z*c.projection.w/(c.projection.w-d*(c.projection.w-c.projection.z));}
float hudCoverage(vec2 screenUV) {
    // hudTexture is a 320x600 atlas: base HUD = top third, docked
    // widescreen HUD = middle third, speedrun = bottom third. Match the
    // first two layers exactly to drawOverlays(). Reading
    // halves instead produces displaced negative HUD imprints in bloom.
    vec2 uv=(screenUV-hudRect.xy)/hudRect.zw;
    float coverage=all(greaterThanEqual(uv,vec2(0)))&&all(lessThanEqual(uv,vec2(1)))?textureLod(hud,vec2(uv.x,uv.y/3.0),0.0).a:0.0;
    for(int group=0;group<3;++group) {
        float shift=hudDock.x*(group==0?-hudRect.x:group==2?1.0-hudRect.x-hudRect.z:0.0);
        vec2 panel=(screenUV-hudRect.xy-vec2(shift,0))/hudRect.zw;
        float lo=group==0?0.0:group==1?136.0/320.0:202.0/320.0;
        float hi=group==0?136.0/320.0:group==1?202.0/320.0:1.0;
        if(panel.x>=lo&&panel.x<hi&&panel.y>=0.0&&panel.y<=1.0)
            coverage=max(coverage,textureLod(hud,vec2(panel.x,(1.0+panel.y)/3.0),0.0).a);
    }
    return coverage;
}
bool coversOverlay(vec2 pixel) {
    for(int i=0;i<3;++i) {
        vec4 rect=overlayRects[i];
        if(rect.z>rect.x&&rect.w>rect.y&&
           all(greaterThanEqual(pixel,rect.xy-0.5))&&all(lessThanEqual(pixel,rect.zw+0.5)))
            return true;
    }
    return false;
}
vec3 applyChromaticAberration(vec2 screenUV,vec2 sceneUV,vec3 original,float coverage) {
    vec2 size=vec2(textureSize(image,0));
    vec2 extent=sceneRect.zw-sceneRect.xy;
    if(any(lessThan(extent,vec2(1.0))))return original;
    vec2 screenPixel=screenUV*size;
    if(any(lessThan(screenPixel,sceneRect.xy))||any(greaterThanEqual(screenPixel,sceneRect.zw)))
        return original;

    vec2 halfExtent=extent*0.5;
    vec2 pixelFromCenter=screenPixel-sceneRect.xy-halfExtent;
    float distanceFromCenter=length(pixelFromCenter);
    float radius=distanceFromCenter/length(halfExtent);
    float innerRadius=mix(0.92,0.0,chromatic.y);
    float edge=smoothstep(innerRadius,chromatic.z,radius);
    if(edge<=0.0||coverage>0.0)return original;

    vec2 direction=pixelFromCenter/max(distanceFromCenter,0.0001);
    vec2 offset=direction*(chromatic.x*extent.y/1080.0)*edge;
    vec2 scenePixel=sceneUV*size;
    if(any(lessThan(scenePixel,sceneRect.xy))||any(greaterThanEqual(scenePixel,sceneRect.zw)))
        return original;
    vec2 low=sceneRect.xy+0.5,high=sceneRect.zw-0.5;
    vec2 red=clamp(scenePixel+offset,low,high);
    vec2 blue=clamp(scenePixel-offset,low,high);

    // Overlays share the scene image. Guard the destination and shifted taps.
    if(coversOverlay(screenPixel)||coversOverlay(red)||coversOverlay(blue))
        return original;
    if((coverage<0.0&&hudCoverage(screenUV)>0.0)||
       hudCoverage(red/size)>0.0||hudCoverage(blue/size)>0.0)
        return original;
    return vec3(textureLod(image,red/size,0.0).r,original.g,textureLod(image,blue/size,0.0).b);
}
void main() {
    vec2 sceneUV=vUV;
    vec4 color=textureLod(image,sceneUV,0.0);
    if(photo.x>0.5&&photo.y>0.0){
        float z=photoZ(vUV),coc=clamp(abs(z-photo.z)/max(z,1.0)*photo.y*5.0,0.0,12.0);
        vec3 sum=color.rgb;float total=1.0;
        for(int i=0;i<24;++i){float a=float(i)*2.399963,r=sqrt((float(i)+.5)/24.0);
            vec2 uv=clamp(vUV+vec2(cos(a),sin(a))*r*coc/vec2(textureSize(image,0)),vec2(0),vec2(1));
            float other=photoZ(uv);float w=other<z-max(8.0,photo.z*.08)?0.0:1.0;
            sum+=textureLod(image,uv,0).rgb*w;total+=w;
        }color.rgb=sum/total;
    }
    // Heat haze: hot-air volumes left 1-heat in alpha. Rows of Doom pixels
    // slide sideways by whole Doom pixels at the tic rate, like an old
    // raster wobble; samples outside the hot area (weapon, HUD) are kept.
    float heat=1.0-color.a;
    if(heat>0.01&&c.fx2.x>0.0) {
        vec2 size=vec2(textureSize(image,0));
        float row=floor(vUV.y*size.y/c.fx2.x),tic=floor(c.fog.w*35.0);
        float shift=round(heat*(1.4*sin(row*0.83+tic*0.61)+0.7*sin(row*0.29-tic*0.37)));
        if(shift!=0.0) {
            vec2 candidateUV=vUV+vec2(shift*c.fx2.x/size.x,0.0);
            vec4 moved=textureLod(image,candidateUV,0.0);
            if(moved.a<0.999) {
                color.rgb=moved.rgb;
                sceneUV=candidateUV;
            }
        }
    }
    bool bloomActive=c.materials.y>0.0;
    float coverage=bloomActive?hudCoverage(vUV):-1.0;
    if(chromatic.x>0.0001&&photo.x<=0.5)
        color.rgb=applyChromaticAberration(vUV,sceneUV,color.rgb,coverage);
    if(bloomActive) {
        // Screen-like additive glow retains detail without clipping to white.
        vec3 glow=textureLod(bloom,vUV,0.0).rgb*(1.0-coverage)*0.8;
         // Where much of the view glows (SIGIL's rooms walled in fire and red rock) the bloom gives way, as
        // the eye does, or it floods the view in one colour; lamps and screens in a room never come near.
        // From the bloom's last mip level, the whole view's average.
        vec3 mean=textureLod(bloom,vec2(0.5),float(textureQueryLevels(bloom)-1)).rgb;
        float most=max(mean.r,max(mean.g,mean.b));
        if(most>0.08) glow*=pow(0.08/most,0.75);
        color.rgb=1.0-(1.0-color.rgb)*exp(-glow);
    }
    if(c.materials.w>0.0) color.rgb=texelFetch(paletteLUT,ivec3(round(saturate(color.rgb)*31.0)),0).rgb;
    if(photo.x>0.5){
        float l=dot(color.rgb,vec3(.2126,.7152,.0722));
        if(photo.w==1.0)color.rgb=vec3(l);
        else if(photo.w==2.0)color.rgb=mix(color.rgb,vec3(l)*vec3(1.2,.96,.72),.65);
        else if(photo.w==3.0)color.rgb=color.rgb*vec3(.80,1.03,1.20);
        else if(photo.w==4.0)color.rgb=clamp((mix(vec3(l),color.rgb,1.35)-.5)*1.15+.5,0.0,1.0);
    }
    outColor=vec4(mix(color.rgb,vec3(1.0),shutter.x),1.0);
}
