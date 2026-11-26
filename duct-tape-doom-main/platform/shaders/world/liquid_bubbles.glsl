// One stateless candidate per world-space cell, with a gap between cycles.
// Uses surface relief and the original liquid color.
LivingShading liquidBubbleSurface(uint kind,uint flags,vec2 worldXY,float distanceToEye,
                                 float footprint,float time,inout vec3 rgb) {
    LivingShading result=LivingShading(vec2(0),0.0,0.0,0.0,1.0);
    if(kind!=LS_LIQUID_ACID&&kind!=LS_LIQUID_LAVA)return result;

    float detail=(1.0-smoothstep(240.0,640.0,distanceToEye))*
                 (1.0-smoothstep(1.0,3.0,footprint));
    if(detail<=0.0)return result;

    bool lava=kind==LS_LIQUID_LAVA;
    float cellSize=lava?32.0:24.0;
    vec2 grid=worldXY/cellSize;
    ivec2 cell=ivec2(floor(grid));
    uint seed=tileHash(cell,lava?0x98b7a251u:0x32c67a9du);
    float cadence=float(seed&255u)/255.0;
    float period=lava?mix(3.0,6.0,cadence):mix(1.8,3.6,cadence);
    float clock=max(time,0.0)/period+float((seed>>8u)&65535u)/65536.0;
    float phase=fract(clock);
    if(phase<=0.06||phase>=0.88)return result;

    uint h=tileHash(cell,seed^uint(floor(clock)));
    if(float(h>>24u)/255.0>(lava?0.68:0.82))return result;
    float radiusRoll=float((h>>16u)&255u)/255.0;
    float maxRadius=lava?mix(2.0,5.0,radiusRoll):mix(1.25,3.0,radiusRoll);
    float resolution=1.0-smoothstep(maxRadius*0.35,maxRadius*0.95,footprint);
    if(resolution<=0.0)return result;

    vec2 center=mix(vec2(0.35),vec2(0.65),vec2(float(h&255u),float((h>>8u)&255u))/255.0);
    vec2 delta=(fract(grid)-center)*cellSize;
    float aa=clamp(footprint*0.5,0.12,0.65);
    // A ripple reaches 1.60 radii plus its 0.12-radius skirt and AA.
    // At maximum size this is 9.25 units, inside lava's 11.2-unit margin.
    float extent=maxRadius*1.72+aa;
    if(dot(delta,delta)>=extent*extent)return result;

    uint intensity=clamp((flags>>LS_INTENSITY_SHIFT)&LS_INTENSITY_MASK,1u,3u);
    float strength=detail*resolution*mix(0.55,1.0,float(intensity-1u)*0.5);
    float dist=length(delta);
    float growth=smoothstep(0.06,0.40,phase);
    float collapse=1.0-smoothstep(0.52,0.61,phase);
    float domeLife=growth*collapse;
    float dome=0.0,rim=0.0,rupture=0.0,ripple=0.0;
    vec2 worldSlope=vec2(0);

    if(domeLife>0.0) {
        float radius=max(0.35,maxRadius*mix(0.18,1.0,growth)*collapse);
        vec2 q=delta/radius;
        float height=max(0.0,1.0-dot(q,q));
        float coverage=1.0-smoothstep(radius-aa,radius+aa,dist);
        dome=height*height*coverage*domeLife;
        // Analytic slope of the rounded dome on the world XY plane.
        float heightRatio=lava?0.30:0.40;
        worldSlope=-4.0*heightRatio*q*height*coverage*domeLife;
        rim=(1.0-smoothstep(aa*0.35,aa+radius*0.15,abs(dist-radius*0.82)))*domeLife;
    }

    float ruptureLife=smoothstep(0.52,0.56,phase)*(1.0-smoothstep(0.58,0.67,phase));
    float ringWidth=aa+maxRadius*0.12;
    if(ruptureLife>0.0)
        rupture=(1.0-smoothstep(aa*0.35,ringWidth,abs(dist-maxRadius)))*ruptureLife;

    if(phase>=0.56) {
        float age=(phase-0.56)/0.32;
        float offset=dist-maxRadius*mix(0.85,1.60,age);
        float rippleLife=smoothstep(0.0,0.12,age)*(1.0-smoothstep(0.20,1.0,age));
        ripple=(1.0-smoothstep(aa*0.35,ringWidth,abs(offset)))*rippleLife;
        float radial=clamp(offset/max(maxRadius*0.18,aa),-1.0,1.0);
        worldSlope+=delta/max(dist,0.001)*(radial*ripple*0.12);
    }

    float darkening=dome*(lava?0.24:0.17);
    float highlight=rim*(lava?0.10:0.13)+rupture*(lava?0.24:0.14)+ripple*(lava?0.07:0.10);
    vec3 response=lava?vec3(1.0,0.75,0.40):vec3(0.75,1.0,0.65);
    rgb*=vec3(1.0)+strength*(response*highlight-darkening);

    result.slope=clamp(worldSlope*strength,vec2(-0.65),vec2(0.65));
    result.gloss=lava?0.20:0.48;
    result.sharpness=lava?22.0:40.0;
    result.finish=clamp(max(dome,max(rim,max(rupture,ripple)*0.5))*strength,0.0,1.0);
    return result;
}
