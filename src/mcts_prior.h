#ifndef SRC_MCTS_PRIOR_H
#define SRC_MCTS_PRIOR_H

#include "movegen.h"
#include "position.h"

struct UnopenedMove {
    Move move;
    float expPrior;
    float relativePrior; // In [0, 1], divided by the sum of e^x for all yet unopened moves.
    float absolutePrior; // In [0, 1], divided by the sum of all e^x

    UnopenedMove(Move _move, float _expPrior, float _prior) : move(_move), expPrior(_expPrior), absolutePrior(_prior), relativePrior(_prior) {
        assert(_expPrior > 0);
        assert(_prior > 0);
    }
    UnopenedMove() = default;

    bool operator==(const UnopenedMove& other) {
        return move == other.move;
    }
};

void calc_exp_evals(Position& pos, ExtMove* moves, int size);
void calc_priors(Position& pos, ExtMove* moves, int size);
Move sampleMove(Position& pos, ExtMove* moves);
UnopenedMove sampleMove(Position& pos, UnopenedMove* moves, int numMoves);
Value safeEval(Position& pos, Move move /*player*/, CheckInfo& ci /*already computed for pos*/, StateInfo& st);
Value qeval(Position& pos);

#endif //SRC_MCTS_PRIOR_H
