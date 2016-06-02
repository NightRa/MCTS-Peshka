#ifndef SRC_MCTS_PRIOR_H
#define SRC_MCTS_PRIOR_H

#include "movegen.h"
#include "mcts.h"

void calc_exp_evals(Position& pos, ExtMove* moves, int size);
void calc_priors(Position& pos, ExtMove* moves, int size);
Move sampleMove(Position& pos, ExtMove* moves);
Move sampleMove(Position& pos, std::vector<UnopenedMove>& moves);

#endif //SRC_MCTS_PRIOR_H
