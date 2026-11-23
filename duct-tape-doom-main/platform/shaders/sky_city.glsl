// Deep city metadata: above the cut, B marks distant painted facades and A
// holds fire influence. Below it, B is facade depth and A is window strength.
vec2 deepCityFire(float longitude,float source,float clouds,float altitude) {
    float t=c.fog.w;
    float quarter=longitude*25.132741;
    float flicker=.75+.14*sin(quarter*3.0+t*2.6)+.08*sin(quarter*7.0-t*1.7);
    // One short flare every few time windows, somewhere in the panorama
    // with visible fire paint. A smooth attack and decay prevent popping.
    float cycle=floor(t/24.0);
    float phase=t-cycle*24.0;
    float eventSeed=fract(sin(cycle*7.17+17.8)*4139.21);
    float direction=fract(eventSeed*13.17);
    float distance=abs(fract(longitude-direction+.5)-.5);
    float pulse=smoothstep(.0,.22,phase)*(1.0-smoothstep(.22,2.2,phase));
    float flare=step(.67,eventSeed)*pulse*(1.0-smoothstep(.012,.061,distance));
    float heat=source*flicker;
    return vec2(heat,flare*source*(.28+.72*clouds)*(1.0-smoothstep(.29,.63,altitude)));
}

vec3 deepCityFacade(vec3 facade,vec2 at,float distance,float window,float smoke,float rise) {
    float haze=smoothstep(.25,.87,distance);
    float low=1.0-smoothstep(.16,.58,rise);
    // The lower tiers are seen through intervening smoke. Tall towers
    // keep their original roofs and dark sides in front of these layers.
    float extinction=haze*(.26+.36*smoke)*(.70+.30*low);
    facade=mix(facade,vec3(.34,.249,.179),extinction);
    if(window>0.001) {
        vec2 pane=floor(at/vec2(2.0,2.0));
        float phase=fract(sin(dot(pane,vec2(31.17,11.23)))*4129.28);
        float flicker=sin(c.fog.w*(.6+phase*1.9)+phase*6.2831853);
        facade+=vec3(.135,.065,.020)*window*flicker*(1.0-.55*haze);
    }
    return max(facade,vec3(0.0));
}
