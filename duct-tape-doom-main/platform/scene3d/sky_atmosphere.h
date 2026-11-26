#ifndef DOOM_SKY_ATMOSPHERE_H
#define DOOM_SKY_ATMOSPHERE_H

#include <array>
#include <cstdint>

namespace doom3d {

struct Image;

enum class SkyAtmosphereKind : uint8_t { Legacy=0, DuskStorm=1, BurningCityDusk=2, InfernalAbyss=3, Count };

struct SkyAtmosphereProfile {
    SkyAtmosphereKind kind=SkyAtmosphereKind::Legacy;
    std::array<float,3> horizon{},zenith{},cloudDark{},cloudMiddle{},cloudLight{};
    float coverage=0;
    float nearAltitude=2300,nearScale=7200,farScale=1;
    float nearOpacity=.82f,farOpacity=.64f,windSpeed=55;
};

// A Doom indexed image stores a palette index and a coverage byte per pixel.
SkyAtmosphereProfile analyzeSkyAtmosphere(const uint8_t *indexed,int width,int height,
                                          const uint8_t *playpal,SkyAtmosphereKind kind);
SkyAtmosphereProfile analyzeMaskedSkyAtmosphere(const uint8_t *indexed,int width,int height,
                                                const uint8_t *playpal,const uint8_t *skyMask,
                                                SkyAtmosphereKind kind);
const SkyAtmosphereProfile *skyAtmosphereProfile(const Image &sky);
extern std::array<float,2> skyAtmosphereHeading;

} // namespace doom3d

#endif
