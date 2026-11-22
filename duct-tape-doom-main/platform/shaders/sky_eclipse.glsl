vec3 bloodEclipseCenter(bool bloodHellRidge) {
    return bloodHellRidge?vec3(0.566719,-0.725368,0.390731):vec3(0.874620,0.0,0.484810);
}
vec3 hellBloodEclipse(vec3 ray,vec3 sky,bool bloodHellRidge) {
    // M1 uses east at 29 degrees. On layered Blood Hell artwork, use a
    // fixed heading over the painted central ridge to show the depth.
    vec3 center=bloodEclipseCenter(bloodHellRidge);
    vec3 up=bloodHellRidge?vec3(-0.240558,0.307900,0.920505):vec3(-0.484810,0.0,0.874620);
    vec3 right=bloodHellRidge?vec3(0.788011,0.615661,0.0):vec3(0.0,1.0,0.0);
    const float radius=0.131; // Chord radius, approximately 15 degrees across.
    float distanceSquared=2.0-2.0*dot(ray,center);
    // Evaluate derivatives before the spatial cutoff.
    float edge=clamp(fwidth(distanceSquared)/(2.0*radius),0.0006,0.004);
    if(distanceSquared>=0.23*0.23)return sky;

    float distance=sqrt(max(0.0,distanceSquared));
    float offset=distance-radius;
    float disc=1.0-smoothstep(-edge,edge,offset);
    float limb=smoothstep(-0.024,0.0,offset);
    vec3 result=mix(sky,vec3(0.006,0.002,0.003)+vec3(0.028,0.004,0.003)*limb,disc);
    if(offset < -0.009)return result;

    // Integer angular frequencies meet across the angle wrap. Independent,
    // slow phase changes give the rim motion without rotating the disc.
    float angle=atan(dot(ray,up),dot(ray,right));
    float time=c.fog.w;
    float uneven=sin(5.0*angle+0.35*sin(time*0.21))+0.45*sin(13.0*angle-time*0.18);
    float filament=pow(max(0.0,sin(9.0*angle+0.8*sin(4.0*angle)+time*0.15)),5.0);
    float rim=1.0-smoothstep(0.001,0.008+0.002*uneven,abs(offset-0.002));
    float wisps=(1.0-smoothstep(0.004,0.025+0.011*filament,offset))*filament;
    float haze=1.0-smoothstep(0.006,0.23-radius,offset);
    float flicker=0.90+0.06*sin(3.0*angle+time*0.47)+0.04*sin(7.0*angle-time*0.31);
    vec3 corona=vec3(0.76,0.18,0.064)*rim*(0.78+0.13*uneven)*flicker;
    corona+=vec3(0.24,0.040,0.016)*wisps+vec3(0.12,0.019,0.021)*haze*haze;
    return result+corona*(1.0-disc);
}
