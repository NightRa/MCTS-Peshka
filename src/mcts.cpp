#include <cmath>
#include <algorithm>
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

    bool isTerminal(Position &position, PlayingResult *playingResult);

    void do_move_mcts(Position &pos, MCTS_Node *&node, StateInfo *&currentSt, MCTS_Edge *childEdge);

    void undo_move_mcts(Position &pos, MCTS_Node *&node, StateInfo *&currentSt);

    void fillVectorWithMoves(Position &pos,std::vector<ExtMove> moves);

    PlayingResult rollout(Position &position, StateInfo *&currentStateInfo, StateInfo *endingStateInfo);

    double mctsSearch(Position &pos, MCTS_Node &root) {
        // Updated by check_time()
        initTableBase();

        StateInfo sts[MAX_PLY];
        StateInfo *lastSt = sts + MAX_PLY;


        int iteration = 0;
        while (!Signals.stop) {
            iteration++;

            MCTS_Node *node = &root;
            StateInfo *currentSt = sts;

            PlayingResult rolloutResult;
            double evalResult;

            bool isCheck = false; //fill with something!
            PlayingResult gameResult = getGameResult(pos, node, isCheck);

            while (gameResult == ContinueGame && !node->fully_opened()) {
                MCTS_Edge *child = select_child_UCT(node);
                do_move_mcts(pos, node, currentSt, child);

                // If we reach the maximum depth, assume repeat or whatever.
                if (currentSt == lastSt) {
                    gameResult = Tie;
                } else {
                    gameResult = getGameResult(pos, node, isCheck);
                }
            }

            if (gameResult != ContinueGame) {

                rolloutResult = gameResult;
                evalResult = rolloutResult;

            } else { // at leaf = not fully opened.
                MCTS_Edge *childEdge = node->open_child(pos);

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

                MCTS_Edge *childEdge = node->incoming_edge;

                undo_move_mcts(pos, node, currentSt);

                // Update max stats in the parent
                node->update_child_stats(childEdge);
                // Nega-max
                rolloutResult = -rolloutResult;
                evalResult = -evalResult;
            }
        }
    }

    PlayingResult rollout(Position &position, StateInfo *&currentStateInfo, StateInfo *lastStateInfo) {
        PlayingResult result;
        ExtMove moveBuffer[128];
        Move movesDone[MAX_PLY];
        int filled = 0;
        while (!isTerminal(position, &result)){
            ExtMove* startingMove = moveBuffer;
            ExtMove* endingMove = generate<LEGAL>(position, startingMove);
            Move chosenMove = sampleMove(startingMove, endingMove - startingMove);
            position.do_move(chosenMove, *currentStateInfo, (bool)position.checkers());
            movesDone[filled] = chosenMove;
            filled++;
            currentStateInfo++;
            if (currentStateInfo == lastStateInfo){
                result = Tie;
                break;
            }
        }
        for (int i = filled - 1; i >= 0; i--) {
            currentStateInfo--;
            position.undo_move(movesDone[i]);
        }

        return result;
    }

    void do_move_mcts(Position &pos, MCTS_Node *&node, StateInfo *&currentSt, MCTS_Edge *childEdge) {
        node = &childEdge->node;
        pos.do_move(childEdge->move, *currentSt, pos.gives_check(childEdge->move, CheckInfo(pos)));
        currentSt++;
    }

    void undo_move_mcts(Position &pos, MCTS_Node *&node, StateInfo *&currentSt) {
        pos.undo_move(node->incoming_edge->move);
        node = node->incoming_edge->parent;
        currentSt--;
    }

    Bitboard promotedPieces(Position& pos) {
        return pos.pieces() & ~pos.pieces(PAWN) & ~pos.pieces(KING);
    }

    PlayingResult getGameResult(Position &pos, MCTS_Node *node, bool isCheck) {
        PlayingResult res;
        if (isTerminal(pos, &res))
            return res;

        Bitboard promoted = promotedPieces(pos);
        Color sideToMove = pos.side_to_move();
        Bitboard ourPromoted = pos.pieces(sideToMove) & promoted;

        if (ourPromoted)
            return Win;
        if (promoted /*&& !ourPromoted*/) {
            return Lose;
        }

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

    bool isTerminal(Position &pos, PlayingResult *playingResult) {
        if (pos.count<ALL_PIECES>(WHITE) + pos.count<ALL_PIECES>(BLACK) > TB::Cardinality)
            return false;
        int found, v = Tablebases::probe_wdl(pos, &found);

        if (!found)
            return false;
        else {
            int drawScore = TB::UseRule50 ? 1 : 0;

            *playingResult = v < -drawScore ? Lose
                           : v >  drawScore ? Win
                                            : Tie;
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

    // MoveBuffer is a 128 cells of ExtMoves that should be empty.
    void fillVectorWithMoves(Position &pos, std::vector<ExtMove>& moveVector, ExtMove* moveBuffer){
        ExtMove* startingPointer = moveBuffer;
        ExtMove* endingPointer = generate<LEGAL>(pos, moveBuffer);
        while (startingPointer != endingPointer){
            moveVector.push_back(*startingPointer);
            startingPointer++;
        }
    }

    Move sampleMove(ExtMove* moves, int size) {
        long long int seed = std::chrono::system_clock::now().time_since_epoch().count();
        std::mersenne_twister_engine generator (seed);

        const int smoothing = 50;

        Value min = VALUE_INFINITE;
        for (int i = 0; i < size; i++) {
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
