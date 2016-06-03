#include <cmath>
#include <algorithm>
#include <syzygy/tbprobe.h>
#include <iostream>
#include "mcts.h"
#include "uci.h"
#include "evaluate.h"
#include "mcts_tablebase.h"
#include "timeman.h"
#include "mcts_pv.h"

namespace Search {

    double eval(Position& pos);

    void mctsSearch(Position& pos, MCTS_Node& root) {
        // Updated by check_time()
        initTableBase();

        StateInfo sts[MAX_PLY];
        StateInfo* lastSt = sts + MAX_PLY;
        ExtMove moveBuffer[128];

        int iteration = 0;
        while (!Signals.stop) {
            iteration++;

            MCTS_Node* node = &root;
            StateInfo* currentSt = sts;

            PlayingResult rolloutResult;
            double evalResult;

            PlayingResult gameResult = getGameResult(pos, node->getNumMoves(pos, moveBuffer));

            while (gameResult == ContinueGame && !node->fully_opened()) {
                MCTS_Edge* child = select_child_UCT(node);
                do_move_mcts(pos, node, currentSt, child);

                // If we reach the maximum depth, assume repeat or whatever.
                if (currentSt == lastSt) {
                    gameResult = Tie;

                } else {
                    gameResult = getGameResult(pos, node->getNumMoves(pos, moveBuffer));
                }
            }

            if (gameResult != ContinueGame) {

                rolloutResult = gameResult;
                evalResult = rolloutResult;

            } else { // at leaf = not fully opened.

                MCTS_Edge* childEdge = node->open_child(pos, moveBuffer);

                do_move_mcts(pos, node, currentSt, childEdge);

                if (currentSt == lastSt) {
                    rolloutResult = Tie;
                } else {
                    rolloutResult = rollout(pos, currentSt, lastSt);
                }

                evalResult = eval(pos);
            }

            // Back propagation
            while (node->incoming_edge != nullptr) {
                node->incoming_edge->update_stats(rolloutResult, evalResult, evalWeight);

                MCTS_Edge* childEdge = node->incoming_edge;

                undo_move_mcts(pos, node, currentSt);

                // Update max stats in the parent
                node->update_child_stats(childEdge);
                // Nega-max
                rolloutResult = -rolloutResult;
                evalResult = -evalResult;
            }

            // optionally print move
            if (iteration % 100 == 0) {
                mcts_check_time();
            }
            if (Time.elapsed() > 1000 && iteration % 100 == 0) {
                sync_cout << mcts_pv_print(root) << sync_endl;
            }
            // check-out search.cpp line 887
        }
    }


    PlayingResult rollout(Position& position, StateInfo*& currentStateInfo, StateInfo* lastStateInfo) {
        PlayingResult result = getGameResult(position, position.count(WHITE) + position.count(BLACK));
        ExtMove moveBuffer[MAX_PLY];
        Move movesDone[MAX_PLY];
        int filled = 0;
        while (result == ContinueGame) {
            ExtMove* startingMove = moveBuffer;

            ExtMove* endingMove = generate<LEGAL>(position, startingMove);
            int movesSize = int(endingMove - startingMove);
            calc_priors(position, startingMove, movesSize);

            Move chosenMove = sampleMove(position, startingMove);
            position.do_move(chosenMove, *currentStateInfo, position.gives_check(chosenMove, CheckInfo(position)));
            movesDone[filled] = chosenMove;
            filled++;
            currentStateInfo++;
            if (currentStateInfo == lastStateInfo) {
                result = Tie;
                break;
            }
            result = getGameResult(position, position.count(WHITE) + position.count(BLACK));
        }

        for (int i = filled - 1; i >= 0; i--) {
            currentStateInfo--;
            position.undo_move(movesDone[i]);
        }

        // Computed in getGameResult, notice reference above.
        return result;
    }

    void do_move_mcts(Position& pos, MCTS_Node*& node, StateInfo*& currentSt, MCTS_Edge* childEdge) {
        node = &childEdge->node;
        pos.do_move(childEdge->move, *currentSt, pos.gives_check(childEdge->move, CheckInfo(pos)));
        currentSt++;
    }

    void undo_move_mcts(Position& pos, MCTS_Node*& node, StateInfo*& currentSt) {
        pos.undo_move(node->incoming_edge->move);
        node = node->incoming_edge->parent;
        currentSt--;
    }

    MCTS_Edge* select_child_UCT(MCTS_Node* node) {
        // attest( ! children.empty() );
        double max_UCT_score = -VALUE_INFINITE;
        MCTS_Edge* bestEdge = nullptr;
        for (MCTS_Edge& child: node->edges) {
            double score = child.overallEval +
                           cpuct * child.prior * std::sqrt(node->totalVisits) / (1 + child.numRollouts);
            if (score > max_UCT_score) {
                max_UCT_score = score;
                bestEdge = &child;
            }
        }

        return bestEdge;
    }

    double eval(Position& pos) {
        // k = ln(p/(1-p))/delta x => delta x = 30, p = 0.9 => k = 0.073
        Value v = Eval::evaluate(pos);
        if (abs(v) < VALUE_MATE - MAX_PLY) { // not in mate, scale to -0.9 0.9
            const double k = 0.073;
            double pawnValue = double(v) / PawnValueEg; // assume range of [-30,30]
            return (2 / (1 + std::exp(-k * pawnValue))) - 1; // Scaled sigmoid.
        } else { // mate
            return v > 0 ? 1 : -1;
        }
    }


}
