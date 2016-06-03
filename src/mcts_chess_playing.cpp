#include <vector>
#include "mcts_chess_playing.h"
#include "mcts_tablebase.h"


Bitboard promotedPieces(Position& pos) {
    return pos.pieces() & ~pos.pieces(PAWN) & ~pos.pieces(KING);
}

PlayingResult getGameResult(Position& pos, int numMoves) {
    PlayingResult res;
    if (isInTableBase(pos, &res))
        return res;

    Bitboard promoted = promotedPieces(pos);
    Color sideToMove = pos.side_to_move();
    Bitboard ourPromoted = pos.pieces(sideToMove) & promoted;

    if (ourPromoted)
        return Win;
    if (promoted /*&& !ourPromoted*/) {
        return Lose;
    }

    if (numMoves == 0) {
        if (pos.checkers())
            return Lose;
        else
            return Tie;
    }

    if (pos.is_draw())
        return Tie;
    return ContinueGame;
}

// MoveBuffer is a 128 cells of ExtMoves that should be empty.
void fillVectorWithMoves(Position& pos, std::vector<ExtMove>& moveVector, ExtMove* moveBuffer) {
    ExtMove* startingPointer = moveBuffer;
    ExtMove* endingPointer = generate<LEGAL>(pos, moveBuffer);
    while (startingPointer != endingPointer) {
        moveVector.push_back(*startingPointer);
        startingPointer++;
    }
}

int getNumMoves(Position& pos, ExtMove* buffer) {
    ExtMove* end = generate<LEGAL>(pos, buffer);
    return int(end - buffer);
}