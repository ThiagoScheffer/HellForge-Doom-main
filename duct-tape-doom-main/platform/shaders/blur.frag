#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
layout(set=2,binding=0) uniform sampler2D image;
layout(std140,set=3,binding=0) uniform BlurKernel { vec4 axis; vec4 weights; };
layout(location=0) out vec4 outColor;
void main() {
    vec2 inner=axis.xy*axis.z,outer=axis.xy*axis.w;
    vec3 glow=textureLod(image,vUV,0.0).rgb*weights.x;
    glow+=(textureLod(image,vUV+inner,0.0).rgb+
           textureLod(image,vUV-inner,0.0).rgb)*weights.y;
    glow+=(textureLod(image,vUV+outer,0.0).rgb+
           textureLod(image,vUV-outer,0.0).rgb)*weights.z;
    outColor=vec4(glow,1);
}
