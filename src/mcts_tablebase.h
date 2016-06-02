#ifndef SRC_MCTS_TABLEBASE_H
#define SRC_MCTS_TABLEBASE_H

#include "position.h"
#include "mcts_chess_playing.h"

void initTableBase();
bool isInTableBase(Position& position, PlayingResult* playingResult);

#endif //SRC_MCTS_TABLEBASE_H
