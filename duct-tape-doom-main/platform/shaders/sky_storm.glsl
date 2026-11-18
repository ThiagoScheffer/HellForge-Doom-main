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
    // Milestone C: wider, longer zigzags, still ending behind the skyline.
    float sway=seed*2.0-1.0;
    vec2 a=vec2(0.0,0.83),b=vec2(-0.019+0.012*sway,0.63);
    vec2 c=vec2(0.016+0.022*sway,0.44),d=vec2(-0.026+0.035*sway,0.21);
    float mainStroke=0.0;
    mainStroke=max(mainStroke,hellforgeBoltLine(p,a,b,0.0040));
    mainStroke=max(mainStroke,hellforgeBoltLine(p,b,c,0.0045));
    mainStroke=max(mainStroke,hellforgeBoltLine(p,c,d,0.0050));
    vec2 left=c+vec2(-0.084-0.013*seed,-0.142);
    vec2 right=b+vec2(0.070+0.016*seed,-0.144);
    float branches=max(hellforgeBoltLine(p,c,left,0.0028),
                       hellforgeBoltLine(p,b,right,0.0026));
    return max(mainStroke,branches*0.88);
}
float hellforgeBoltHalo(float az,float rise,float strikeAz) {
    float dx=atan(sin(az-strikeAz),cos(az-strikeAz));
    return exp(-dx*dx/0.08)*exp(-(rise-0.46)*(rise-0.46)/0.12);
}
