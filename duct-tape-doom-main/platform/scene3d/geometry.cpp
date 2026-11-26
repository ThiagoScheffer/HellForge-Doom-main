#include "../monster_profile.h"
/* The visible world each frame: leaf visibility, walls, flats and things
   batched per texture. */
#include "internal.h"
#include "../decoration_ambience_rules.h"
#include "frame_workers.h"
#include "../sprite_frustum.h"
#include "../sprite_visibility.h"
#include "../world_frustum.h"
#include "p_digwall.h"
#include "../../../scene.h"
#include "../../../map_world.h"
#include "../../../skin_partition.h"
extern "C" int director_view_active;
extern const lab::Scene *SV_Scene();
extern const lab::MapWorld *SV_MapWorld();
extern float SV_DoorRenderZ(float fraction);

namespace doom3d
{
static float floorOverhang(const mobj_t*,int,const Image&);
    void reportStaticSamples();
    static doom_world_frustum::Frustum worldFrustum;
    static bool cullWorld=false;
    static unsigned processedLeaves=0,rejectedLeaves=0,processedLines=0,rejectedLines=0;

    bool customGunPuff(const mobj_t *puff);
    void quad(std::vector<Vertex> &out, Vertex a, Vertex b, Vertex c, Vertex d)
    {
        out.insert(out.end(), {a, b, c, a, c, d});
    }
#include "dig_mesh.inc"
#include "dig_wall_mesh.inc"
#include "dig_flat_mesh.inc"
#include "solid_mesh.inc"
#include "solid_map_mesh.inc"
    static void addWall(const line_t &line, int side, Point a, Point b, float bottom, float top, int tex, float anchor, float light, bool facing, int span)
    {
        wallLights(line, side, a, b, bottom, top, tex, anchor, span);
        if (!facing)
            return;
        if (top <= bottom)
            return;
        if (tex <= 0)
        {
            // Some classic maps omit a required upper/lower texture and rely on
            // software flat filling. Seal that solid span with its backing flat.
            const sector_t *back = side ? line.frontsector : line.backsector;
            if (!back || (span != 1 && span != 2))
                return;
            int flat = span == 1 ? back->ceilingpic : back->floorpic;
            if (flat == skyflatnum)
                return;
            tex = -1 - (firstflat + flattranslation[flat]);
            lumpImage(-1 - tex, true);
        }
        else
        {
            tex = texturetranslation[tex];
            wallImage(tex);
        }
        bool translucent = false;
#ifdef DOOM_BOOM_SECTORS
        translucent = span == 3 && line.tranlump && tex >= 0;
        if (translucent)
            tex = translucentWallImage(tex);
#endif
        const Image &material = tex < 0 ? lumpImage(-1 - tex, true) : wallImage(tex);
        float u = units(sides[line.sidenum[side]].textureoffset), len = std::hypot(b.x - a.x, b.y - a.y);
        if (addExcavatedWall(line, side, span, a, b, bottom, top, tex, anchor, light))
            return;
        std::vector<Vertex> *destination = &batches[tex];
        if (translucent)
        {
            float x = (a.x + b.x) * 0.5f - surfaceEye[0], y = (a.y + b.y) * 0.5f - surfaceEye[1];
            translucentWorld.push_back({tex, x * x + y * y, {}});
            destination = &translucentWorld.back().vertices;
        }
        auto &out = *destination;
        unsigned mode = sectorMode(sides[line.sidenum[side]].sector);
        if (material.leaded && settings.emissive)
            mode |= MODE_STAINED_GLASS;
        size_t first = out.size();
        Vertex corners[4] = {{a.x, a.y, top, u, anchor - top, light, mode}, {b.x, b.y, top, u + len, anchor - top, light, mode}, {b.x, b.y, bottom, u + len, anchor - bottom, light, mode}, {a.x, a.y, bottom, u, anchor - bottom, light, mode}};
        // Wall coordinates restart on every line, so each section seeds its own tiles.
        unsigned seed = (unsigned)(&line - lines) * 8u + (unsigned)side * 4u + (unsigned)span + 1u;
        if (!addBrokenWall(out, line, side, span, material, a, b, bottom, top, u, anchor, light, mode, seed))
        {
            for (int n = 0; n < 4; ++n)
            {
                bakeWallCoordinates(line, side, n == 1 || n == 2 ? len : 0, corners[n], true);
                corners[n].statics = seed;
            }
            quad(out, corners[0], corners[1], corners[2], corners[3]);
        }
        lampGlow(out, first, sides[line.sidenum[side]].sector, wallImage(tex));
    }
    // The mirrored pass draws the world a second time, so it runs only while a
    // reflective floor can be on screen. Doom's own visibility answers that: the
    // BSP front to back from the eye, one-sided walls and closed doors covering
    // the view angles behind them until the whole view is covered. Walls are full
    // height and the sky hides what lies past it, so the map's plan is enough; a
    // closed door under a sky ceiling still counts as open, since sky ceilings
    // rise above their walls (skyTop). Angles run from the view direction.
    struct ViewCover
    {
        std::vector<std::pair<float, float>> spans; // Sorted, merged.
        // A span from lo across width radians, split where it wraps past behind.
        template <class F>
        static bool parts(float lo, float width, F &&f)
        {
            float hi = lo + width;
            if (hi <= doomPi)
                return f(lo, hi);
            return f(lo, (float)doomPi) && f((float)-doomPi, hi - 2 * (float)doomPi);
        }
        bool hidden(float lo, float width) const
        {
            return parts(lo, width, [&](float a, float b)
                         {
            for(const auto &s:spans) if(s.first<=a&&s.second>=b) return true;
            return false; });
        }
        void cover(float lo, float width)
        {
            parts(lo, width, [&](float a, float b)
                  {
            spans.push_back({a,b});std::sort(spans.begin(),spans.end());
            size_t count=0;
            for(size_t i=0;i<spans.size();++i) {
                auto s=spans[i];
                if(count&&s.first<=spans[count-1].second)spans[count-1].second=std::max(spans[count-1].second,s.second);
                else spans[count++]=s;
            }
            spans.resize(count);return true; });
        }
        bool full() const { return spans.size() == 1 && spans[0].first <= -doomPi && spans[0].second >= doomPi; }
    };
    // Which of the candidate leaves the eye can see (visible[leaf] set).
    static void visibleLeaves(const Uniforms &camera, const std::vector<char> &candidate, std::vector<char> &visible, bool mapLines = false)
    {
        float ex = camera.eye[0], ey = camera.eye[1];
        float yaw = std::atan2(camera.right[0], -camera.right[1]);
        auto relative = [&](float x, float y)
        { return std::remainder(std::atan2(y - ey, x - ex) - yaw, 2 * (float)doomPi); };
        // Looking up or down widens the view's footprint on the map: the frustum's
        // corners lean back toward the eye, and once its lower or upper edge passes
        // straight down or up the footprint is the whole circle.
        float tanH = 1 / camera.projection[0], tanV = 1 / camera.projection[1];
        float flat = std::hypot(camera.forward[0], camera.forward[1]), reach = flat - tanV * std::abs(camera.forward[2]);
        float half = reach > 0.05f ? std::min((float)doomPi, std::atan2(tanH, reach) + 0.05f) : (float)doomPi;
        static ViewCover view;
        view.spans.clear();
        if (half < doomPi)
        {
            view.spans = {{-(float)doomPi, -half}, {half, (float)doomPi}};
        }
        // A convex shape's angles from the eye, a little wider so a wall's end
        // never quite hides a floor running up to it; false when the eye is inside.
        auto extent = [&](const Point *points, size_t count, float &lo, float &width)
        {
            float base = relative(points[0].x, points[0].y), low = 0, high = 0;
            for (size_t i = 1; i < count; ++i)
            {
                float d = std::remainder(relative(points[i].x, points[i].y) - base, 2 * (float)doomPi);
                low = std::min(low, d);
                high = std::max(high, d);
            }
            if (high - low >= doomPi - 0.01f)
                return false;
            lo = std::remainder(base + low - 0.01f, 2 * (float)doomPi);
            width = high - low + 0.02f;
            return true;
        };
        auto seen = [&](const Point *points, size_t count)
        {
            float lo, width;
            return !extent(points, count, lo, width) || !view.hidden(lo, width);
        };
        std::function<void(unsigned)> walk = [&](unsigned node)
        {
            if (view.full())
                return;
            if (node & NF_SUBSECTOR)
            {
                unsigned leaf = node & ~NF_SUBSECTOR;
                if (leaf >= (unsigned)numsubsectors)
                    return;
                const auto &poly = floors[leaf];
                if (!candidate.empty() && candidate[leaf] && poly.size() >= 3 && seen(poly.data(), poly.size()))
                    visible[leaf] = 1;
                const subsector_t &sub = subsectors[leaf];
                for (int i = 0; i < sub.numlines; ++i)
                {
                    const seg_t &seg = segs[sub.firstline + i];
                    const sector_t *front = seg.frontsector, *back = seg.backsector;
                    bool solid = !back || (front->ceilingpic != skyflatnum && back->ceilingpic != skyflatnum &&
                                           (ceilZ(back) <= floorZ(front) || floorZ(back) >= ceilZ(front)));
                    float a1 = relative(units(seg.v1->x), units(seg.v1->y)), a2 = relative(units(seg.v2->x), units(seg.v2->y));
                    float span = std::remainder(a1 - a2, 2 * (float)doomPi);
                    if (span > 0)
                    {
                        if (mapLines && !view.hidden(a2, span))
                            seg.linedef->flags |= ML_MAPPED;
                        if (solid && !P_GlobalDigBreachFace(int(seg.linedef - lines), seg.sidedef == &sides[seg.linedef->sidenum[0]] ? 0 : 1))
                            view.cover(a2, span);
                    }
                }
                return;
            }
            if (node >= (unsigned)numnodes)
                return;
            const node_t &n = nodes[node];
            float px = ex - units(n.x), py = ey - units(n.y);
            int side = units(n.dy) * px - units(n.dx) * py > 0 ? 0 : 1;
            walk(n.children[side]);
            // The far side only if some of its box is still in view.
            const fixed_t *box = n.bbox[side ^ 1];
            Point corners[4] = {{units(box[BOXLEFT]), units(box[BOXTOP])}, {units(box[BOXRIGHT]), units(box[BOXTOP])}, {units(box[BOXRIGHT]), units(box[BOXBOTTOM])}, {units(box[BOXLEFT]), units(box[BOXBOTTOM])}};
            bool inside = ex >= corners[0].x && ex <= corners[1].x && ey >= corners[2].y && ey <= corners[0].y;
            if (inside || seen(corners, 4))
                walk(n.children[side ^ 1]);
        };
        walk(numnodes ? unsigned(numnodes - 1) : unsigned(NF_SUBSECTOR));
    }
    // The nearest reflective liquid below the eye that it can see sets this
    // frame's mirror plane (visibleLeaves).
    static void findReflection(const Uniforms &camera)
    {
        reflectionActive = false;
        if (!settings.reflections || (profile.skip & I_RENDER3D_SKIP_REFLECTIONS) || camera.effects[2] != 0)
            return;
        static std::vector<char> candidate, visible;
        static std::vector<std::pair<float, int>> near;
        near.clear();
        for (int i = 0; i < numsectors; ++i)
        {
            if (!sectorMist[i].reflective || sectors[i].floorpic == skyflatnum)
                continue;
            float z = floorZ(&sectors[i]);
            if (camera.eye[2] < z + 1)
                continue;
            for (int leaf : sectorFloors[i])
            {
                const auto &b = floorBounds[leaf];
                float dx = std::max({b[0] - camera.eye[0], 0.0f, camera.eye[0] - b[2]}), dy = std::max({b[1] - camera.eye[1], 0.0f, camera.eye[1] - b[3]});
                float d = std::hypot(dx, dy);
                if (d < 1024)
                    near.push_back({d, leaf});
            }
        }
        if (!near.empty())
        {
            candidate.assign(numsubsectors, 0);
            visible.assign(numsubsectors, 0);
            for (const auto &n : near)
                candidate[n.second] = 1;
            visibleLeaves(camera, candidate, visible);
            float nearest = 1024;
            for (const auto &[d, leaf] : near)
                if (visible[leaf] && d < nearest)
                {
                    nearest = d;
                    reflectionPlane = floorZ(subsectors[leaf].sector);
                    reflectionActive = true;
                }
        }
    }
    // Vanilla draws sky wherever a sky flat would be, hiding everything past
    // it; sky ceilings and the open band above a lower sky neighbor therefore
    // go into depth with the sky shader. A sky ceiling rises to the tallest
    // sprite in its sector, with sky walls along its edges up to that height,
    // so trees and monsters still stand out against a low sky. skyTops gives
    // each sector's sky height.
    static const std::vector<float> &skyTops(float fraction)
    {
        static std::vector<float> skyTop;
        skyTop.resize(numsectors);
        for (int i = 0; i < numsectors; ++i)
        {
            skyTop[i] = ceilZ(&sectors[i]);
            if (sectors[i].ceilingpic != skyflatnum)
                continue;
            for (const mobj_t *thing = sectors[i].thinglist; thing; thing = thing->snext)
            {
                if (thing->sprite < 0 || thing->sprite >= numsprites)
                    continue;
                const auto &sprite = sprites[thing->sprite];
                int frame = thing->frame & FF_FRAMEMASK;
                if (frame >= sprite.numframes)
                    continue;
                const auto &sf = sprite.spriteframes[frame];
                float x, y, z;
                interpolatedPosition(*thing, fraction, x, y, z);
                for (int r = 0; r < (sf.rotate ? 8 : 1); ++r)
                    skyTop[i] = std::max(skyTop[i], z + units(spritetopoffset[sf.lump[r]]) + 1);
            }
        }
        return skyTop;
    }
    static std::vector<Vertex> &skyOutput(const sector_t *sector)
    {
#ifdef DOOM_BOOM_SECTORS
        if (sector->skyline)
        {
            const line_t &line = lines[std::abs(sector->skyline) - 1];
            int texture = texturetranslation[sides[line.sidenum[0]].toptexture];
            wallImage(texture);
            return transferredSkies[texture];
        }
#endif
        return skyVertices;
    }
    // Floors and ceilings, batched per flat; sky flats go into skyVertices.
    static void flatGeometry(const std::vector<float> &skyTop)
    {
        for (int i = 0; i < numsubsectors; ++i)
        {
            ++processedLeaves;
            if(cullWorld&&worldFrustum.outside(floorBounds[i])) {++rejectedLeaves;continue;}
            const auto &poly = floors[i];
            if (poly.size() < 3)
                continue;
            const sector_t *sector = subsectors[i].sector;
#ifdef DOOM_BOOM_SECTORS
            sector_t boomCopy;
            const sector_t *visual = P_BoomRenderSector(const_cast<sector_t *>(sector), &boomCopy, 0);
#else
            const sector_t *visual = sector;
#endif
            if (digMeshEnabled() && sector->tag == 19001)
                continue;
            int support = P_GlobalDigBreachSupportRoom(int(sector - sectors));
            for (int ceiling = 0; ceiling < 2; ++ceiling)
            {
                // A destroyed closed slab needs a roof at the exposed room's
                // ceiling, not its original coincident floor/ceiling plane.
                int flat = ceiling ? (support >= 0 ? sectors[support].ceilingpic : visual->ceilingpic) : support >= 0 ? sectors[support].floorpic
                                                                                                                      : visual->floorpic;
                if (flat == skyflatnum)
                {
                    float z = ceiling ? skyTop[sector - sectors] : floorZ(sector);
                    for (size_t n = 1; n + 1 < poly.size(); ++n)
                        for (Point p : {poly[0], poly[n], poly[n + 1]})
                            skyOutput(sector).push_back({p.x, p.y, z});
                    continue;
                }
                int lump = firstflat + flattranslation[flat];
                lumpImage(lump, true);
                float z = visual == sector ? sectorHeight(sector, ceiling) : units(ceiling ? visual->ceilingheight : visual->floorheight);
                const sector_t *lit = support >= 0 ? &sectors[support] : sector;
#ifdef DOOM_BOOM_SECTORS
                int lightSector = ceiling ? visual->ceilinglightsec : visual->floorlightsec;
                if (lightSector >= 0)
                    lit = &sectors[lightSector];
#endif
                float light = lighting(lit);
                if (ceiling && support >= 0)
                {
                    float floor = floorZ(sector);
                    P_GlobalDigBreachRoomBounds(int(sector - sectors), &floor, &z);
                }
                // MODE_MIRROR_FLOOR: this floor samples the mirrored pass.
                unsigned mirror = reflectionActive && !ceiling && sectorMist[sector - sectors].reflective && std::abs(z - reflectionPlane) < 0.5f ? MODE_MIRROR_FLOOR : 0;
                if (!ceiling && sectorMist[sector - sectors].liquid)
                    mirror |= MODE_LIQUID;

                if (addExcavatedFlat(i, ceiling, -1 - lump, z, light, mirror | sectorMode(sector)))
                    continue;
                auto &out = batches[-1 - lump];
                size_t first = out.size();
                if (!addBrokenFlat(out, i, int(sector - sectors), ceiling, lumpImage(lump, true), z, light, mirror | sectorMode(sector)))
                    for (size_t n = 1; n + 1 < poly.size(); ++n)
                        for (Point p : {poly[0], poly[n], poly[n + 1]})
                            out.push_back({p.x, p.y, z, p.x, -p.y, light, mirror | sectorMode(sector)});
#ifdef DOOM_BOOM_SECTORS
                float ox = units(ceiling ? visual->ceiling_xoffs : visual->floor_xoffs), oy = units(ceiling ? visual->ceiling_yoffs : visual->floor_yoffs);
                for (size_t n = first; n < out.size(); n++)
                {
                    out[n].u += ox;
                    out[n].v += oy;
                }
#endif
                lampGlow(out, first, sector, lumpImage(lump, true));
            }
        }
    }
    // The renderer and its attached effects use the same texel anchor and
    // clipping for one-sided, pegged and masked wall sections.
    static bool mappedWallTexture(const line_t &line,const side_t &side,const sector_t *back,int part,
                                  float floor,float ceil,float bf,float bc,WallSpan &span)
    {
        switch(part)
        {
        case 0:
            span.bottom=floor;span.top=ceil;span.texture=side.midtexture;
            span.anchor=span.texture&&(line.flags&ML_DONTPEGBOTTOM)?
                floor+units(textureheight[span.texture]):ceil;
            break;
        case 1:
            if(bc>=ceil||(side.sector->ceilingpic==skyflatnum&&back->ceilingpic==skyflatnum))return false;
            span.bottom=std::max(bc,floor);span.top=ceil;span.texture=side.toptexture;
            span.anchor=(line.flags&ML_DONTPEGTOP)?ceil:bc+(span.texture?units(textureheight[span.texture]):0);
            break;
        case 2:
            if(bf<=floor)return false;
            span.bottom=floor;span.top=std::min(bf,ceil);span.texture=side.bottomtexture;
            span.anchor=(line.flags&ML_DONTPEGBOTTOM)?ceil:bf;
            break;
        case 3:
        {
            if(!side.midtexture)return false;
            span.texture=side.midtexture;
            const float low=std::max(floor,bf),high=std::min(ceil,bc);
            int width,height;
            R_TextureDimensions(texturetranslation[span.texture],&width,&height);
            span.anchor=((line.flags&ML_DONTPEGBOTTOM)?low+height:high)+units(side.rowoffset);
            span.bottom=std::max(low,span.anchor-height);
            span.top=std::min(high,span.anchor);
            return true;
        }
        default:return false;
        }
        span.anchor+=units(side.rowoffset);
        return true;
    }

    bool wallAmbienceSpan(const WallHit &hit,WallSpan &span,bool simulation)
    {
        if(hit.line<0||hit.line>=numlines||hit.side<0||hit.side>1)return false;
        const line_t &line=lines[hit.line];
        if(line.sidenum[hit.side]<0)return false;
        const side_t &side=sides[line.sidenum[hit.side]];
        const sector_t *front=side.sector,*back=hit.side?line.frontsector:line.backsector;
        if((back==nullptr)!=(hit.section==0))return false;
        if(digMeshEnabled()&&((front->tag==19001)||(back&&back->tag==19001)))return false;
        float damage[4];
        if(P_GlobalDigEnabled()&&P_GlobalDigBounds(hit.line,hit.side,damage))return false;
#ifdef DOOM_BOOM_SECTORS
        if(hit.section==3&&line.tranlump)return false;
#endif
        auto heights=[&](const sector_t *sector,int facing,float &floor,float &ceil) {
            (void)facing;
            floor=simulation?units(sector->floorheight):floorZ(sector);
            ceil=simulation?units(sector->ceilingheight):ceilZ(sector);
#ifdef DOOM_BOOM_SECTORS
            sector_t fake;
            if(P_BoomRenderSector(const_cast<sector_t*>(sector),&fake,facing)!=sector) {
                floor=units(fake.floorheight);ceil=units(fake.ceilingheight);
            }
#endif
        };
        float floor,ceil,bf=0,bc=0;
        heights(front,0,floor,ceil);
        if(back)heights(back,1,bf,bc);
        else P_GlobalDigBreachRoomBounds(int(front-sectors),&floor,&ceil);
        if(!mappedWallTexture(line,side,back,hit.section,floor,ceil,bf,bc,span)||
           span.texture<=0||span.top<=span.bottom)return false;
        span.texture=texturetranslation[span.texture];
        if(span.texture<=0||span.texture==skytexture)return false;
        const vertex_t *a=hit.side?line.v2:line.v1,*b=hit.side?line.v1:line.v2;
        span.a={units(a->x),units(a->y)};span.b={units(b->x),units(b->y)};
        return true;
    }

    // The upper, lower, middle and one-sided wall sections of every line side,
    // and sky walls along the edges of sky ceilings.
    // allSides: also wall sides facing away from the eye, for frames that
    // capture glass probes from elsewhere.
    static void wallGeometry(const Uniforms &camera, const std::vector<float> &skyTop, bool allSides)
    {
        auto skyWall = [](const sector_t *sector, Point a, Point b, float bottom, float top)
        {
            if (top <= bottom)
                return;
            for (const Vertex &v : {Vertex{a.x, a.y, bottom}, Vertex{b.x, b.y, bottom}, Vertex{b.x, b.y, top},
                                    Vertex{a.x, a.y, bottom}, Vertex{b.x, b.y, top}, Vertex{a.x, a.y, top}})
                skyOutput(sector).push_back(v);
        };
        for (int i = 0; i < numlines; ++i)
        {
            ++processedLines;
            bool rejected=cullWorld&&worldFrustum.outside(wallBounds[i]);
            if(rejected)++rejectedLines;
            const line_t &line = lines[i];
            if (digMeshEnabled() && ((line.frontsector && line.frontsector->tag == 19001) || (line.backsector && line.backsector->tag == 19001)))
                continue;
            for (int side = 0; side < 2; ++side)
            {
                if (line.sidenum[side] < 0)
                    continue;
                const side_t &s = sides[line.sidenum[side]];
                const sector_t *front = s.sector, *back = side ? line.frontsector : line.backsector;
                Point a = {units((side ? line.v2 : line.v1)->x), units((side ? line.v2 : line.v1)->y)};
                Point b = {units((side ? line.v1 : line.v2)->x), units((side ? line.v1 : line.v2)->y)};
                // Keep span/light collection, including emitters outside the view.
                // addWall exits after wallLights, before generating vertices.
                bool facing = !rejected&&(allSides || (b.x - a.x) * (camera.eye[1] - a.y) - (b.y - a.y) * (camera.eye[0] - a.x) <= 0);
                if (!facing && P_GlobalDigEnabled())
                {
                    float damage[4];
                    facing = P_GlobalDigBounds(i, side, damage) != 0;
                }
                float floor = floorZ(front), ceil = ceilZ(front), light = lighting(front);
#ifdef DOOM_BOOM_SECTORS
                sector_t boomFront, boomBack;
                const sector_t *vf = P_BoomRenderSector(const_cast<sector_t *>(front), &boomFront, 0);
                const sector_t *vb = back ? P_BoomRenderSector(const_cast<sector_t *>(back), &boomBack, 1) : nullptr;
                if (vf != front)
                {
                    floor = units(vf->floorheight);
                    ceil = units(vf->ceilingheight);
                }
#endif
                if (std::abs(b.y - a.y) < 0.01)
                    light = std::max(0.1f, light - 0.04f);
                if (std::abs(b.x - a.x) < 0.01)
                    light = std::min(1.0f, light + 0.04f);
                float bf = back ? floorZ(back) : 0, bc = back ? ceilZ(back) : 0;
#ifdef DOOM_BOOM_SECTORS
                if (back && vb != back)
                {
                    bf = units(vb->floorheight);
                    bc = units(vb->ceilingheight);
                }
#endif
                if (!back)
                    P_GlobalDigBreachRoomBounds(int(front - sectors), &floor, &ceil);
                for(int part=back?1:0;part<=(back?3:0);++part)
                {
                    WallSpan span;
                    if(mappedWallTexture(line,s,back,part,floor,ceil,bf,bc,span))
                        addWall(line,side,a,b,span.bottom,span.top,span.texture,span.anchor,light,facing,part);
                }
                if (!rejected&&front->ceilingpic == skyflatnum)
                    skyWall(front, a, b, back && back->ceilingpic == skyflatnum ? std::max(skyTop[back - sectors], floor) : ceil, skyTop[front - sectors]);
            }
        }
    }
    struct Caster
    {
        const mobj_t *thing;
        const Image *image;
        int lump;
        bool flip;
        float x, y, z, distance;
        RagdollCard card{};
        float roll=0,pivot=0,pivotDown=0;
    };
    // The rotation of a sprite seen from (dx,dy) away, for a thing facing angle.
    static unsigned spriteRotation(float dx, float dy, angle_t facing)
    {
        angle_t angle = (angle_t)(int64_t)(atan2(dy, dx) * (4294967296.0 / (2 * doomPi)));
        return (angle - facing + (unsigned)(ANG45 / 2) * 9) >> 29;
    }
    // The dynamic light on a thing (pulse, rgb) and the strongest light above it
    // that can cast its shadow; returns that light's strength. The classic
    // flashlight goes to beam instead, in the shaders' 1/8 steps (beamSteps).
    static void dynamicThingLight(const mobj_t *thing, float x, float y, float z, std::array<float, 3> &pulse, std::array<float, 3> &beam, ShadowLights &shadowLights)
    {
        for (unsigned n = 0; n < flashes.count; ++n)
        {
            if (fogOnly[n])
                continue;
            float contribution = flashAt(x, y, z + units(thing->height) * 0.5f, flashes.lights[n]);
            if (settings.classicFlashlight && flashes.lights[n].direction[3] > 0)
            {
                float stepped = std::floor(std::min(contribution, 1.0f) * 8 + 0.5f) / 8;
                for (int c = 0; c < 3; ++c)
                    beam[c] += stepped * flashes.lights[n].color[c];
            }
            else
            {
                // Static lights reach sprites through the baked light at their feet.
                float dynamic = contribution * (1 - (bakedLightsActive ? flashes.lights[n].baked : 0));
                for (int c = 0; c < 3; ++c)
                    pulse[c] += dynamic * flashes.lights[n].color[c];
            }
            bool bounce = lightKinds[n] == I_RENDER3D_LIGHT_FLASHLIGHT && flashes.lights[n].direction[3] <= 0;
            if (!bounce && lightSources[n] != thing && flashes.lights[n].position[2] > z + 8)
            {
                const Flash &f = flashes.lights[n];
                shadowLights.add({f.position[0], f.position[1], f.position[2], contribution});
            }
        }
    }
    // Immutable scene inputs, independent output per actor. No image-cache or
    // geometry mutation occurs in the workers.
    struct ThingLighting
    {
        mobj_t *thing;
        int sector;
        float x, y, z;
        bool mirrored, culled = false;
        std::array<float, 3> pulse{}, beam{}, baked{};
        ShadowLights shadows;
        StaticSample statics;
    };
    static void lightThing(ThingLighting &v)
    {
        if (v.culled)
            return;
        MP_AUTO(MP_SPRITE_LIGHT);
        v.pulse = {};
        v.beam = {};
        v.shadows = {};
        v.statics = {};
        dynamicThingLight(v.thing, v.x, v.y, v.z, v.pulse, v.beam, v.shadows);
        v.baked = bakedLightAt(v.x, v.y, v.z + units(v.thing->height) * 0.5f, &sectors[v.sector], &v.statics);
        staticShadows(v.statics, v.z, v.shadows);
    }
    struct FilterArtwork
    {
        int width = 0, height = 0, left = 0, top = 0;
        bool read = false, valid = false;
    };
    static std::vector<FilterArtwork> filterArtwork;
    static std::vector<byte> filterPatch;
    static SpriteVisibilityStats filterStats{};
    void spriteFilteringShutdown()
    {
        filterArtwork.clear();
        filterPatch.clear();
        filterStats = {};
    }
    static const FilterArtwork *filterDimensions(int lump)
    {
        int index = lump - firstspritelump;
        if (index < 0 || index >= numspritelumps)
            return nullptr;
        if (filterArtwork.size() != (size_t)numspritelumps)
            filterArtwork.resize(numspritelumps);
        auto &a = filterArtwork[index];
        if (!a.read)
        {
            a.read = true;
            int bytes = W_LumpLength(lump);
            // Read without adding zone-cache allocations ahead of the original
            // draw order. Oversized/uncertain resources keep the original path.
            if (bytes >= 8 && bytes <= 32 * 1024 * 1024)
            {
                filterPatch.resize(bytes);
                W_ReadLump(lump, filterPatch.data());
                const patch_t *p = (const patch_t *)filterPatch.data();
                a.width = SHORT(p->width);
                a.height = SHORT(p->height);
                a.left = SHORT(p->leftoffset);
                a.top = SHORT(p->topoffset);
                a.valid = a.width > 0 && a.height > 0 && a.width <= 8192 && a.height <= 8192 && 8 + (size_t)a.width * 4 <= (size_t)bytes;
            }
        }
        return a.valid ? &a : nullptr;
    }
    static bool filterThing(const ThingLighting &item, const Uniforms &camera, float fraction)
    {
        auto keep = [](int reason)
        {++filterStats.fallback[reason];return false; };
        const mobj_t *thing = item.thing;
        if (settings.ps2Style || thing->player || thing->health <= 0 || !(thing->flags & MF_COUNTKILL) ||
            (thing->flags & (MF_CORPSE | MF_MISSILE)) || !thing->subsector || !thing->info)
            return keep(SVF_OTHER);
        if (voxelMonster(*thing))
            return keep(SVF_VOXEL);
        BodyPose pose{};
        DeathView death = {thing->sprite, thing->frame, thing->angle, false, 1, 0, {}};
        if (bodyPose(*thing, fraction, pose) || (settings.deathDirector && deathView(*thing, fraction, death)))
            return keep(SVF_POSE);
        WoundSpot spot;
        if (woundSpots(thing, &spot, 1))
            return keep(SVF_WOUND);
        if ((size_t)item.sector >= sectorMist.size() || sectorMist[item.sector].liquid)
            return keep(SVF_LIQUID);
        if (!prepareCriticalSurface)
            return keep(SVF_OTHER);
        const auto &sf = sprites[thing->sprite].spriteframes[thing->frame & FF_FRAMEMASK];
        unsigned rotation = sf.rotate ? spriteRotation(units(thing->x) - camera.eye[0], units(thing->y) - camera.eye[1], thing->angle) : 0u;
        const auto *art = filterDimensions(firstspritelump + sf.lump[rotation]);
        if (!art)
            return keep(SVF_ARTWORK);
        using namespace doom_sprite_frustum;
        float floor = floorZ(&sectors[item.sector]), ceiling = ceilZ(&sectors[item.sector]);
        Camera view{{camera.eye[0], camera.eye[1], camera.eye[2]},
                    {camera.right[0], camera.right[1], camera.right[2]},
                    {camera.up[0], camera.up[1], camera.up[2]},
                    {camera.forward[0], camera.forward[1], camera.forward[2]},
                    camera.projection[0],
                    camera.projection[1],
                    camera.projection[2]};
        Bounds bounds = artwork(item.x, item.y, item.z, art->width, art->height, art->left, art->top, view.right, floor, ceiling, sectors[item.sector].ceilingpic != skyflatnum);
        double reach = shadowReach();
        // Shadow vertices include distance-dependent decal lift. Bound its maximum
        // over the entire horizontal footprint, not just at the actor centre.
        double far = std::hypot(item.x - camera.eye[0], item.y - camera.eye[1]) + 2 * reach;
        include(bounds, {{item.x - reach, item.y - reach, floor - 1.}, {item.x + reach, item.y + reach, floor + 2 * (.15 + far * .0004) + 1}});
        if (!valid(bounds) || !finite(view.eye) || !std::isfinite(floor) || !std::isfinite(ceiling))
            return keep(SVF_INVALID);
        ++filterStats.eligible;
        if (!outside(bounds, view))
            return false;
        ++filterStats.rejected;
        return true;
    }
    static void filteredWoundView(const ThingLighting &v, const Uniforms &camera)
    {
        // Preserve the original image-cache allocation order and surface semantics,
        // including off-camera cosmetic impacts; skip only lighting/drawing work.
        const mobj_t *thing = v.thing;
        const auto &sf = sprites[thing->sprite].spriteframes[thing->frame & FF_FRAMEMASK];
        unsigned rotation = sf.rotate ? spriteRotation(units(thing->x) - camera.eye[0], units(thing->y) - camera.eye[1], thing->angle) : 0u;
        Image &image = lumpImage(firstspritelump + sf.lump[rotation], false);
        float top = v.z + image.top, bottom = top - image.height, floor = floorZ(&sectors[v.sector]), ceiling = ceilZ(&sectors[v.sector]);
        float shift = std::max(0.f, floor - bottom);
        if (sectors[v.sector].ceilingpic != skyflatnum && ceiling - floor >= image.height)
            shift = std::min(shift, ceiling - top);
        top += shift;
        bottom += shift;
        woundView(thing, players[displayplayer].mo, image, sf.flip[rotation], v.x, v.y, top - image.top, top, bottom, camera.right[0], camera.right[1], nullptr, (int)rotation);
    }
    // Things standing in a liquid sink in a little, like Heretic's
    // footclip (the floor hides their feet; small pickups only a
    // little), and thick liquids wrap a wobbling collar of goo around
    // them. Pools are often several sectors at one height, so every
    // liquid floor counts, not only pits. Returns how far the sprite sinks.
    static float sinkInLiquid(const mobj_t *thing, int i, const Image &image, float x, float y, const Uniforms &camera, bool collar = true, bool down = false)
    {
        if (!settings.splashes || (size_t)i >= sectorMist.size() || !sectorMist[i].liquid || (!down && thing->z > sectors[i].floorheight) ||
            (thing->flags & (MF_MISSILE | MF_NOBLOCKMAP | MF_FLOAT)) || sectors[i].floorpic == skyflatnum)
            return 0;
        if (collar && sectorMist[i].thick && !(thing->flags & MF_SPECIAL) && std::hypot(x - camera.eye[0], y - camera.eye[1]) < 1024)
        {
            float r = units(thing->radius) * 0.55f;
            int tic = leveltime / 4;
            uint32_t seed = (uint32_t)(uintptr_t)thing;
            auto color = gooColor(i, seed, x, y, floorZ(&sectors[i]), camera);
            for (int k = 0; k < 6; ++k)
            {
                float angle = (k + hashUnit(k, tic, seed) * 0.5f) * doomPi / 3, wobble = r * (0.9f + 0.25f * hashUnit(k, tic, seed ^ 3u));
                pixelBlock(particleVertices, camera, x + std::cos(angle) * wobble, y + std::sin(angle) * wobble, floorZ(&sectors[i]) + 0.2f, 2.0f, 0.6f, color, true, &color);
            }
        }
        return std::min(8.0f, image.height * 0.35f);
    }
    void criticalSurface(mobj_t *thing, mobj_t *viewer)
    {
        if (!thing || !viewer || !thing->subsector || P_HazardAcidCorpse(thing) || thing->sprite < 0 || thing->sprite >= numsprites)
            return;
        const auto &sprite = sprites[thing->sprite];
        int frame = thing->frame & FF_FRAMEMASK;
        if (frame >= sprite.numframes)
            return;
        const auto &sf = sprite.spriteframes[frame];
        unsigned rotation = sf.rotate ? spriteRotation(units(thing->x - viewer->x), units(thing->y - viewer->y), thing->angle) : 0u;
        const Image &image = lumpImage(firstspritelump + sf.lump[rotation], false);
        float x = units(thing->x), y = units(thing->y), z = units(thing->z);
        BodyPose pose{};
        bool posed = bodyPose(*thing, 1, pose);
        if (posed)
            z += pose.lift;
        int i = (int)(thing->subsector->sector - sectors);
        float top = z + image.top, bottom = top - image.height;
        float floor = floorZ(&sectors[i]), ceiling = ceilZ(&sectors[i]);
        float shift = std::max(0.f, floor - bottom);
        if (sectors[i].ceilingpic != skyflatnum && ceiling - floor >= image.height)
            shift = std::min(shift, ceiling - top);
        top += shift;
        bottom += shift;
        Uniforms camera{};
        camera.eye[0] = units(viewer->x);
        camera.eye[1] = units(viewer->y);
        float sink = sinkInLiquid(thing, i, image, x, y, camera, false);
        float yaw = (float)(viewer->angle * (2 * doomPi / 4294967296.0));
        woundView(thing, viewer, image, sf.flip[rotation], x, y, top - image.top - sink, top - sink, bottom - sink,
                  std::sin(yaw), -std::cos(yaw), posed ? &pose : nullptr, (int)rotation);
    }
    // The glass mirror's view (glassMirrorActive) for the sprites drawn into it:
    // the mirrored eye, and the right of an ordinary camera there. The mirror
    // pass's own axes are reflected, which already flips its whole image left
    // to right, so sprites are laid out as that unflipped camera sees them, the
    // reflected right reversed; otherwise they turn the wrong way in the glass.
    struct MirrorView
    {
        float eye[2], right[2];
    };
    static bool visibleEyeInjuryActive(mobj_t *actor)
    {
        return settings.accelerated && settings.enhancedWeakPoints && settings.eyeInjuries &&
               settings.visibleEyeInjuries && actor->health > 0 && !actor->player &&
               (actor->type == MT_HEAD || actor->type == MT_PAIN) && P_WeakpointBlind(actor);
    }

    // One thing in sector i: its shadows, light, liquid footclip and sprite
    // quad. Things the flashlight beam can outline join casters. With mirror,
    // also its quad for the glass mirror (mirrorBatches), from the same light:
    // seen from the mirrored eye, so its frame shows the side facing the glass,
    // and facing the mirrored camera. The player's own sprite gets only that one.
    static void addThing(mobj_t *thing, int i, const Uniforms &camera, float fraction, const Flash *beam, std::vector<Caster> &casters,
                         const MirrorView *mirror, const ThingLighting &lit)
    {
        bool self = thing == players[displayplayer].mo && !director_view_active;
        if ((self && !mirror) || thing->sprite < 0 || thing->sprite >= numsprites)
            return;
        if (customGunPuff(thing) || liquidProjectileHidden(thing))
            return;
        // Reflected/voxel presentation retains the existing sprite/model path.
        if (!mirror && monsterProjectileVisual(*thing, camera, fraction))
            return;
        const bool acidCorpse=P_HazardAcidCorpse(thing);
        const bool lavaCorpse=P_HazardLavaCorpse(thing);
        const bool charred=lavaCorpse||P_CombatCharred(thing);
        DeathView death = {thing->sprite, thing->frame, thing->angle, false, 1, 0, {}};
        bool directed = !self && !acidCorpse && deathView(*thing, fraction, death);
        if (death.sprite < 0 || death.sprite >= numsprites)
            return;
        const auto &sprite = sprites[death.sprite];
        int frame = death.frame & FF_FRAMEMASK;
        if (frame >= sprite.numframes)
            return;
        const auto &sf = sprite.spriteframes[frame];
        auto rotationFrom = [&](const float *eye)
        { return sf.rotate && !death.fall.active ? spriteRotation(units(thing->x) - eye[0], units(thing->y) - eye[1], death.facing) : 0u; };
        auto flipped = [&](unsigned seen)
        { return (bool)sf.flip[seen] != death.mirror; };
        unsigned rotation = rotationFrom(camera.eye);
        int lump = firstspritelump + sf.lump[rotation];
        Image &image = lumpImage(lump, false);
        float x, y, z;
        interpolatedPosition(*thing, fraction, x, y, z);
        // A ragdoll is lit, sunk and grounded where its body lies, not where the corpse slid to: thrown
        // across a light's edge or down steps, it was lit as where it died.
        bool dollDown = false;
        if (death.ragdoll)
        {
            dollDown = ragdollWhere(*death.ragdoll, fraction, x, y, z);
            i = (int)(R_PointInSubsector((fixed_t)(x * FRACUNIT), (fixed_t)(y * FRACUNIT))->sector - sectors);
        }
        // Hit flinches and death jolts (body_motion.cpp): drawn in strips, a hop lifts the body.
        // A directed death scales the jolt by its phase and adds the landing's settle.
        BodyPose pose;
        bool posed = !self && !acidCorpse && !death.fall.active && (thing->flags & MF_COUNTKILL) && bodyPose(*thing, fraction, pose);
        if (directed)
        {
            if (!posed)
                pose = {};
            for (int k = 0; k < 2; ++k)
            {
                pose.lean[k] *= death.bend;
                pose.whip[k] *= death.bend;
            }
            pose.squash *= death.bend;
            pose.flatten = death.settle;
            posed = posed || death.settle > 0;
        }
        if (posed)
            z += pose.lift;
        const bool ragdoll = death.ragdoll != nullptr;
        const size_t casterIndex=casters.size();
        if (!self && !acidCorpse && !ragdoll)
            addEnemyShadow(*thing, image, lump, flipped(rotation), x, y, z, camera);
        if (!self && !acidCorpse && !ragdoll && beam && !(thing->flags & (MF_MISSILE | MF_NOBLOCKMAP | MF_SPECIAL)) && (thing->flags & (MF_SOLID | MF_SHOOTABLE)) && !(death.frame & FF_FULLBRIGHT))
            casters.push_back({thing, &image, lump, (bool)flipped(rotation), x, y, z, std::hypot(x - camera.eye[0], y - camera.eye[1])});
        auto pulse = lit.pulse, beamLight = lit.beam, baked = lit.baked;
        const auto &shadowLights = lit.shadows;
        const auto &statics = lit.statics;
        if (!self && !acidCorpse && !ragdoll)
            for (const auto &light : shadowLights.lights)
                if (light.strength > 0.025f)
                    addEnemyShadow(*thing, image, lump, flipped(rotation), x, y, z, camera, &light);
        float light = lighting(&sectors[i], death.frame & FF_FULLBRIGHT);
        unsigned mode = MODE_SPRITE | (thing->flags & MF_SHADOW ? MODE_FUZZ : 0);
        // Per-pixel lighting from silhouette normals (flipped with a mirrored frame, below).
        if (settings.detail && !(death.frame & FF_FULLBRIGHT))
            mode |= MODE_RELIT;
        // Airborne blood shines like the floor blood (needs the normals of MODE_RELIT).
        if (settings.bloodShine && Game_ThingRole(thing->type) == GAME_THING_BLOOD && (mode & MODE_RELIT))
            mode |= MODE_WET_SPRITE;
        const bool eyeInjured = visibleEyeInjuryActive(thing) && !death.ragdoll &&
            !P_CombatCharred(thing) &&
            ((thing->type == MT_HEAD && death.sprite == SPR_HEAD) ||
             (thing->type == MT_PAIN && death.sprite == SPR_PAIN));
        if (settings.glowingEyes && (thing->flags & MF_COUNTKILL) && thing->health > 0 && !eyeInjured)
            mode |= MODE_GLOWING_EYES;
        if (!(death.frame & FF_FULLBRIGHT))
            light = sunLightAt(x, y, &sectors[i], flowLightAt(x, y, &sectors[i], seamLightAt(x, y, &sectors[i], light)));
        // MODE_SOFT: effect sprites (shots, puffs, fogs) dither out at floors
        // and ceilings and draw in front of walls within half their width.
        bool soft = settings.softSprites && ((thing->flags & MF_MISSILE) || ((thing->flags & MF_NOBLOCKMAP) && (thing->flags & MF_NOGRAVITY)));
        if (soft)
            mode |= MODE_SOFT | sectorMode(&sectors[i]);
        if (thing->flags & MF_SPECIAL)
        {
            // Keep supplies readable; reserve bloom for light-emitting pickups.
            float phase = (float)((uintptr_t)thing % 997) * 0.37f;
            light = std::max(light, 0.5f + 0.08f * std::sin(gameSeconds() * 3.0f + phase));
            if (glowingPickup(thing->sprite))
                mode |= MODE_PICKUP;
        }
        // Emissive WAD frames keep their original white cores and artwork;
        // their light still illuminates surrounding surfaces and other sprites.
        // Detail sprites light themselves per pixel from the light mask, so
        // their tint carries only the baked light. With bake-only lights they
        // shape it by where it comes from (statics).
        // The classic beam lifts toward BEAM_CEILING (lighting.glsl), never past it, as in world.frag.
        if (mode & MODE_RELIT)
            pulse = baked;
        else
            for (int c = 0; c < 3; ++c)
            {
                pulse[c] += baked[c];
                pulse[c] += beamLight[c] * std::max(0.0f, 1.2f - light - pulse[c]);
            }
        unsigned packedStatics = 0;
        float towards = std::sqrt(statics.towards[0] * statics.towards[0] + statics.towards[1] * statics.towards[1] + statics.towards[2] * statics.towards[2]);
        if (bakeOnlyActive && (mode & MODE_RELIT) && statics.amount > 0 && towards > 1e-6f)
        {
            for (int c = 0; c < 3; ++c)
                packedStatics |= (unsigned)std::lround((statics.towards[c] / towards * 0.5f + 0.5f) * 255) << (8 * c);
            packedStatics |= (unsigned)std::lround(std::clamp(towards / statics.amount, 0.0f, 1.0f) * 255) << 24;
        }
        if (death.frame & FF_FULLBRIGHT)
            pulse = {};
        unsigned tint = death.frame & FF_FULLBRIGHT ? 0xFFFFFFFFu : packedLampTint(statics, light);
        // MODE_GROUNDED: grounding darkens the feet of things that rest on the floor.
        // Sprites have no sun map, so vSun carries strength and height above floor.
        float lift = units(thing->z - thing->floorz) + (posed ? pose.lift : 0), grounding = 1 - lift / 32;
        bool grounded = settings.detail && grounding > 0 && !(death.frame & FF_FULLBRIGHT) && !soft && Game_ThingRole(thing->type) != GAME_THING_BLOOD &&
                        !(thing->flags & (MF_SHADOW | MF_MISSILE | MF_SPAWNCEILING));
        // The quad for one view: that view's frame, facing along its right.
        auto emit = [&](std::map<int, std::vector<Vertex>> &out, unsigned seen, float rx, float ry, bool collar)
        {
            if (!acidCorpse && !lavaCorpse && !death.ragdoll &&
                drawVoxel(out, *thing, death.sprite, death.frame, death.facing, x, y, z, light, baked, packedStatics, tint, camera)) {
                // Voxel substitution bypasses the sprite-texture path entirely.
                // Report it explicitly instead of falsely diagnosing missing age data.
                if(M_CheckParm((char*)"-decaydebug")) {
                    const int voxelDecayStage=corpseDecayStage(thing);
                    static int lastVoxelDiagnostic=-1;
                    if(voxelDecayStage && (lastVoxelDiagnostic<0||leveltime-lastVoxelDiagnostic>=10*TICRATE)) {
                        lastVoxelDiagnostic=leveltime;
                        std::fprintf(stderr,"HELLFORGE DECAY: VOXEL-SKIP actor=%d stage=%d (sprite recolor not supported on voxel models)\n",
                                     int(thing->type),voxelDecayStage);
                    }
                }
                return;
            }
            int lump = firstspritelump + sf.lump[seen], spriteKey = -1 - lump;
            int bloodTint = thing->type == MT_BLOOD ? (thing->flags & MF_TRANSLATION) >> MF_TRANSSHIFT : 0;
            Image &sourceImage = bloodTint ? bloodSpriteImage(lump, bloodTint, spriteKey) : lumpImage(lump, false);
            const bool oilRupture=thing->type==MT_MISC77&&thing->sprite==SPR_BEXP&&thing->state==&states[S_BEXP5]&&
                thing->health==0&&(thing->flags&MF_NOBLOOD);
            Image &baseImage = charred ? charredImage(sourceImage,spriteKey,spriteKey) :
                (thing->player||oilRupture) ? playerColourImage(sourceImage,spriteKey,(thing->flags&MF_TRANSLATION)>>MF_TRANSSHIFT,spriteKey) :
                sourceImage;
            // Keep the unmodified art authoritative for silhouette, pose, wound attachment
            // and hole-mask matching; only the sampled GPU texture changes with health.
            // A lightning-ignited original dead tree is only recolored after
            // its cosmetic fire ends. Use the existing palette-based, cached
            // charred sprite variant: no per-frame uploads, actor edits or WAD mutations.
            Image &treeImage = !self && infernoTreeCharred(thing)
                ? charredImage(baseImage,spriteKey,spriteKey) : baseImage;
            Image &skinImage = (!self && !death.ragdoll && settings.monsterSkinDamage && thing->health>0 && thing->info &&
                                !P_CombatCharred(thing) && !thing->player)
                ? monsterSkinImage(thing->type,lump,baseImage,thing->health,thing->info->spawnhealth,spriteKey)
                : treeImage;
            // The age tracker alone decides whether the actor is a terminal
            // ordinary corpse. A death presenter may retain a fall pose or a
            // different frame well after the actor reaches its last death state;
            // do not suppress decomposition merely for those cosmetic details.
            // Still exclude ragdoll cards, charred actors and foreign sprite
            // families, and keep original art for wounds and silhouette tests.
            const int readyDecayStage = !self && !thing->player && !P_CombatCharred(thing)
                ? corpseDecayStage(thing) : 0;
            const int decayStage = readyDecayStage && !death.ragdoll && death.sprite==thing->sprite
                ? readyDecayStage : 0;
            const int decayFamily=(thing->type==MT_TROOP)?1:
                (thing->type==MT_SERGEANT)?2:
                (thing->type==MT_FATSO||thing->type==MT_BRUISER||thing->type==MT_KNIGHT)?3:
                (thing->type==MT_HEAD)?4:0;
            const DecorationArt *art=!self&&!directed&&!sf.rotate?decorationArt(*thing,lump):nullptr;
            Image &eyeImage = eyeInjured
                ? monsterEyeInjuryImage(thing->type, lump, sourceImage, skinImage, spriteKey)
                : skinImage;
            Image &uncoatedImage=decayStage ? corpseDecayImage(lump,skinImage,decayStage,decayFamily,spriteKey) :
                art ? decorationStain(*art,skinImage,spriteKey) : eyeImage;
            Image *residue=acidCorpse&&P_HazardDissolved(thing)?dissolvedCorpseImage(spriteKey):nullptr;
            if(acidCorpse&&!residue)spriteKey=-1-lump;
            Image &image=residue?*residue:acidCorpse?acidCorpseImage(lump,sourceImage,spriteKey):
                acidCoatedImage(thing,uncoatedImage,spriteKey,camera.eye[0],camera.eye[1],flipped(seen),
                                uncoatedImage.eyeDamageMask.empty() ? nullptr : &sourceImage);
            // A traceable renderer hand-off is essential: isolated rule tests
            // cannot prove that the real game selects the new GPU texture.
            if(readyDecayStage && M_CheckParm((char*)"-decaydebug")) {
                static int lastRenderDiagnostic=-1;
                if(lastRenderDiagnostic<0||leveltime-lastRenderDiagnostic>=10*TICRATE) {
                    lastRenderDiagnostic=leveltime;
                    std::fprintf(stderr,"HELLFORGE DECAY: RENDER actor=%d stage=%d applied=%d actorSprite=%d drawnSprite=%d actorFrame=%d drawnFrame=%d fall=%d ragdoll=%d textureKey=%d\n",
                                 int(thing->type),readyDecayStage,int(decayStage!=0),
                                 int(thing->sprite),int(death.sprite),int(thing->frame&FF_FRAMEMASK),
                                 int(death.frame&FF_FRAMEMASK),int(death.fall.active),int(death.ragdoll!=nullptr),spriteKey);
                }
            }
            float left = -(float)image.left, right = left + image.width, top = z + image.top, bottom = top - image.height;
            // The living monster floor-fit below already corrects any art overhang.
            // Do not apply floorOverhang() again to the same sprite.
            const bool floorFittedMonster = (thing->flags & MF_COUNTKILL) && thing->health > 0;
            if (floorFittedMonster)
            {
                float floor = floorZ(&sectors[i]), ceiling = ceilZ(&sectors[i]);
                float shift = std::max(0.f, floor - bottom);
                if (sectors[i].ceilingpic != skyflatnum && ceiling - floor >= image.height)
                    shift = std::min(shift, ceiling - top);
                top += shift;
                bottom += shift;
            }
            float fittedFeet = top - image.top;
            if (death.fall.active && !death.fall.rig)
            {
                // A fall's drawing set down on the floor, lifted, pressed flat.
                float keep = 1 - death.fall.flatten;
                top = z + (image.top - death.fall.ground) * keep + death.fall.lift;
                bottom = z + (image.top - image.height - death.fall.ground) * keep + death.fall.lift;
            }
            // float sink=!ragdoll?sinkInLiquid(thing,i,image,x,y,camera,collar):dollDown?sinkInLiquid(thing,i,image,x,y,camera,collar,true):0;top-=sink;bottom-=sink;
            float sink = acidCorpse ? 0.f : !ragdoll ? sinkInLiquid(thing, i, image, x, y, camera, collar) : dollDown ? sinkInLiquid(thing, i, image, x, y, camera, collar, true)
                                                                                                   : 0;
            if (!acidCorpse && !ragdoll && !death.fall.active && sink == 0 && !floorFittedMonster)
                sink = -floorOverhang(thing, i, image); // fallback for other sprite types
            top -= sink;
            bottom -= sink;
            if(acidCorpse) {
                if(residue) {
                    const float scale=thing->type==MT_SERGEANT?1.3f:
                        (thing->type==MT_BRUISER||thing->type==MT_KNIGHT)?1.5f:1.f;
                    left=-image.width*scale*.5f;
                    right=-left;
                    bottom=units(thing->floorz)+.5f;
                    top=bottom+image.height*scale;
                } else {
                    const float t=float(P_HazardDeathAge(thing))/ACID_DISSOLVE_TICS;
                    const float collapse=t*t*(3.f-2.f*t);
                    bottom=std::max(bottom,std::min(z,units(thing->floorz))+.5f);
                    top=bottom+image.height*(1.f-.78f*collapse);
                }
            }

            float pivotDown=0;
            const float roll=art?decorationRoll(*thing,*art,fraction,rx,ry,top,ceilZ(&sectors[i]),pivotDown):0;
            const float pivot=art?decorationPivot(*art,image,flipped(seen)):0;

            const bool flipImage=!residue&&flipped(seen);
            float u0 = flipImage ? image.width : 0, u1 = flipImage ? 0 : image.width;
            unsigned m = mode | ((mode & MODE_RELIT) && flipImage ? MODE_FLIPPED : 0);
            if (collar && !self && !acidCorpse && !ragdoll)
                woundView(thing, players[displayplayer].mo, baseImage, flipped(seen), x, y, fittedFeet - sink, top, bottom, rx, ry, posed ? &pose : nullptr, (int)seen);
            // Wound holes (wounds.cpp): texels shot through, and their rims.
            std::vector<byte> holes;
            bool wounded = !self && !acidCorpse && !ragdoll && woundMask(*thing, baseImage, flipped(seen), rx, ry, holes);
            const std::vector<byte> *cut = wounded ? &holes : nullptr;
            auto finish = [&](std::vector<Vertex> &batch, size_t from)
            {
                for (size_t n = from; n < batch.size(); ++n)
                {
                    batch[n].red = pulse[0];
                    batch[n].green = pulse[1];
                    batch[n].blue = pulse[2];
                    batch[n].statics = packedStatics;
                    batch[n].lampTint = tint;
                    if(ragdoll&&P_CombatCharred(thing)){batch[n].light*=.22f;batch[n].red=batch[n].green=batch[n].blue=0;batch[n].lampTint=0xFF383838u;}
                    if (grounded)
                    {
                        batch[n].mode |= MODE_GROUNDED;
                        batch[n].sunU = grounding;
                        batch[n].sunV = batch[n].z - z + lift;
                    }
                    if (soft)
                        batch[n].sunU = std::clamp(image.width * 0.5f, 8.0f, 64.0f);
                }
            };
            RagdollCard card;
            if (ragdoll)
            {
                // A ragdoll: its virtual camera's picture on an upright card (ragdoll.cpp), already mirrored.
                if (ragdollCard(*death.ragdoll, fraction, sink, camera.eye, nullptr, card))
                {
                    auto &batch = out[card.key];
                    size_t from = batch.size();
                    unsigned cm = m & ~MODE_FLIPPED;
                    float l = -card.half, r = card.half, t = card.top, b = card.top - card.size, s = (float)card.size;
                    quad(batch, {card.x + rx * l, card.y + ry * l, t, 0, 0, light, cm}, {card.x + rx * r, card.y + ry * r, t, s, 0, light, cm},
                         {card.x + rx * r, card.y + ry * r, b, s, s, light, cm}, {card.x + rx * l, card.y + ry * l, b, 0, s, light, cm});
                    finish(batch, from);
                    if (collar)
                    {
                        const Image &shadow = images.at(card.key);
                        int shadowLump = -1 - card.key; // the shadow renderer resolves images by -1-lump
                        addEnemyShadow(*thing, shadow, shadowLump, false, card.x, card.y, z, camera, nullptr, false, &card);
                        for (const auto &nearLight : shadowLights.lights)
                            if (nearLight.strength > 0.025f)
                                addEnemyShadow(*thing, shadow, shadowLump, false, card.x, card.y, z, camera, &nearLight, false, &card);
                        if (beam && !(thing->flags & (MF_MISSILE | MF_NOBLOCKMAP | MF_SPECIAL)) && (thing->flags & (MF_SOLID | MF_SHOOTABLE)) && !(death.frame & FF_FULLBRIGHT))
                            casters.push_back({thing, &shadow, shadowLump, false, card.x, card.y, z, std::hypot(card.x - camera.eye[0], card.y - camera.eye[1]), card});
                    }
                    return;
                }
                // A first card can wait for admission. Draw the actor's current death frame
                // at its normal position until a generated card is available.
                const int fallbackFrame=thing->frame&FF_FRAMEMASK;
                if(thing->sprite>=0&&thing->sprite<numsprites&&fallbackFrame<sprites[thing->sprite].numframes) {
                    const auto &fallback=sprites[thing->sprite].spriteframes[fallbackFrame];
                    float fx,fy,fz;
                    interpolatedPosition(*thing,fraction,fx,fy,fz);
                    const float *eye=collar||!mirror?camera.eye:mirror->eye;
                    const unsigned facing=fallback.rotate?spriteRotation(fx-eye[0],fy-eye[1],thing->angle):0u;
                    const int fallbackLump=firstspritelump+fallback.lump[facing];
                    const Image &art=lumpImage(fallbackLump,false);
                    if(art.width<=0||art.height<=0)return;
                    const bool flip=fallback.flip[facing];
                    const float l=-float(art.left),r=l+art.width,t=fz+art.top-sink,b=t-art.height;
                    auto &batch=out[-1-fallbackLump];
                    const size_t from=batch.size();
                    const unsigned fm=(m&~MODE_FLIPPED)|((m&MODE_RELIT)&&flip?MODE_FLIPPED:0);
                    const float u=flip?float(art.width):0,v=flip?0:float(art.width);
                    quad(batch,{fx+rx*l,fy+ry*l,t,u,0,light,fm},{fx+rx*r,fy+ry*r,t,v,0,light,fm},
                         {fx+rx*r,fy+ry*r,b,v,float(art.height),light,fm},{fx+rx*l,fy+ry*l,b,u,float(art.height),light,fm});
                    finish(batch,from);
                    if(collar)addEnemyShadow(*thing,art,fallbackLump,flip,fx,fy,fz,camera);
                }
                return;
            }
            auto &vertices = out[spriteKey];
            size_t first = vertices.size();
            const BloodSourceScope woundTint(wounded?monsterBloodTint(thing):0);
            if (death.fall.rig)
                fallQuads(vertices, {0, 0, 0, 0, 0, light, m}, death.fall, x, y, z - sink, rx, ry, flipped(seen), cut);
            else if (posed)
                bodyQuads(vertices, pose, {0, 0, 0, 0, 0, light, m}, x, y, fittedFeet - sink, rx, ry, left, right, image.top, image.height, u0, u1, cut);
            else if (wounded)
                woundedQuads(vertices, {0, 0, 0, 0, 0, light, m}, image, holes, x, y, top, rx, ry, flipped(seen), (top - bottom) / image.height);
            else
                quad(vertices, {x + rx * left, y + ry * left, top, u0, 0, light, m}, {x + rx * right, y + ry * right, top, u1, 0, light, m},
                     {x + rx * right, y + ry * right, bottom, u1, (float)image.height, light, m}, {x + rx * left, y + ry * left, bottom, u0, (float)image.height, light, m});
            if(roll!=0)
            {
                const doom_decor::SpriteRoll rotation{pivot,pivotDown,std::sin(roll),std::cos(roll)};
                for(size_t n=first;n<vertices.size();++n)
                {
                    Vertex &v=vertices[n];
                    const float across=(v.x-x)*rx+(v.y-y)*ry;
                    float rotated=across,down=top-v.z;
                    rotation.apply(rotated,down);
                    v.x+=rx*(rotated-across);
                    v.y+=ry*(rotated-across);
                    v.z=top-down;
                }
                if(collar&&casterIndex<casters.size())
                {
                    auto &caster=casters[casterIndex];
                    caster.z=top-image.top;
                    caster.roll=roll;caster.pivot=pivot;caster.pivotDown=pivotDown;
                }
            }
            if(art&&collar)
                queueDecorationCritter(*thing,*art,baseImage,x,y,top,rx,ry,roll,pivotDown,flipped(seen));
            finish(vertices, first);
        };
        if (!self)
            emit(batches, rotation, camera.right[0], camera.right[1], true);
        if (mirror)
            emit(mirrorBatches, rotationFrom(mirror->eye), mirror->right[0], mirror->right[1], false);
    }

    // Many WAD frames hang a few pixels lower than their foot line (top
    // offset past the image height). The software renderer drew those rows
    // over the floor; the depth buffer hides them, so things on solid ground
    // stand on it instead: lifted by that overhang, at most 16 (larger ones
    // are drawn that way on purpose). Liquids keep their sinking above.

    static float floorOverhang(const mobj_t *thing, int i, const Image &image)
    {
        if (thing->z > thing->floorz || (thing->flags & (MF_MISSILE | MF_SPAWNCEILING | MF_NOGRAVITY)) || image.top >= image.height)
            return 0;
        if ((size_t)i < sectorMist.size() && sectorMist[i].liquid)
            return 0;
        return std::min(16.0f, (float)(image.height - image.top));
    }

    float decorationSpriteTop(const mobj_t &thing,const Image &image)
    {
        const int i=int(thing.subsector->sector-sectors);
        const float sink=sinkInLiquid(&thing,i,image,0,0,Uniforms{},false);
        return units(thing.z)+image.top+(sink==0?floorOverhang(&thing,i,image):-sink);
    }

    // The player's own shadow: the player sprite as seen from the strongest
    // light above (not the flashlight or the player's own shots), or from
    // the sun where the player stands in it.
    static void playerShadow(const Uniforms &camera, float fraction)
    {
        mobj_t *self = players[displayplayer].mo;
        if (!(settings.playerShadow && camera.effects[2] == 0 && self && self->sprite >= 0 && self->sprite < numsprites))
            return;
        float x, y, z;
        interpolatedPosition(*self, fraction, x, y, z);
        ShadowLights lights;
        for (unsigned n = 0; n < flashes.count; ++n)
        {
            const Flash &f = flashes.lights[n];
            if (fogOnly[n] || lightSources[n] == self || lightKinds[n] == I_RENDER3D_LIGHT_FLASHLIGHT || f.position[2] <= z + 8)
                continue;
            float amount = flashAt(x, y, z + 28, f);
            lights.add({f.position[0], f.position[1], f.position[2], amount});
        }
        if (bakeOnlyActive || gridLightActive())
        {
            StaticSample statics;
            bakedLightAt(x, y, z + 28, self->subsector->sector, &statics);
            staticShadows(statics, z, lights);
        }
        bool found = std::any_of(lights.lights.begin(), lights.lights.end(), [](const auto &light)
                                 { return light.strength > 0.025f; });
        if (!found && settings.sun && sunLevel > 0 && !contactCells.empty())
        {
            int cx = std::clamp((int)std::floor((x - mapOrigin[0]) / mapCell), 0, mapWidth - 1), cy = std::clamp((int)std::floor((y - mapOrigin[1]) / mapCell), 0, mapHeight - 1);
            float visible = contactCells[4 * ((size_t)cy * mapWidth + cx) + 2] / 255.0f;
            if (visible > 0.5f)
                lights.add({x + sunAzimuth[0] * 766, y + sunAzimuth[1] * 766, z + 643, 0.6f * visible});
        }
        int frame = self->frame & FF_FRAMEMASK;
        if (frame < sprites[self->sprite].numframes)
            for (const auto &light : lights.lights)
                if (light.strength > 0.025f)
                {
                    const auto &sf = sprites[self->sprite].spriteframes[frame];
                    unsigned rotation = sf.rotate ? spriteRotation(light.x - x, light.y - y, self->angle) : 0;
                    int lump = firstspritelump + sf.lump[rotation];
                    addEnemyShadow(*self, lumpImage(lump, false), lump, sf.flip[rotation], x, y, z, camera, &light, true);
                }
    }
    void geometry(const Uniforms &camera, float fraction)
    {
        MP_AUTO(MP_GEOMETRY);
        const bool visibilityTrace=M_CheckParm((char*)"-profileworldvisibility")!=0;
        uint64_t visibilityStart=(visibilityTrace||profile.enabled)?SDL_GetTicksNS():0;
        processedLeaves=rejectedLeaves=processedLines=rejectedLines=0;
        prepareVoxelModels();
        const bool spike = M_CheckParm((char *)"-profilespikes") != 0;
        const bool lightingTrace=M_CheckParm((char*)"-profilelighting")!=0;
        const bool detailed = spike || lightingTrace || M_CheckParm((char *)"-profilegeometry") != 0;
        uint64_t mark = detailed ? SDL_GetTicksNS() : 0;
        static std::array<double, 10> totals{};
        static int frames = 0;
        std::array<double, 10> current{};
        auto timing = [&](int n)
        {if(detailed){auto now=SDL_GetTicksNS();current[n]=(now-mark)/1e6;totals[n]+=current[n];mark=now;} };

        // The hardware renderer bypasses R_StoreWallRange, which normally records
        // automap discovery. Reuse our front-to-back occlusion traversal once a tic.
        static int mappedTic = -1;
        static unsigned mappedLevel = ~0u;
        if (mappedTic != gametic || mappedLevel != r_levelserial)
        {
            mappedTic = gametic;
            mappedLevel = r_levelserial;
            std::vector<char> unused;
            visibleLeaves(camera, unused, unused, true);
        }
        retainSolidAssembly();
        beginMonsterProjectileFrame();
        vertexSections.clear();
        voxelDraws.clear();
        voxelMirrorDraws.clear();
        for (auto &batch : batches)
            batch.second.clear();
        for (auto &batch : shadowBatches)
            batch.second.clear();
        for (auto &batch : mirrorBatches)
            batch.second.clear();
        surfaceLights.clear();
        std::copy(camera.eye, camera.eye + 3, surfaceEye);
        collectFlashes(fraction, camera);
        timing(0);
        collectFlatLights();
        timing(1);
        findReflection(camera);
        skyVertices.clear();
        transferredSkies.clear();
        translucentWorld.clear();
        const auto &skyTop = skyTops(fraction);
        const bool captures=planGlassCaptures(camera);
        // Secondary cameras, baking and displaced/damaged surfaces keep the
        // complete original batches. Bounds describe only ordinary map faces.
        const auto *world=SV_MapWorld();
        cullWorld=!M_CheckParm((char*)"-noworldculling")&&!reflectionActive&&!settings.glassReflections&&
            !captures&&!bakesPending()&&!loadingOnThisThread()&&!P_GlobalDigEnabled()&&!digMeshEnabled()&&
            !(world&&world->cutCount())&&floorBounds.size()==(size_t)numsubsectors&&wallBounds.size()==(size_t)numlines;
        worldFrustum=doom_world_frustum::Frustum({{camera.eye[0],camera.eye[1],camera.eye[2]},
            {camera.right[0],camera.right[1],camera.right[2]},{camera.up[0],camera.up[1],camera.up[2]},
            {camera.forward[0],camera.forward[1],camera.forward[2]},camera.projection[0],camera.projection[1],camera.projection[2]});
        timing(2);
        if (!buildSolidScene(fraction))
        {
            flatGeometry(skyTop);
            timing(3);
            wallGeometry(camera, skyTop, captures);
            std::stable_sort(translucentWorld.begin(), translucentWorld.end(), [](const auto &a, const auto &b)
                             { return a.distance > b.distance; });
            buildDigMesh();
            buildSolidMapMesh();
        }
        timing(4);
        if(visibilityTrace||profile.enabled) {
            size_t triangles=skyVertices.size()/3;
            for(const auto &b:batches)triangles+=b.second.size()/3;
            for(const auto &b:transferredSkies)triangles+=b.second.size()/3;
            for(const auto &b:translucentWorld)triangles+=b.vertices.size()/3;
            profile.frame.worldSubsectors=processedLeaves;profile.frame.worldRejectedSubsectors=rejectedLeaves;
            profile.frame.worldLines=processedLines;profile.frame.worldRejectedLines=rejectedLines;
            profile.frame.worldTriangles=(int)triangles;
            profile.frame.worldGeometryMs=(SDL_GetTicksNS()-visibilityStart)/1e6;
            if(visibilityTrace)fprintf(stderr,"World visibility tic %d: enabled %d subsectors %u rejected %u lines %u rejected %u triangles %zu geometry %.3f ms\n",
                gametic,cullWorld,processedLeaves,rejectedLeaves,processedLines,rejectedLines,triangles,profile.frame.worldGeometryMs);
        }
        collectSurfaceLights();
        timing(5);
        buildImpacts(camera);
        buildGlass(camera);
        timing(6);
        static std::vector<Caster> casters;
        casters.clear();
        const Flash *beam = settings.flashlightShadows && flashlightOn && camera.effects[2] == 0 && flashes.count && flashes.lights[0].direction[3] > 0 ? &flashes.lights[0] : nullptr;
        uint64_t spritesStart = profileStart();
        // The glass mirror's sprites: every thing on the eye's side of its plane.
        MirrorView mirror;
        if (glassMirrorActive && camera.effects[2] == 0)
        {
            const float *n = glassMirrorPlane;
            float side = n[0] * camera.eye[0] + n[1] * camera.eye[1] - n[3], along = n[0] * camera.right[0] + n[1] * camera.right[1];
            mirror = {{camera.eye[0] - 2 * side * n[0], camera.eye[1] - 2 * side * n[1]}, {2 * along * n[0] - camera.right[0], 2 * along * n[1] - camera.right[1]}};
        }
        bool mirrored = glassMirrorActive && camera.effects[2] == 0;
        static std::vector<ThingLighting> lightingJobs;
        lightingJobs.clear();
        const bool vileFx = vileEffectsEnabled() && camera.effects[2] == 0;
        bool hasVileActors = false;
        mobj_t *liveViewer = nullptr;
        for (int i = 0; i < numsectors; ++i)
            for (mobj_t *thing = sectors[i].thinglist; thing; thing = thing->snext)
            {
                if (vileFx && thing == players[displayplayer].mo)
                    liveViewer = thing;
                bool inFront = mirrored && glassMirrorPlane[0] * units(thing->x) + glassMirrorPlane[1] * units(thing->y) > glassMirrorPlane[3];
                if ((thing == players[displayplayer].mo && !inFront && !director_view_active) || thing->sprite < 0 || thing->sprite >= numsprites ||
                    (thing->frame & FF_FRAMEMASK) >= sprites[thing->sprite].numframes)
                    continue;
                ThingLighting item{};
                item.thing = thing;
                item.sector = i;
                item.mirrored = inFront;
                interpolatedPosition(*thing, fraction, item.x, item.y, item.z);
                lightingJobs.push_back(item);
                if (vileFx && (thing->type == MT_FIRE || thing->type == MT_VILE))
                    hasVileActors = true;
            }
        filterStats = {};
        filterStats.examined = lightingJobs.size();
        if (!settings.spriteFiltering)
            filterStats.fallback[SVF_DISABLED] = lightingJobs.size();
        else if (mirrored || reflectionActive || beam || director_view_active || camera.effects[2] != 0)
            filterStats.fallback[SVF_SECONDARY] = lightingJobs.size();
        else
        {
            uint64_t begin = SDL_GetTicksNS();
            for (auto &v : lightingJobs)
                v.culled = filterThing(v, camera, fraction);
            filterStats.filter_ns = SDL_GetTicksNS() - begin;
        }
        filterStats.retained = lightingJobs.size() - filterStats.rejected;
        auto calculate = [&](int n)
        { lightThing(lightingJobs[n]); };
        if (lightingJobs.size() >= 32)
            frameParallelFor((int)lightingJobs.size(), calculate);
        else
            for (int n = 0; n < (int)lightingJobs.size(); ++n)
                calculate(n);
        if (M_CheckParm((char *)"-verifylightthreads"))
            for (const auto &item : lightingJobs)
            {
                if (item.culled)
                    continue;
                auto serial = item;
                lightThing(serial);
                if (serial.pulse != item.pulse || serial.beam != item.beam || serial.baked != item.baked ||
                    memcmp(&serial.statics, &item.statics, sizeof(StaticSample)) ||
                    memcmp(&serial.shadows, &item.shadows, sizeof(ShadowLights)))
                    I_Error((char *)"Parallel sprite lighting differs from serial lighting");
            }
        decorationAmbienceBegin(camera);
        for (const auto &item : lightingJobs)
        {
            if (item.culled)
                filteredWoundView(item, camera);
            else
            {
                addThing(item.thing, item.sector, camera, fraction, beam, casters, item.mirrored ? &mirror : nullptr, item);
                if (!item.mirrored)
                    decorationAmbienceThing(item.thing, camera);
            }
        }
        drawDecorationCritters(camera);
        drawWallAmbience(camera);
        // addThing queues enhanced monster shots; draw them only after the
        // actor pass, and after buildImpacts has cleared the FX batches.
        drawMonsterProjectileEffects(camera);
        if (hasVileActors)
        {
            // Resolve gameplay links against the current sector lists before
            // dereferencing them. The overlays own no actor references.
            auto live = [&](mobj_t *actor) -> mobj_t *
            {
                if (!actor || actor == liveViewer)
                    return actor;
                auto found = std::find_if(lightingJobs.begin(), lightingJobs.end(),
                    [actor](const ThingLighting &item) { return item.thing == actor; });
                return found == lightingJobs.end() ? nullptr : found->thing;
            };
            int handChecks = 8, fireChecks = 8;
            for (const auto &item : lightingJobs)
            {
                mobj_t &actor = *item.thing;
                if (item.culled || (actor.type != MT_VILE && actor.type != MT_FIRE) ||
                    !vileEffectNear(actor, camera))
                    continue;
                if (actor.type == MT_VILE && actor.health > 0 && actor.target && handChecks > 0)
                {
                    --handChecks;
                    drawVileHands(actor, live(actor.target), camera, fraction);
                }
                if (actor.type == MT_FIRE && actor.target && actor.tracer && fireChecks > 0)
                {
                    --fireChecks;
                    const mobj_t *vile = live(actor.target), *victim = live(actor.tracer);
                    if (vile && victim)
                        drawVileFire(actor, *vile, *victim, camera, fraction);
                }
            }
        }
        if (vileFx)
            drawVileBursts(camera, fraction);
        if (settings.revenantSoulRelease)
            drawRevenantSouls(camera, fraction);
        timing(7);
        profileAdd(profile.frame.spritesMs, spritesStart);
        std::sort(casters.begin(), casters.end(), [](const Caster &a, const Caster &b)
                  { return a.distance < b.distance; });
        for (size_t n = 0; n < std::min(size_t(6), casters.size()); ++n)
        {
            const Caster &c = casters[n];
            flashlightSilhouette(*c.thing, *c.image, c.lump, c.flip, c.x, c.y, c.z, camera, *beam,
                                 c.card.key ? &c.card : nullptr, c.roll, c.pivot, c.pivotDown);
        }
        playerShadow(camera, fraction);
        timing(8);
        buildBlood(camera);
        buildHeat(camera);
        buildDust(camera);
        buildEmbers(camera);
        buildOutdoorWind(camera);
        buildRain(camera);
        buildEpisodeAtmosphere(camera);
        buildInfernoTreeStrikes(camera);
        buildHellfire(camera);
        buildBubbles(camera);
        timing(9);
        if(lightingTrace) {
            fprintf(stderr,"Lighting stages tic %d: flashes %.3f flats %.3f surface %.3f actors %.3f ms\n",gametic,current[0],current[1],current[5],current[7]);
            reportStaticSamples();
        }
        if (spike)
        {
            double sum = 0;
            for (double t : current)
                sum += t;
            if (sum > 8)
                fprintf(stderr, "Geometry spike tic %d: total %.3f collect %.3f flatlights %.3f visibility %.3f flats %.3f walls %.3f surface %.3f glass %.3f actors %.3f shadows %.3f effects %.3f ms\n", gametic, sum, current[0], current[1], current[2], current[3], current[4], current[5], current[6], current[7], current[8], current[9]);
        }
        if (detailed && ++frames == 128)
        {
            fprintf(stderr, "Geometry ms: collect %.3f flatsources %.3f visibility %.3f flats %.3f walls %.3f surface %.3f glass %.3f actors %.3f shadows %.3f effects %.3f\n",
                    totals[0] / frames, totals[1] / frames, totals[2] / frames, totals[3] / frames, totals[4] / frames, totals[5] / frames, totals[6] / frames, totals[7] / frames, totals[8] / frames, totals[9] / frames);
            totals = {};
            frames = 0;
        }
    }
} // namespace doom3d

extern "C" SpriteVisibilityStats I_SpriteVisibilityStats(void) { return doom3d::filterStats; }
