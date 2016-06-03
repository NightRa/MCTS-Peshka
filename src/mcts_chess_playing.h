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
Bitboard promotedPieces(Position& pos);
int getNumMoves(Position& pos, ExtMove* buffer);
int countValidMoves(ExtMove* moves, int maxSize);

#endif //SRC_MCTS_CHESS_PLAYING_H
