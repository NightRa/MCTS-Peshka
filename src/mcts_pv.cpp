#include <sstream>
#include "mcts_pv.h"
#include "timeman.h"
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

std::string mcts_pv_print(MCTS_Node& root) {
    std::stringstream ss;
    int elapsed = Time.elapsed() + 1;
    int64_t nodes_searched = Threads.nodes_searched();

    MCTS_PV pv = mctsPv(root);

    if (pv.depth <= 1)
        return "";

    Value v = Value(pv.score); // Same scale as Eval::evaluate - in pawns.

    bool tb = TB::RootInTB && abs(v) < VALUE_MATE - MAX_PLY;
    v = tb ? TB::Score : v;

    if (ss.rdbuf()->in_avail()) // Not at first line
        ss << "\n";

    ss << "info"
       << " depth " << pv.depth / ONE_PLY
       << " seldepth " << pv.depth
       << " multipv " << 1
       << " score " << UCI::value(v);

    ss << " nodes " << nodes_searched
       << " nps " << nodes_searched * 1000 / elapsed;

    ss << " tbhits " << TB::Hits
       << " time " << elapsed
       << " pv";

    for (Move m : pv.moves)
        ss << " " << UCI::move(m, false);

    return ss.str();
}

MCTS_PV mctsPv(MCTS_Node& node) {
    if (!node.fully_opened() /*leaf*/ || node.totalVisits < Search::pvThreshold) {
        return MCTS_PV(std::vector<Move>(0), 0 /*changed in rec*/, Time.elapsed() + 1, 0, Threads.nodes_searched());
    }
    auto best = std::max_element(node.edges.begin(), node.edges.end(), [](MCTS_Edge edge){ return edge.numRollouts; });
    MCTS_Edge& bestEdge = *best;
    MCTS_PV childPv = mctsPv(bestEdge.node);
    childPv.depth++;
    childPv.moves.push_back(bestEdge.move);
    if (node.incoming_edge == nullptr)
        childPv.score = bestEdge.score();
    return childPv;
}

// check_time() is used to print debug info and, more importantly, to detect
// when we are out of available time and thus stop the search.

void mcts_check_time() {

    static TimePoint lastInfoTime = now();

    int elapsed = Time.elapsed();
    TimePoint tick = Search::Limits.startTime + elapsed;

    if (tick - lastInfoTime >= 1000)
    {
        lastInfoTime = tick;
        dbg_print();
    }

    // An engine may not stop pondering until told so by the GUI
    if (Search::Limits.ponder)
        return;

    if (   (Search::Limits.use_time_management() && elapsed > Time.maximum() - 10)
           || (Search::Limits.movetime && elapsed >= Search::Limits.movetime)
           || (Search::Limits.nodes && Threads.nodes_searched() >= Search::Limits.nodes))
        Search::Signals.stop = true;
}
