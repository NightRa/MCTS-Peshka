#include <cmath>
#include <chrono>
#include <random>
#include "mcts_prior.h"
#include "evaluate.h"
#include "position.h"
#include "mcts_chess_playing.h"

Value qeval(Position& pos);

Value safeEval(Position& pos, Move move /*player*/, CheckInfo& ci /*already computed for pos*/, StateInfo& st) {
    bool isCheck = pos.gives_check(move, ci); // moving player gave check.
    if (isCheck) {
        pos.do_move(move, st, isCheck);
        Value deepEval = qeval(pos);
        pos.undo_move(move);
        return -deepEval;
    } else {
        pos.do_move(move, st, isCheck);
        Value eval = Eval::evaluate<false>(pos);
        pos.undo_move(move);
        return eval;
    }
}

Value qeval(Position& pos) {
    ExtMove evasionsBuffer[16];
    
    StateInfo st;
    CheckInfo ci(pos);

    ExtMove* evasions = evasionsBuffer;
    ExtMove* end = generate<LEGAL>(pos, evasions);

    if (evasions == end) {
        // Mate, we don't have moves, we lost.
        return -VALUE_MATE;
    }

    Value bestValue = -VALUE_INFINITE;
    while (evasions != end && *evasions != MOVE_NONE) {
        // rec on the move, max.
        Value currentValue;

        bool isCheck = pos.gives_check(*evasions, ci);
        pos.do_move(*evasions, st, isCheck);
        if (!isCheck) {
             currentValue = Eval::evaluate<false>(pos);
        } else {
            currentValue = -qeval(pos);
        }
        pos.undo_move(*evasions);

        if (currentValue > bestValue) {
            bestValue = currentValue;
        }

        evasions++;
    }

    return bestValue;
}


// e^(x/t - max)
void calc_exp_evals(Position& pos, ExtMove* moves, int size) {
    const float normalizationFactor = 200; // Something like a pawn

    StateInfo st;
    CheckInfo ci(pos);

    int count = 0;
    for (int i = 0; i < size && moves[i] != MOVE_NONE; i++, count++) {
        // calculate move values (heuristics)
        Value eval = safeEval(pos, moves[i], ci, st);
        moves[i].setPrior(float(eval) / normalizationFactor);
    }

    float max = -VALUE_INFINITE;
    for (int i = 0; i < count; i++) {
        max = std::max(max, moves[i].getPrior());
    }

    for (int i = 0; i < count; i++) {
        float eval = std::exp(moves[i].getPrior() - max);
        moves[i].setPrior(eval);
    }

}

void calc_priors(Position& pos, ExtMove* moves, int size) {
    // e^(x/t - max)
    calc_exp_evals(pos, moves, size);

    double expSum = 0;
    for (int i = 0; i < size; i++) {
        expSum += moves[i].getPrior();
    }
}

Move sampleMove(Position& pos, ExtMove* moves) {
    auto generator = pos.getGenerator();
    std::uniform_real_distribution<float> distribution(0.0, 1.0);

    float stopPoint = distribution(generator);

    float partialSum = 0;
    int i = 0;
    while (partialSum <= stopPoint) {
        partialSum += moves[i].getPrior();
        i++;
    }

    return moves[i - 1];
}

UnopenedMove sampleMove(Position& pos, std::vector<UnopenedMove>& moves) {
    auto generator = pos.getGenerator();
    std::uniform_real_distribution<float> distribution(0.0, 1.0);

    float stopPoint = distribution(generator);

    float partialSum = 0;
    int i = 0;
    while (partialSum <= stopPoint) {
        partialSum += moves[i].relativePrior;
        i++;
    }

    return moves[i - 1];
}