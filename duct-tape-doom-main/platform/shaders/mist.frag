#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
layout(set=2,binding=0) uniform sampler2D cloud;
layout(set=2,binding=1) uniform sampler2D depth;
layout(std140,set=3,binding=0) uniform Camera CAMERA_BLOCK c;
layout(location=0) out vec4 outColor;
float heightFogTau(float oz,float pz,float distance,float density,float falloff,float reference) {
    float x=falloff*abs(pz-oz);
    float f=x<1e-3?1.0-x*0.5+x*x/6.0:(1.0-exp(-x))/x;
    return density*distance*exp(clamp(-falloff*(min(oz,pz)-reference),-80.0,80.0))*f;
}
void main() {
    float d=texelFetch(depth,ivec2(gl_FragCoord.xy),0).r;
    float sceneZ=c.projection.z*c.projection.w/(c.projection.w-d*(c.projection.w-c.projection.z));
    float layerZ=dot(vWorld-c.eye.xyz,c.forward.xyz);
    vec3 delta=vWorld-c.eye.xyz;
    float distance=length(delta);
    float transmission=exp(-heightFogTau(c.eye.z,vWorld.z,distance,c.fog.x,c.fog.y,c.fog.z));
    if((vMode&MODE_SUNBEAM)!=0u) {
        // Smooth, continuous sunbeam ribbon (buildShafts). UV.x spans the
        // width (-1..1); UV.y runs from the floor (0) past the opening (1).
        // Keep all attenuation in floating point: the former 1/32 alpha
        // quantization + Bayer dither made broad shafts visibly checkerboard.
        float across=1.0-smoothstep(0.12,1.0,abs(vUV.x));
        float along=smoothstep(0.0,0.16,vUV.y)*(1.0-smoothstep(0.60,1.0,vUV.y));
        // Fade smoothly into nearby geometry using the already-bound scene
        // depth; no extra depth pass or light/shadow query is necessary.
        float near=smoothstep(16.0,80.0,distance)*smoothstep(0.0,36.0,sceneZ-layerZ);
        // Zero alpha adds no light and the shaft pipeline has no depth target.
        if(across<=0.0||along<=0.0||near<=0.0)discard;
        // Animate continuously at the prior average speed (35 tics/s *
        // 0.35 world units/tic), rather than jumping on whole game tics.
        float noise=textureLod(cloud,
            vec2(dot(vWorld.xy,vec2(0.7071)),vWorld.z-c.fog.w*12.25)/112.0,0.0).r;
        // Restrained density variation keeps shafts soft without introducing
        // striped/scattered highlights. The average brightness is preserved.
        float glow=vLight*across*along*near*(0.8+0.4*noise)*0.05;
        outColor=vec4(vTint*c.fx.x*transmission,saturate(glow));
        return;
    }
    float soft=saturate((sceneZ-layerZ)/8.0);
    // Suppress sheets edge-on and soften the camera crossing a layer.
    soft*=smoothstep(0.025,0.18,abs(delta.z)/max(distance,0.001));
    soft*=smoothstep(0.0,3.0,abs(delta.z));
    // Zero alpha preserves both color and alpha in the mist blend.
    if(soft<=0.0)discard;
    vec2 uv=vWorld.xy/160.0+vec2(c.fog.w*0.009,c.fog.w*0.005)+vUV;
    float noise=textureLod(cloud,uv,0.0).r;
    float alpha=vLight*(0.25+noise*0.75)*soft;
    outColor=vec4(mix(fogHaze(c.fx.yzw),vTint,transmission)*c.fx.x,alpha);
}
