#ifndef DOOM_BAKE_CACHE_KEY_H
#define DOOM_BAKE_CACHE_KEY_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>
#include <type_traits>

namespace doom3d {
// Canonical identity fields are little-endian. Native payload bytes have a
// separate compatibility domain and retain the existing storage format.
class BakeKeyHash {
    uint64_t digest=14695981039346656037ull;

    template<class T> void integer(T number) {
        for(size_t i=0;i<sizeof(number);++i) {
            byte(static_cast<uint8_t>(number));
            number>>=8;
        }
    }
public:
    void byte(uint8_t value) {digest=(digest^value)*1099511628211ull;}
    void bytes(const void *data,size_t size) {
        auto p=static_cast<const uint8_t*>(data);
        for(size_t i=0;i<size;++i)byte(p[i]);
    }
    void blob(const void *data,size_t size) {length(size);bytes(data,size);}
    void text(std::string_view value) {blob(value.data(),value.size());}
    void length(size_t size) {integer(uint64_t(size));}
    void component(uint64_t value) {integer(value);}
    void value(bool flag) {byte(flag?1:0);}
    template<class T,std::enable_if_t<std::is_integral_v<T>&&!std::is_same_v<T,bool>,int> =0>
    void value(T number) {
        static_assert(sizeof(T)<=4,"Use an explicit 64-bit field");
        integer(uint32_t(number));
    }
    void value(float number) {
        static_assert(sizeof(float)==4&&std::numeric_limits<float>::is_iec559);
        uint32_t bits;
        std::memcpy(&bits,&number,sizeof(bits));
        integer(bits);
    }
    template<class T,size_t Count> void value(const T (&items)[Count]) {
        for(const auto &item:items)value(item);
    }
    template<class T,size_t Count> void value(const std::array<T,Count> &items) {
        for(const auto &item:items)value(item);
    }
    template<class... T> void fields(const T &...items) {(value(items),...);}
    uint64_t result() const {return digest;}
};
}
#endif
