#include <cmath>
#include <chrono>
#include <random>
#include "mcts_prior.h"
#include "evaluate.h"

// e^(x/t - max)
void calc_exp_evals(Position& pos, ExtMove* moves, int size) {
    const float normalizationFactor = 200; // Something like a pawn

    StateInfo st;
    const CheckInfo ci(pos);

    for (int i = 0; i < size; i++) {
        // calculate move values (heuristics)
        pos.do_move(moves[i].move, st, pos.gives_check(moves[i], ci));
        float eval = float(Eval::evaluate<false>(pos)) / normalizationFactor;
        moves[i].setPrior(eval);
        pos.undo_move(moves[i]);
    }

    float max = -VALUE_INFINITE;
    for (int i = 0; i < size; i++) {
        max = std::max(max, moves[i].getPrior());
    }

    for (int i = 0; i < size; i++) {
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

    long long int seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::mersenne_twister_engine generator(seed);

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

    long long int seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::mersenne_twister_engine generator(seed);

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