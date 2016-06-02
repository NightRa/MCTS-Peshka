#ifndef SRC_MCTS_H
#define SRC_MCTS_H

#include <vector>
#include "types.h"
#include "position.h"
#include "movepick.h"

typedef int NumVisits;
typedef double EvalType;    // first check whether things work and only then change it to float

class MCTS_Node;

struct UnopenedMove {
    Move move;
    float prior;

};

struct MCTS_Edge {
    MCTS_Node node;
    Move move;
    float prior;
    EvalType evalSum;
    NumVisits numEvals;
    NumVisits rolloutsSum;
    NumVisits numRollouts;
    float overallEval;
    MCTS_Node *parent;

    // Don't forget to initialize overallEval with the Prior

    void update_stats(int rolloutResult, double evalResult) {
        rolloutsSum += rolloutResult;
        numRollouts += 1;
        evalSum += evalResult;
        numEvals += 1;
        overallEval = compute_overall_eval();
    }

private:
    float compute_overall_eval() {
        return (0.5f * (float(rolloutsSum) / float(numRollouts)) + 0.5f * (float(evalSum) / float(numEvals)));
    }
};

class MCTS_Node {
public:
    // States: Uninitialized - Edges empty & unopened moves empty
    //         Not fully opened - Edges not empty & unopened moves not empty
    //         Fully opened - Edges not empty & unopened moves empty
    std::vector<MCTS_Edge> edges;
    std::vector<UnopenedMove> unopened_moves;
    NumVisits maxVisits;
    NumVisits totalVisits;
    MCTS_Edge *incoming_edge;
public:
    MCTS_Node() : MCTS_Node(nullptr) {}

    MCTS_Node(MCTS_Edge *parent) : edges(0 /*Init with size 0*/), unopened_moves(0), maxVisits(0), totalVisits(0),
                                   incoming_edge(parent) {}

    ~MCTS_Node() {
        if (edges != nullptr) {
            delete[] edges;
        }
    }

    inline bool fully_opened() {
        return !edges.empty() && unopened_moves.empty();
    }

    void update_child_stats(MCTS_Edge *childEdge) {
        totalVisits++;
        maxVisits = std::max(maxVisits, childEdge->numRollouts);
    }

    void generate_moves(Position &pos) {
        MovePicker mp(pos, ttMove, thisThread->history, PieceValue[MG][pos.captured_piece_type()]);
        CheckInfo ci(pos);
        Move move;
        while ((move = mp.next_move()) != MOVE_NONE) {
            if (pos.legal(move, ci.pinned)) {
                // ss->currentMove = move;
                pos.do_move(move, st, pos.gives_check(move, ci));
                // value = -search<NonPV>(pos, ss+1, -rbeta, -rbeta+1, rdepth, !cutNode);
                pos.undo_move(move);
                // if (value >= rbeta)
                //    return value;
            }
        }

    };

    MCTS_Edge *open_child(Position &pos) {
        // Precondition: not terminal => possible moves not empty
        if (edges.empty() && unopened_moves.empty()) {
            generate_moves(pos);
            // Generate moves, unopened_moves not empty.
        }
        // unopened_moves not empty.
        // sample move according to prior probabilities / take maximal probability
        // remove it from unopened_moves and insert to edges.

    }
};

enum PlayingResult {
    ContinueGame = 2,
    Win = 1,
    Lose = -1,
    Tie = 0
};

#endif //SRC_MCTS_H
