#ifndef SRC_MCTS_CHESS_PLAYING_H
#define SRC_MCTS_CHESS_PLAYING_H

#include "position.h"
#include "movegen.h"

enum PlayingResult {
    ContinueGame = 2,
    Win = 1,
    Lose = -1,
    Tie = 0
};

PlayingResult getGameResult(Position& pos, int numMoves);

void fillVectorWithMoves(Position& pos, std::vector<ExtMove> moves);

Bitboard promotedPieces(Position& pos);

#endif //SRC_MCTS_CHESS_PLAYING_H
