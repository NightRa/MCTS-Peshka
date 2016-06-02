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
        ExtMove moveBuffer[128];

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

    void calc_priors(Position& pos, ExtMove* moves, int size);

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
            Move chosenMove = sampleMove(position, startingMove, movesSize);
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

    // MoveBuffer is a 128 cells of ExtMoves that should be empty.
    void fillVectorWithMoves(Position& pos, std::vector<ExtMove>& moveVector, ExtMove* moveBuffer) {
        ExtMove* startingPointer = moveBuffer;
        ExtMove* endingPointer = generate<LEGAL>(pos, moveBuffer);
        while (startingPointer != endingPointer) {
            moveVector.push_back(*startingPointer);
            startingPointer++;
        }
    }

    void calc_priors(Position& pos, ExtMove* moves, int size) {
        const float normalizationFactor = 200; // Something like a pawn
        StateInfo st;
        const CheckInfo ci(pos);
        float max = -VALUE_INFINITE;
        // u.f is always the real value, u.i is always the stored repr.
        for (int i = 0; i < size; i++) {
            // calculate move values (heuristics)
            pos.do_move(moves[i].move, st, pos.gives_check(moves[i], ci));
            float eval = float(Eval::evaluate<false>(pos)) / normalizationFactor;
            moves[i].setPrior(eval);
            pos.undo_move(moves[i]);

            max = std::max(max, eval);
        }

        double expSum = 0;
        for (int i = 0; i < size; i++) {
            float eval = std::exp(moves[i].getPrior() - max);
            moves[i].setPrior(eval);

            expSum += eval;
        }

        for (int i = 0; i < size; i++) {
            double eval = double(moves[i].getPrior()) / expSum;
            moves[i].setPrior(float(eval));
        }
    }

    Move sampleMove(Position& pos, ExtMove* moves, int size) {

        long long int seed = std::chrono::system_clock::now().time_since_epoch().count();
        std::mersenne_twister_engine generator(seed);

        std::uniform_real_distribution<float> distribution(0.0, 1.0);

        float stopPoint = distribution(generator);

        float partialSum = 0;
        int i = 0;
        while (partialSum <= stopPoint) {
            partialSum += moves[i].getPrior();
            i++;
        }

        return moves[i - 1];
    }
}
