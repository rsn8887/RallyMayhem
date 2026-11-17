// Rally Mayhem - the tracks
// by rsn8887
//
// A track is a closed spline through control points {x, y, road width, height, flags}.
// Cars are 9x5 pixels; a normal road is about 24 wide.
// Flags apply to the stretch from that point to the next one (fJ and fO act at the point).
#pragma once
#include <vector>

enum { // control point flags
    fR = 1,    // railings on both sides
    fD = 2,    // dirt road
    fI = 4,    // ice road
    fB = 8,    // bridge deck (other roads pass underneath); needs height 10 on both ends
    fJ = 16,   // jump: a ramp rises towards this point and ends here
    fG = 32,   // gap: no road at all on this stretch
    fF = 64,   // ford: water is not bridged, it stays as shallow water
    fK = 128,  // banked curve
    fO = 256,  // turbo boost pad right after this point
    fT = 512,  // tunnel
    fN = 1024, // no automatic railings where the road runs over water
    fC = 2048, // tree tops grow over the road and hide the cars
    fY = 4096, // ferry gap: no road, a raft (O_FERRY) carries the cars across
    fW = 8192  // warp: no road on this stretch, a gate at its start beams the cars to its end
};
enum { TH_SUMMER, TH_SPRING, TH_AUTUMN, TH_WINTER, TH_MEXICO, TH_CITY, TH_VILLAGE, TH_CARNIVAL, TH_F1, TH_MOUNTAIN, TH_GERMANY, TH_SWAMP,
       // the five worlds of tracks 31-50 (several looks each, chosen with O_STYLE):
       TH_SCIFI,   // hovercraft; styles: 0 neon city, 1 tunnel base, 2 outer space, 3 reactor
       TH_WATER,   // boats on open water (O_ISLAND is dry land here)
       TH_CANDY,   // chocolate roads; water is liquid chocolate; O_DONUT needs a road running up or down the screen
       TH_HORROR,  // styles: 0 suburb on Halloween night, 1 summer camp by a slime lake, 2 attic, 3 graveyard
       TH_SPORT }; // styles: 0 billiard table, 1 table tennis, 2 tennis court, 3 athletics stadium
enum { WX_CLEAR, WX_RAIN, WX_SNOW, WX_CONFETTI, WX_STORM };   // WX_STORM: rain with lightning
enum { // scenery objects
    O_LAKE,    // x, y, rx, ry
    O_RIVER,   // x1, y1, x2, y2, width
    O_STAND,   // x, y, w, h   (grandstand with spectators)
    O_HOUSE,   // x, y
    O_TENT,    // x, y
    O_HAY,     // x, y         (solid hay bale)
    O_TREE,    // x, y
    O_ROCK,    // x, y
    O_FLAG,    // x, y
    O_SNOWMAN, // x, y
    O_TRAIN,   // x1, y1, x2, y2, period (s), phase (s)   (horizontal or vertical only)
    O_ICE,     // x, y, rx, ry (ice patch on the road)
    O_DIRT,    // x, y, rx, ry (dirt patch on the road)
    O_PUDDLE,  // x, y, rx, ry (puddle on the road)
    O_BOOST,   // x, y, radius (small round turbo field on the road)
    O_LAMP,    // x, y         (street lamp)
    O_NIGHT,   // night race: only lamps, windows and head lights light the track
    O_WIND,    // dx, dy       (side wind: cars drift this many pixels per second)
    O_FLOOD,   // the whole map is water; use O_ISLAND for dry land
    O_ISLAND,  // x, y, rx, ry
    O_TURNTABLE, // x, y, radius, speed (rad/s)  (rotating platform in the road)
    O_DRAWBRIDGE, // x, y, half length, period (s), phase (s)  (on a road that crosses water)
    O_FERRY,   // x1, y1, x2, y2 (raft centre at the two docks; the road needs an fY stretch there)
    O_STYLE,   // n            (which look of the theme this track has, see the theme list)
    O_VOID,    // lakes, rivers and floods are a bottomless pit instead of water
    O_SLIME,   // x, y, rx, ry (sticky patch on the road that slows the cars down)
    O_ZOMBIES, // count        (zombies, spiders or gummi bears walk about; running one over costs speed)
    O_HURDLE,  // x, y         (hurdle across part of the road; cars knock it over and lose speed)
    O_DONUT,   // x, y, outer radius, inner radius  (giant donut standing half buried across the road: an arch to drive through)
    O_BUMPER,  // x, y, radius (round bumper that kicks the cars away)
    O_BOUNCE,  // x, y, radius (trampoline on the road that throws the cars into the air)
    O_WHIRL,   // x, y, radius (whirlpool that drags boats around)
    O_NET,     // x1, y1, x2, y2 (low barrier: only a car in the air gets across, so it needs a jump)
    O_ROLLER,  // x1, y1, x2, y2, speed, one-way (0/1)  (a ball, ghost... that travels along a line and knocks cars about)
    O_PIER,    // x1, y1, x2, y2 (water world: a wooden pier, horizontal or vertical)
    O_QUAY     // x, y, w, h   (water world: a harbour quay with cranes, containers and moored boats)
};

struct CP { float x, y, w, h; int f; };
struct Obj { int t; float a = 0, b = 0, c = 0, d = 0, e = 0, f = 0; };
struct TrackDef {
    const char* name;
    int theme, weather, trees;
    std::vector<CP> p;
    std::vector<Obj> o;
};

extern const std::vector<TrackDef> TRACKS;   // defined in tracks.cpp
