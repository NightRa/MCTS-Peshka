#include <syzygy/tbprobe.h>
#include "mcts_tablebase.h"
#include "uci.h"

namespace Tablebases {
    int Cardinality;
    uint64_t Hits;
    bool RootInTB;
    bool UseRule50;
    Depth ProbeDepth;
    Value Score;
}

namespace TB = Tablebases;

bool isInTableBase(Position& pos, PlayingResult* playingResult) {
    if (pos.count<ALL_PIECES>(WHITE) + pos.count<ALL_PIECES>(BLACK) > TB::Cardinality)
        return false;
    int found;                       //  =>
    int v = Tablebases::probe_wdl(pos, &found);

    if (!found)
        return false;
    else {
        int drawScore = TB::UseRule50 ? 1 : 0;

        *playingResult = v < -drawScore     ? Lose
                                            : v > drawScore ? Win
                                                            :                 Tie;
        return true;
    }
}

void initTableBase() {
    TB::Hits = 0;
    TB::RootInTB = false;
    TB::UseRule50 = Options["Syzygy50MoveRule"];
    TB::ProbeDepth = Options["SyzygyProbeDepth"] * ONE_PLY;
    TB::Cardinality = Options["SyzygyProbeLimit"];

    // Skip TB probing when no TB found: !TBLargest -> !TB::Cardinality
    if (TB::Cardinality > TB::MaxCardinality) {
        TB::Cardinality = TB::MaxCardinality;
        TB::ProbeDepth = DEPTH_ZERO;
    }
}
