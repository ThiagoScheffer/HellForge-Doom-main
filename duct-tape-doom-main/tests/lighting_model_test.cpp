#include "../platform/lighting_model.h"

#include <cstdio>
#include <cstdlib>

namespace {
int checks=0;

void require(bool condition,const char *message) {
    ++checks;
    if(!condition) {
        std::fprintf(stderr,"FAIL: %s\n",message);
        std::exit(EXIT_FAILURE);
    }
}

bool near(float actual,float expected) {
    return std::abs(actual-expected)<=.00001f;
}
}

int main() {
    using namespace doom_lighting_model;
    require(validatedModel(0)==Enhanced,"Enhanced model");
    require(validatedModel(1)==Hybrid,"Hybrid model");
    require(validatedModel(2)==Physical,"Physical model");
    require(validatedModel(-1)==Enhanced&&validatedModel(99)==Enhanced,"Unknown model fallback");
    for(int model:{Hybrid,Physical}) {
        require(effectiveModel(model,true,true,true)==model,"All prerequisites met");
        require(effectiveModel(model,false,true,true)==Enhanced,"Software renderer fallback");
        require(effectiveModel(model,true,false,true)==Enhanced,"Static bake disabled fallback");
        require(effectiveModel(model,true,true,false)==Enhanced,"Emissive disabled fallback");
    }
    require(near(interiorAmbient(0.f,Enhanced),.12f),"Legacy ambient floor");
    require(near(interiorAmbient(.5f,Enhanced),.5f),"Legacy authored ambient");
    require(interiorAmbient(0.f,Hybrid)==0.f&&interiorAmbient(0.f,Physical)==0.f,"Zero ambient stays zero");
    require(interiorAmbient(-1.f,Physical)==0.f,"Negative ambient clamped");
    require(near(interiorAmbient(5.f,Physical),.05f),"Excess ambient clamped");
    require(near(interiorAmbient(1.f,Hybrid),.25f),"Hybrid maximum ambient");
    require(near(interiorAmbient(1.f,Physical),.05f),"Physical maximum ambient");
    constexpr float authored=160.f/255.f;
    require(near(interiorAmbient(authored,Hybrid),.15686275f),"Representative Hybrid ambient");
    require(near(interiorAmbient(authored,Physical),.03137255f),"Representative Physical ambient");

    require(near(legacyFixtureOutput(0),.08f)&&near(legacyFixtureOutput(96),.08f),"Legacy dim fixture");
    require(near(legacyFixtureOutput(152),.31f),"Legacy fixture midpoint");
    require(near(legacyFixtureOutput(208),1.f),"Legacy full fixture output");
    require(near(fixtureOutput(96,Hybrid),.54f),"Hybrid dim fixture");
    require(near(fixtureOutput(152,Hybrid),.655f),"Hybrid fixture midpoint");

    for(int level=0;level<=255;++level) {
        const float legacy=fixtureOutput(level,Enhanced),hybrid=fixtureOutput(level,Hybrid);
        require(near(legacy,legacyFixtureOutput(level)),"Enhanced fixture unchanged");
        require(hybrid>=legacy&&hybrid<=1.f,"Hybrid fixture bounds");
        require(fixtureOutput(level,Physical)==1.f,"Physical fixture independent of sector level");
        const float normalized=level/255.f;
        for(int model:{Hybrid,Physical}) {
            const float ambient=interiorAmbient(normalized,model);
            require(ambient>=0.f&&ambient<=(model==Hybrid?.25f:.05f),"Source-driven ambient bounds");
            require(ambient>=interiorAmbient((level-1)/255.f,model),"Source-driven ambient monotonic");
        }
    }
    require(near(quantizedExposure(10.f,Enhanced),1.5f),"Enhanced exposure cap");
    require(near(quantizedExposure(10.f,Hybrid),1.25f),"Hybrid exposure cap");
    require(near(quantizedExposure(10.f,Physical),1.f),"Physical exposure cap");
    for(int model:{Enhanced,Hybrid,Physical}) {
        require(near(quantizedExposure(1.f,model),1.f),"Neutral exposure");
        require(near(quantizedExposure(0.f,model),.625f),"Finite quantized lower bound");
        require(near(quantizedExposure(.9f,model),.875f),"Exposure steps in sixteenths");
    }
    std::printf("PASS: lighting model, %d checks\n",checks);
    return EXIT_SUCCESS;
}
