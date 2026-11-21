#ifndef HELLFORGE_RELIGHT_REGIONS_H
#define HELLFORGE_RELIGHT_REGIONS_H
#include <algorithm>
#include <cmath>
#include <vector>
namespace doom3d {
// Immutable XY bounds only: sector heights, light values and bake texels stay
// live. Inclusive overlap matches the original wall-strip marking predicate.
class RelightRegions {
public:
    struct Box {float x0,y0,x1,y1;};
    struct Entry {Box box;int id;};
    static bool overlap(Box a,Box b) {return !(a.x1<b.x0||a.x0>b.x1||a.y1<b.y0||a.y0>b.y1);}
private:
    struct Node {Box box;int begin,end,left=-1,right=-1;};
    std::vector<Entry> entries;
    std::vector<Node> nodes;
    bool valid=true;
    static bool finite(Box b) {return std::isfinite(b.x0)&&std::isfinite(b.y0)&&std::isfinite(b.x1)&&std::isfinite(b.y1)&&b.x0<=b.x1&&b.y0<=b.y1;}
    int build(int begin,int end) {
        Box b=entries[begin].box;
        for(int i=begin+1;i<end;++i){const auto &e=entries[i].box;b={std::min(b.x0,e.x0),std::min(b.y0,e.y0),std::max(b.x1,e.x1),std::max(b.y1,e.y1)};}
        int n=(int)nodes.size();nodes.push_back({b,begin,end});
        if(end-begin>8) {
            bool x=b.x1-b.x0>=b.y1-b.y0;int middle=begin+(end-begin)/2;
            std::nth_element(entries.begin()+begin,entries.begin()+middle,entries.begin()+end,[&](const Entry &a,const Entry &b){
                return x?double(a.box.x0)+a.box.x1<double(b.box.x0)+b.box.x1:double(a.box.y0)+a.box.y1<double(b.box.y0)+b.box.y1;
            });
            int left=build(begin,middle),right=build(middle,end);nodes[n].left=left;nodes[n].right=right;
        }
        return n;
    }
    template<class Visit> void query(int n,Box b,const Visit &visit) const {
        const Node &node=nodes[n];if(!overlap(node.box,b))return;
        if(node.left<0){for(int i=node.begin;i<node.end;++i)if(overlap(entries[i].box,b))visit(entries[i].id);}
        else {query(node.left,b,visit);query(node.right,b,visit);}
    }
public:
    void reset(std::vector<Entry> input={}) {
        entries=std::move(input);nodes.clear();valid=true;
        for(const auto &e:entries)if(!finite(e.box)){valid=false;break;}
        if(valid&&!entries.empty()){nodes.reserve(entries.size()*2);build(0,(int)entries.size());}
    }
    template<class Visit> void each(Box b,const Visit &visit) const {
        if(!valid||!finite(b)){for(const auto &e:entries)if(overlap(e.box,b))visit(e.id);}
        else if(!nodes.empty())query(0,b,visit);
    }
};
}
#endif
