// Numeric bake outputs only. Rebuild metadata and GPU resources normally.
#ifndef DOOM_BAKE_CACHE_H
#define DOOM_BAKE_CACHE_H
#include "bake_cache_key.h"
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>
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
void beginBakeCacheSession(bool level);
void hashGlassBakeInputs(BakeKeyHash &hash);
void endBakeCacheSession(uint64_t preparationStart,double mapMs);
void recordBakeGeometry(uint64_t start);
void recordBakeReconcile(uint64_t start);
uint64_t bakeUpstreamIdentity(bool sunlight);
class BakeCache {
    std::string path,stage;
    std::vector<CacheField> fields;
    uint64_t key=0,expected=0,stored=0,checksum=0,outputIdentity=0;
    uint64_t geometry=0,atlas=0,sources=0,inputs=0,layout=0;
    const char *algorithm="none";
    uint64_t clock=0,dispatches=0;
    double keyMs=0,lookupMs=0,readMs=0,calculateMs=0,writeMs=0,uploadMs=0;
    const char *readResult="NOT_ATTEMPTED",*writeResult="NOT_ATTEMPTED";
    bool enabled=false,hit=false,calculated=false,uploaded=false,hasOutputIdentity=false;
    std::vector<uint8_t> verification;
    bool verifying=false;
    size_t session=0;
    bool readEntry();
    uint64_t payloadChecksum(const std::vector<uint8_t> &data,uint64_t &upstream) const;
public:
    BakeCache(const char *name,std::initializer_list<CacheField> output,
        const std::function<void(BakeKeyHash &)> &inputs={});
    ~BakeCache();
    bool restore();
    void dispatch(int count) {dispatches+=count>0;}
    void beginUpload();
    void endUpload();
    const char *outputSource() const {return verifying?(hit?"verified":"reference calculated"):(hit?"restored":"calculated");}
    void store();
#ifdef DOOM_TEST_DRIVER
    static int selfTest();
#endif
};
}
#endif
