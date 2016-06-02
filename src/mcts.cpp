#include <cmath>
#include <c++/5.2.0/algorithm>
#include <syzygy/tbprobe.h>
#include "mcts.h"
#include "uci.h"
namespace TB = Tablebases;

namespace Search {
    const double cpuct = 1;

    bool is_terminal(Position &position);

    PlayingResult getGameResult(Position &pos, MCTS_Node *node, bool isCheck);

    MCTS_Edge *select_child_UCT(MCTS_Node *node);

    void initTableBase();
    bool canGetResult(Position &position, PlayingResult *playingResult);

    double mctsSearch(Position &pos, MCTS_Node &root) {
        // Updated by check_time()
        initTableBase();
        int iteration = 0;
        while (!Signals.stop) {
            iteration++;

            MCTS_Node *node = &root;

            int rolloutResult;
            double evalResult;
            bool isCheck = false; //fill with something!
            PlayingResult gameResult = getGameResult(pos, node, isCheck);

            while (gameResult == ContinueGame && !node->fully_opened()) {
                MCTS_Edge *child = select_child_UCT(node);
                node = &child->node;
                pos.do_move(child->move, st, gives_check);
                gameResult = getGameResult(pos, node, isCheck);
            }
            if (gameResult != ContinueGame) {

                rolloutResult = gameResult;
                evalResult = rolloutResult;

            } else { // at leaf / not fully opened.
                MCTS_Edge *childEdge = node->open_child(pos);


                node = &childEdge->node;
                pos.do_move(childEdge->move, st, gives_check);

                rolloutResult = rollout(pos);
                evalResult = eval(pos, pos.side_to_move());
            }

            // Back propagation
            while (node->incoming_edge != nullptr) {
                node->incoming_edge->update_stats(rolloutResult, evalResult);
                pos.undo_move(node->incoming_edge->move);
                node = node->incoming_edge->parent;
                // Update max stats in the parent
                node->update_child_stats(rolloutResult, evalResult);
                // Nega-max
                rolloutResult = -rolloutResult;
                evalResult = -evalResult;
            }
        }
    }


    PlayingResult getGameResult(Position &pos, MCTS_Node *node, bool isCheck) {
        PlayingResult res;
        if (canGetResult(pos, &res))
            return res;

        if ((pos.pieces() & ~pos.pieces(PAWN) & ~pos.pieces(KING)) != 0)
            return Win;
        if (node->edges.size() + node->unopened_moves.size() == 0) {
            if (isCheck)
                return Lose;
            else
                return Tie;
        }

        if (pos.is_draw())
            return Tie;
        return ContinueGame;
    }

    bool canGetResult(Position &pos, PlayingResult *playingResult) {
        if (pos.count<ALL_PIECES>(WHITE) + pos.count<ALL_PIECES>(BLACK) > TB::Cardinality)
            return false;
        int found, v = Tablebases::probe_wdl(pos, &found);

        if (!found)
            return false;
        else {
            int drawScore = TB::UseRule50 ? 1 : 0;

            *playingResult = v < -drawScore ? Lose
                           : v > drawScore  ? Win
                           :                  Tie;
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
        if (TB::Cardinality > TB::MaxCardinality)
        {
            TB::Cardinality = TB::MaxCardinality;
            TB::ProbeDepth = DEPTH_ZERO;
        }
    }

    MCTS_Edge *select_child_UCT(MCTS_Node *node) {
        // attest( ! children.empty() );
        double max_UCT_score = -VALUE_INFINITE;
        MCTS_Edge *bestEdge = nullptr;
        for (MCTS_Edge &child: node->edges) {
            double score = child.overallEval +
                           cpuct * child.prior * std::sqrt(node->totalVisits) / (1 + child.numRollouts);
            if (score > max_UCT_score) {
                max_UCT_score = score;
                bestEdge = &child;
            }
        }

        return bestEdge;
    }
}
