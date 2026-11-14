#ifndef DOOM_HUD_SCREEN_POSITION_H
#define DOOM_HUD_SCREEN_POSITION_H
#include <algorithm>
namespace doom3d {
struct HudScreenPosition {float x,y,w,h;};
inline HudScreenPosition hudScreenPosition(float width,float height,float uiWidth,int panelWidth,int panelHeight,int anchor,float offsetX,float offsetY,bool topDocked){
    const float sx=uiWidth/320.f,sy=height/200.f;
    HudScreenPosition p{};p.w=std::min(width,panelWidth*sx);p.h=std::min(height,panelHeight*sy);
    float x=(anchor&1)?width-p.w-8*sx:8*sx;
    float y=(anchor>=2?158-panelHeight:topDocked?98:56)*sy;
    p.x=std::clamp(x+offsetX*width/100.f,0.f,std::max(0.f,width-p.w));
    p.y=std::clamp(y+offsetY*height/100.f,0.f,std::max(0.f,height-p.h));return p;
}
}
#endif
