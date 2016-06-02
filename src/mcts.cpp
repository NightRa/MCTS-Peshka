#include <cmath>
#include <algorithm>
#include <syzygy/tbprobe.h>
#include "mcts.h"
#include "uci.h"
#include "evaluate.h"
#include "mcts_chess_playing.h"
#include "mcts_tablebase.h"


namespace Search {
    const double cpuct = 1;

    MCTS_Edge* select_child_UCT(MCTS_Node* node);
    void do_move_mcts(Position& pos, MCTS_Node*& node, StateInfo*& currentSt, MCTS_Edge* childEdge);
    void undo_move_mcts(Position& pos, MCTS_Node*& node, StateInfo*& currentSt);
    PlayingResult rollout(Position& position, StateInfo*& currentStateInfo, StateInfo* endingStateInfo);
    Move sampleMove(ExtMove* moves, int size);

    double mctsSearch(Position& pos, MCTS_Node& root) {
        // Updated by check_time()
        initTableBase();

        StateInfo sts[MAX_PLY];
        StateInfo* lastSt = sts + MAX_PLY;

        int iteration = 0;
        while (!Signals.stop) {
            iteration++;

            MCTS_Node* node = &root;
            StateInfo* currentSt = sts;

            PlayingResult rolloutResult;
            double evalResult;

            bool isCheck = false; //fill with something!
            PlayingResult gameResult = getGameResult(pos, node->unopened_moves.size() + node->unopened_moves.size());

            while (gameResult == ContinueGame && !node->fully_opened()) {
                MCTS_Edge* child = select_child_UCT(node);
                do_move_mcts(pos, node, currentSt, child);

                // If we reach the maximum depth, assume repeat or whatever.
                if (currentSt == lastSt) {
                    gameResult = Tie;

            } else {
                gameResult = getGameResult(pos, node->unopened_moves.size() + node->unopened_moves.size());
            }
        }

        if (gameResult != ContinueGame) {

            rolloutResult = gameResult;
            evalResult = rolloutResult;
            } else { // at leaf = not fully opened.
                MCTS_Edge* childEdge = node->open_child(pos);

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
            Move chosenMove = sampleMove(startingMove, int(endingMove - startingMove));
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

    Move sampleMove(Position& pos, ExtMove* moves, int size) {
        long long int seed = std::chrono::system_clock::now().time_since_epoch().count();
        std::mersenne_twister_engine generator(seed);

        StateInfo st;
        const int smoothing = 50;

        const CheckInfo ci(pos);

        Value min = VALUE_INFINITE;
        for (int i = 0; i < size; i++) {
            // calculate move values (heuristics)
            pos.do_move(moves[i].move, st, pos.gives_check(moves[i], ci));
            moves[i].value = Eval::evaluate<false>(pos);
            pos.undo_move(moves[i]);

            min = std::min(min, moves[i].value);
        }

        // x -> x - min + smoothing

        int sum = 0;
        for (int i = 0; i < size; i++) {
            moves[i].value = moves[i].value - min + smoothing;
            sum += moves[i].value;
        }

        std::uniform_int_distribution<int> distribution(0, sum - 1);

        int stopPoint = distribution(generator);

        int partialSum = 0;
        int i = 0;
        while (partialSum <= stopPoint) {
            partialSum += moves[i].value;
            i++;
        }

        return moves[i - 1];
    }
}
