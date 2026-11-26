#include "../platform/corpse_decomposition.h"
#include <cstdio>
#include <cstdlib>

static void check(bool ok,const char *label) {
    if(ok)return;
    std::fprintf(stderr,"Corpse decay: %s\n",label);
    std::exit(1);
}

int main() {
    constexpr int rate=35,width=16,height=16;
    for(int delay:{2,5,8,10}) {
        const int first=delay*60*rate;
        check(doom_decay::stage(first-1,delay,rate)==0,"fresh before the preset");
        check(doom_decay::stage(first,delay,rate)==1,"stage 1 at the preset");
        check(doom_decay::stage(first+45*rate,delay,rate)==2,"stage 2 at +45 seconds");
        check(doom_decay::stage(first+120*rate,delay,rate)==3,"stage 3 at +120 seconds");
    }
    std::array<unsigned char,768> palette{};
    std::vector<unsigned char> original(width*height*2);
    for(unsigned i=0;i<256;++i) {
        palette[3*i]=static_cast<unsigned char>(i);
        palette[3*i+1]=static_cast<unsigned char>(i*47);
        palette[3*i+2]=static_cast<unsigned char>(i*79);
        original[2*i]=static_cast<unsigned char>(i);
        original[2*i+1]=i%7==0?0:i%3==0?150:255;
    }
    auto unchanged=original;
    std::vector<unsigned char> previousFamily;
    for(int family=0;family<5;++family) {
        std::vector<unsigned char> direct,progressive,previous=original;
        check(doom_decay::paint(direct,original,width,height,palette.data(),3,family,43),"direct stage 3");
        for(int stage=1;stage<=3;++stage) {
            check(doom_decay::paint(progressive,original,width,height,palette.data(),stage,family,43),
                  "paint stage from fresh pixels");
            check(progressive.size()==original.size()&&progressive!=previous,"stage extent and color change");
            for(size_t i=0;i<original.size();i+=2) {
                check(original[i+1]==progressive[i+1],"alpha and silhouette preserved");
                if(!original[i+1])check(original[i]==progressive[i],"transparent index preserved");
            }
            previous=progressive;
        }
        check(direct==progressive&&original==unchanged,"direct and progressive results agree");
        check(progressive!=previousFamily,"families retain distinct colors");
        previousFamily=progressive;
    }
    check(!doom_decay::paint(unchanged,original,257,height,palette.data(),1,0,43),"oversized art rejected");
    std::puts("PASS corpse decay timing and immutable indexed artwork");
}
