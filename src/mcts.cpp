#include <cmath>
#include <algorithm>
#include "syzygy/tbprobe.h"
#include <iostream>
#include "mcts.h"
#include "uci.h"
#include "evaluate.h"
#include "mcts_tablebase.h"
#include "timeman.h"
#include "mcts_pv.h"

using std::sqrt;

namespace Search {
    const bool debug_UCT = false;
    const double cpuct = 0.01;
    const double explorationExponent = 0.5;
    const float evalWeight = 0.0f;
    const int pvThreshold = 7;
    const float normalizationFactor = 200; // Something like a pawn

    double eval(Position& pos);

    void mctsSearch(Position& pos, MCTS_Node& root) {
        const int printEvery = 1000;
        // Updated by check_time()
        initTableBase();

        StateInfo sts[MAX_PLY];
        StateInfo* lastSt = sts + MAX_PLY;
        ExtMove moveBuffer[128];
        MCTS_Node* moveHistoryBuffer[MAX_PLY];
        MCTS_Node** moveHistory = moveHistoryBuffer;

        int iteration = 0;
        while (!Signals.stop) {
            MCTS_Node* node = &root;
            StateInfo* currentSt = sts;

            int rolloutResult;
            double evalResult;

            int numMoves = node->getNumMoves(pos, moveBuffer);
            PlayingResult gameResult = getGameResult(pos, numMoves);
            while (gameResult == ContinueGame && !node->isLeaf()) {
                MCTS_Edge* child = select_child_UCT(node);
                do_move_mcts(pos, node, currentSt, child, moveHistory);

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
                do_move_mcts(pos, node, currentSt, childEdge, moveHistory);
                if (currentSt == lastSt) {
                    rolloutResult = Tie;
                } else {
                    rolloutResult = rollout(pos, currentSt, lastSt, moveBuffer);
                }
                evalResult = eval(pos);
            }

            // Back propagation
            while (node->incoming_edge != nullptr) {
                node->incoming_edge->update_stats(rolloutResult, evalResult, evalWeight);

                MCTS_Edge* childEdge = node->incoming_edge;

                undo_move_mcts(pos, node, currentSt, moveHistory);

                // Update max stats in the parent
                node->update_child_stats(childEdge);
                // Nega-max
                rolloutResult = -rolloutResult;
                evalResult = -evalResult;
            }

            // optionally print move
            if (iteration % printEvery == 0) {
                mcts_check_time();
            }
            if (Time.elapsed() > 1000 && iteration % printEvery == 0) {
                sync_cout << mcts_pv_print(root) << sync_endl;
                if (debug_UCT) {
                    for (MCTS_Edge* edge: root.edges) {
                        sync_cout << UCI::move(edge->move, false) << ": " << edge->numRollouts << " = ";
                        printf("%.2f", edge->overallEval);
                        std::cout << ", ";
                    }
                    std::cout << sync_endl;
                }

            }
            // check-out search.cpp line 887
            iteration++;
        }
    }


    PlayingResult rollout(Position& pos, StateInfo*& currentStateInfo, StateInfo* lastStateInfo, ExtMove* moveBuffer) {
        // TODO: Don't initialize in getNumMoves
        PlayingResult result = getGameResult(pos, getNumMoves(pos, moveBuffer));
        Move movesDone[MAX_PLY];
        int filled = 0;
        while (result == ContinueGame) {
            ExtMove* startingMove = moveBuffer;

            ExtMove* endingMove = generate<LEGAL>(pos, startingMove);
            int movesSize = countValidMoves(startingMove, int(endingMove - startingMove));

            calc_priors(pos, startingMove, movesSize);

            Move chosenMove = sampleMove(pos, startingMove);
            pos.do_move(chosenMove, *currentStateInfo, pos.gives_check(chosenMove, CheckInfo(pos)));
            movesDone[filled] = chosenMove;
            filled++;
            currentStateInfo++;
            if (currentStateInfo == lastStateInfo) {
                result = Tie;
                break;
            }
            result = getGameResult(pos, getNumMoves(pos, moveBuffer));
        }

        for (int i = filled - 1; i >= 0; i--) {
            currentStateInfo--;
            pos.undo_move(movesDone[i]);
        }

        // Computed in getGameResult, notice reference above.
        return result;
    }

    void do_move_mcts(Position& pos, MCTS_Node*& node, StateInfo*& currentSt, MCTS_Edge* childEdge, MCTS_Node**& moveHistory) {
        // Last element in the history is the parent
        *moveHistory = node;
        moveHistory++;

        node = &childEdge->node;
        pos.do_move(childEdge->move, *currentSt, pos.gives_check(childEdge->move, CheckInfo(pos)));
        currentSt++;
    }

    void undo_move_mcts(Position& pos, MCTS_Node*& node, StateInfo*& currentSt, MCTS_Node**& moveHistory) {
        pos.undo_move(node->incoming_edge->move);

        moveHistory--;
        node = *moveHistory; // Last element in the history is the parent

        currentSt--;
    }

    MCTS_Edge* select_child_UCT(MCTS_Node* node) {
        // attest( ! children.empty() );
        double max_UCT_score = -VALUE_INFINITE;
        MCTS_Edge* bestEdge = nullptr;
        for (MCTS_Edge* child: node->edges) {
            double score = child->overallEval +
                           cpuct * child->prior * std::pow(node->totalVisits, explorationExponent) / (1 + child->numRollouts);
            if (score > max_UCT_score) {
                max_UCT_score = score;
                bestEdge = child;
            }
        }

        return bestEdge;
    }

    double eval(Position& pos) {
        // k = ln(p/(1-p))/delta x => delta x = 30, p = 0.9 => k = 0.073
        Value v = qeval(pos);
        if (abs(v) < VALUE_MATE - MAX_PLY) { // not in mate, scale to -0.9 0.9
            const double k = 0.073;
            double pawnValue = double(v) / PawnValueEg; // assume range of [-30,30]
            return (2 / (1 + std::exp(-k * pawnValue))) - 1; // Scaled sigmoid.
        } else { // mate
            return v > 0 ? 1 : -1;
        }
    }


}

void MCTS_Node::update_child_stats(MCTS_Edge* childEdge) {
    totalVisits++;
    maxVisits = std::max(maxVisits, childEdge->numRollouts);
}

MCTS_Edge* MCTS_Node::open_child(Position& pos, ExtMove* moveBuffer) {
    // Precondition: not terminal, leaf => possible moves not empty
    initialize(pos, moveBuffer);
    // unopened_moves not empty.
    // sample move according to prior probabilities / take maximal probability
    UnopenedMove move = sampleMove(pos, unopened_moves.unopened_moves, unopened_moves.numMoves);
    // remove it from unopened_moves and insert to edges.
    unopened_moves.remove(move);
    MCTS_Edge* childEdge = new MCTS_Edge(move.move, move.absolutePrior); // Notice Allocation here!
    edges.push_back(childEdge);
    return edges[edges.size() - 1];
}

MCTS_Edge* MCTS_Node::selectBest() {
    if (!initialized) {
        return nullptr;
    } else {
        NumVisits max_visits = 0;
        MCTS_Edge* maxEdge = nullptr;
        for (MCTS_Edge* edge: edges) {
            if (edge->numRollouts > max_visits) {
                max_visits = edge->numRollouts;
                maxEdge = edge;
            }
        }
        return maxEdge;
    }
}

MCTS_Node::~MCTS_Node() {
    for (MCTS_Edge* child: edges) {
        delete child;
    }
}
