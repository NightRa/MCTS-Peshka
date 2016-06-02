#include <cmath>
#include <algorithm>
#include <syzygy/tbprobe.h>
#include "mcts.h"
#include "uci.h"
#include "evaluate.h"
#include "mcts_tablebase.h"
#include "mcts_prior.h"


namespace Search {
    const double cpuct = 1;

    double mctsSearch(Position& pos, MCTS_Node& root) {
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

            bool isCheck = false; //fill with something!

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

                evalResult = eval(pos, pos.side_to_move());
            }

            // Back propagation
            while (node->incoming_edge != nullptr) {
                node->incoming_edge->update_stats(rolloutResult, evalResult);

                MCTS_Edge* childEdge = node->incoming_edge;

                undo_move_mcts(pos, node, currentSt);

                // Update max stats in the parent
                node->update_child_stats(childEdge);
                // Nega-max
                rolloutResult = -rolloutResult;
                evalResult = -evalResult;
            }
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
}
