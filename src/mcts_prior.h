#ifndef SRC_MCTS_PRIOR_H
#define SRC_MCTS_PRIOR_H

#include "movegen.h"
#include "position.h"

struct UnopenedMove {
    Move move;
    float expPrior;
    float relativePrior;
    float prior; // In [0, 1], devided by the sum of all e^x

    UnopenedMove(Move _move, float _expPrior, float _prior) : move(_move), expPrior(_expPrior), prior(_prior) {}
    UnopenedMove() = default;

    bool operator==(const UnopenedMove& other) {
        return move == other.move;
    }
};

void calc_exp_evals(Position& pos, ExtMove* moves, int size);
void calc_priors(Position& pos, ExtMove* moves, int size);
Move sampleMove(Position& pos, ExtMove* moves);
UnopenedMove sampleMove(Position& pos, std::vector<UnopenedMove>& moves);
Value safeEval(Position& pos, Move move /*player*/, CheckInfo& ci /*already computed for pos*/, StateInfo& st);
Value qeval(Position& pos);

#endif //SRC_MCTS_PRIOR_H
