#ifndef SRC_MCTS_PRIOR_H
#define SRC_MCTS_PRIOR_H

#include "movegen.h"

void calc_priors(Position& pos, ExtMove* moves, int size);
Move sampleMove(Position& pos, ExtMove* moves, int size);

#endif //SRC_MCTS_PRIOR_H
