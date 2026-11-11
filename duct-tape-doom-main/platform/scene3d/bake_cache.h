// Numeric bake outputs only. Rebuild metadata and GPU resources normally.
#ifndef DOOM_BAKE_CACHE_H
#define DOOM_BAKE_CACHE_H
#include <initializer_list>
#include <type_traits>
namespace doom3d {
struct CacheField { void *data; size_t count,stride,width,row=0,gap=0; };
template<class T> CacheField cacheField(std::vector<T> &v) {
    static_assert(std::is_arithmetic<T>::value,"Cache fields must contain numeric data only");
    return {v.data(),v.size(),sizeof(T),sizeof(T)};
}
inline CacheField cacheChannel(std::vector<uint8_t> &v,size_t begin,size_t count,size_t width=1) {
    return {v.empty()?nullptr:v.data()+begin,count,4,width};
}
CacheField cacheWallChannel(std::vector<uint8_t> &cells,int height,int channel,size_t width,bool bounce=false);
class BakeCache {
    std::string path,stage;
    std::vector<CacheField> fields;
    uint64_t key=0,start=0;
    bool enabled=false,hit=false;
public:
    BakeCache(const char *name,std::initializer_list<CacheField> output);
    bool restore();
    void store();
#ifdef DOOM_TEST_DRIVER
    static int selfTest();
#endif
};
}
#endif
