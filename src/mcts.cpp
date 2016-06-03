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

namespace Search {

    double eval(Position& pos);

    void mctsSearch(Position& pos, MCTS_Node& root) {
        const int printEvery = 1;
        // Updated by check_time()
        initTableBase();

        StateInfo sts[MAX_PLY];
        StateInfo* lastSt = sts + MAX_PLY;
        ExtMove moveBuffer[128];

        int iteration = 0;
        while (!Signals.stop) {
            std::cout << "all ok.."  << iteration << std::endl;
            MCTS_Node* node = &root;
            StateInfo* currentSt = sts;

            int rolloutResult;
            double evalResult;

            int numMoves = node->getNumMoves(pos, moveBuffer);
            PlayingResult gameResult = getGameResult(pos, numMoves);
            std::cout << "1" << std::endl;
            while (gameResult == ContinueGame && !node->isLeaf()) {
                MCTS_Edge* child = select_child_UCT(node);
                do_move_mcts(pos, node, currentSt, child);

                // If we reach the maximum depth, assume repeat or whatever.
                if (currentSt == lastSt) {
                    gameResult = Tie;

                } else {
                    gameResult = getGameResult(pos, node->getNumMoves(pos, moveBuffer));
                }
            }
            std::cout << "2" << std::endl;
            if (gameResult != ContinueGame) {

                rolloutResult = gameResult;
                evalResult = rolloutResult;

            } else { // at leaf = not fully opened.

                MCTS_Edge* childEdge = node->open_child(pos, moveBuffer);
                std::cout << "3" << std::endl;
                do_move_mcts(pos, node, currentSt, childEdge);
                std::cout << "4" << std::endl;
                if (currentSt == lastSt) {
                    rolloutResult = Tie;
                } else {
                    std::cout << "5" << std::endl;
                    rolloutResult = rollout(pos, currentSt, lastSt, moveBuffer);
                }
                std::cout << "6" << std::endl;
                evalResult = eval(pos);
                std::cout << "7" << std::endl;
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
            if (iteration % printEvery == 0) {
                mcts_check_time();
            }
            if (Time.elapsed() > 1000 && iteration % printEvery == 0) {
                std::cout << mcts_pv_print(root) << std::endl;
                // sync_cout << mcts_pv_print(root) << sync_endl;
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

            std::cout << "Position:" << pos << std::endl;
            calc_priors(pos, startingMove, movesSize);
            std::cout << "After calc_priors, before sampleMove:" << pos << std::endl;

            Move chosenMove = sampleMove(pos, startingMove);
            std::cout << "Chosen move:" << chosenMove << std::endl;
            if (chosenMove == 1048) {
                std::cout << "yo" << std::endl;
            }
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

    void do_move_mcts(Position& pos, MCTS_Node*& node, StateInfo*& currentSt, MCTS_Edge* childEdge) {
        node = &childEdge->node;
        std::cout << pos << std::endl;
        pos.do_move(childEdge->move, *currentSt, pos.gives_check(childEdge->move, CheckInfo(pos)));
        std::cout << pos << std::endl;
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
    UnopenedMove move = sampleMove(pos, unopened_moves.unopened_moves);
    // remove it from unopened_moves and insert to edges.
    unopened_moves.remove(move);
    edges.push_back(MCTS_Edge(move.move, move.prior, this));
    return &edges[edges.size() - 1];
}

MCTS_Edge* MCTS_Node::selectBest() {
    if (!initialized) {
        return nullptr;
    } else {
        NumVisits max_visits = 0;
        MCTS_Edge* maxEdge = nullptr;
        for (MCTS_Edge& edge: edges) {
            if (edge.numRollouts > max_visits) {
                max_visits = edge.numRollouts;
                maxEdge = &edge;
            }
        }
        return maxEdge;
    }
}
