#ifndef SRC_MCTS_H
#define SRC_MCTS_H

#include <vector>
#include "types.h"

typedef int NumVisits;
typedef double EvalType;    // first check whther things work and only then change it to float

class MCTS_Node;

struct MCTS_Edge {
    MCTS_Node node;
    Move move;
    float prior;
    EvalType evalSum;
    NumVisits numEvals;
    NumVisits rolloutsSum;
    NumVisits numRollouts;
    float overallEval;
    MCTS_Node* parent;

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
    std::vector<MCTS_Edge> edges;
    std::vector<Move> unopened_moves;
    NumVisits maxVisits;
    NumVisits totalVisits;
    MCTS_Edge* incoming_edge;
public:
    MCTS_Node() : MCTS_Node(nullptr) {}
    MCTS_Node(MCTS_Edge* parent): edges(0 /*Init with size 0*/), unopened_moves(0), maxVisits(0), totalVisits(0), incoming_edge(parent) {}

    ~MCTS_Node() {
        if(edges != nullptr) {
            delete[] edges;
        }
    }

    inline bool fully_opened() {
        return unopened_moves.empty();
    }
};

enum PlayingResult {
    ContinueGame = 2,
    Win = 1,
    Lose = -1,
    Tie = 0
};

#endif //SRC_MCTS_H
