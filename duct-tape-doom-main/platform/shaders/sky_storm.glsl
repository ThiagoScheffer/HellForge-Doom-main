// HELLFORGE Storm Milestone B — analytic, deterministic distant sky lightning.
// Uses existing Camera.style.yzw (azimuth, bolt intensity, branch seed).
// No mesh, bolt texture, global shader uniform, gameplay actor or RNG draws.
// Angles use the world-space sky cylinder so strikes do not track camera yaw.
float hellforgeBoltLine(vec2 p,vec2 a,vec2 b,float radius) {
    vec2 d=b-a;
    float t=clamp(dot(p-a,d)/max(dot(d,d),0.000001),0.0,1.0);
    return 1.0-smoothstep(radius,radius*2.6,length(p-(a+t*d)));
}
float hellforgeBoltMask(float az,float rise,float strikeAz,float seed) {
    // Wrap into [-PI, PI] so a strike crosses the 360-degree sky seam.
    float dx=atan(sin(az-strikeAz),cos(az-strikeAz));
    vec2 p=vec2(dx,rise);
    // Short forked path, terminating above the majority of horizon ridges.
    float sway=seed*2.0-1.0;
    vec2 a=vec2(0.0,0.70),b=vec2(-0.014+0.010*sway,0.55);
    vec2 c=vec2(0.009+0.018*sway,0.41),d=vec2(-0.016+0.026*sway,0.31);
    float mainStroke=0.0;
    mainStroke=max(mainStroke,hellforgeBoltLine(p,a,b,0.0020));
    mainStroke=max(mainStroke,hellforgeBoltLine(p,b,c,0.0025));
    mainStroke=max(mainStroke,hellforgeBoltLine(p,c,d,0.0030));
    vec2 left=c+vec2(-0.060-0.010*seed,-0.125);
    vec2 right=b+vec2(0.047+0.012*seed,-0.110);
    float branches=max(hellforgeBoltLine(p,c,left,0.0017),
                       hellforgeBoltLine(p,b,right,0.0015));
    return max(mainStroke,branches*0.78);
}
float hellforgeBoltHalo(float az,float rise,float strikeAz) {
    float dx=atan(sin(az-strikeAz),cos(az-strikeAz));
    return exp(-dx*dx/0.08)*exp(-(rise-0.46)*(rise-0.46)/0.12);
}
