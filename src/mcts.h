#ifndef SRC_MCTS_H
#define SRC_MCTS_H

#include <vector>
#include "types.h"
#include "position.h"
#include "movepick.h"
#include "mcts_chess_playing.h"
#include "mcts_prior.h"

typedef int NumVisits;
typedef double EvalType;    // first check whether things work and only then change it to float

struct UnopenedMoves {
    std::vector<UnopenedMove> unopened_moves;
    double sumUnopenedPriorExps;

    UnopenedMoves() : unopened_moves(0), sumUnopenedPriorExps(0) {}

    inline void initialize(Position& pos, ExtMove* buffer) {
        ExtMove* end = generate<LEGAL>(pos, buffer);
        int numMoves = int(end - buffer);
        // Calculate e^(x/t - max) for all elements in the buffer.
        calc_exp_evals(pos, buffer, numMoves);
        // And write the ExtMoves with the correct Priors to the moves vector.
        unopened_moves.resize(numMoves);

        double expSum = 0.0;
        for (int i = 0; i < numMoves; i++) {
            expSum += buffer[i].getPrior();
        }

        for (int i = 0; i < numMoves; i++) {
            ExtMove& x = *buffer;
            UnopenedMove move(x.move, x.getPrior(), float(x.getPrior() / expSum));
            unopened_moves.push_back(move);
            buffer++;
        }
    }

    void remove(UnopenedMove move) {
        const std::vector<UnopenedMove>::iterator& iterator = std::find(unopened_moves.begin(), unopened_moves.end(), move);
        auto it = iterator;
        if(it != unopened_moves.end())
            unopened_moves.erase(it);

        sumUnopenedPriorExps -= move.expPrior;
        for (int i = 0; i < size(); i++) {
            UnopenedMove& move_to_update = unopened_moves[i];
            move_to_update.relativePrior = float(move_to_update.expPrior / sumUnopenedPriorExps);
        }
    }

    inline bool empty() {
        return unopened_moves.empty();
    }

    inline int size() {
        return int(unopened_moves.size());
    }

};

struct MCTS_Edge;

class MCTS_Node {
public:
    // States: Uninitialized - Edges empty & unopened moves empty
    //         Not fully opened - Edges not empty & unopened moves not empty
    //         Fully opened - Edges not empty & unopened moves empty
    bool initialized;
    std::vector<MCTS_Edge> edges;
    UnopenedMoves unopened_moves;
    NumVisits maxVisits;
    NumVisits totalVisits;
    MCTS_Edge* incoming_edge;
public:
    MCTS_Node() : MCTS_Node(nullptr) {}

    MCTS_Node(MCTS_Edge* parent) : initialized(false), edges(0 /*Init with size 0*/), maxVisits(0), totalVisits(0),
                                   incoming_edge(parent) {}

    inline bool fully_opened() {
        return initialized && unopened_moves.empty();
    }

    bool inMate() {
        return initialized && unopened_moves.empty() && edges.empty();
    }

    void update_child_stats(MCTS_Edge* childEdge);

    int getNumMoves(Position& pos, ExtMove* buffer) {
        // Notice that always when we use getNumMoves, we immediately after initialize.
        initialize(pos, buffer);
        return int(edges.size() + unopened_moves.size());
    }

    void initialize(Position& pos, ExtMove* buffer) {
        if (!initialized) {
            initialized = true;
            unopened_moves.initialize(pos, buffer);
        }
    }

    MCTS_Edge* selectBest();

    MCTS_Edge* open_child(Position& pos, ExtMove* moveBuffer);
};

struct MCTS_Edge {
    MCTS_Node node;
    Move move;
    float prior;
    EvalType evalSum;
    NumVisits numEvals;
    NumVisits rolloutsSum;
    NumVisits numRollouts;
    float overallEval;
    MCTS_Node* parent;

    int score () {
        return (int) ((overallEval * 30) * int(PawnValueEg));
    }

    void update_stats(int rolloutResult, double evalResult, float evalWeight) {
        rolloutsSum += rolloutResult;
        numRollouts += 1;
        evalSum += evalResult;
        numEvals += 1;
        overallEval = compute_overall_eval(evalWeight);
    }

    MCTS_Edge(Move _move, float _prior, MCTS_Node* _parent) : node(this), move(_move), prior(_prior), parent(_parent) {
        overallEval = _prior;
    }

    MCTS_Edge() {}

private:
    float compute_overall_eval(float evalWeight) {
        return ((1 - evalWeight) * (float(rolloutsSum) / float(numRollouts)) +
                (evalWeight) * (float(evalSum) / float(numEvals)));
    }
};

namespace Search {
    const double cpuct = 1;
    const float evalWeight = 0.2;
    const int pvThreshold = 7;

    void mctsSearch(Position& pos, MCTS_Node& root);
    MCTS_Edge* select_child_UCT(MCTS_Node* node);
    void do_move_mcts(Position& pos, MCTS_Node*& node, StateInfo*& currentSt, MCTS_Edge* childEdge);
    void undo_move_mcts(Position& pos, MCTS_Node*& node, StateInfo*& currentSt);
    PlayingResult rollout(Position& pos, StateInfo*& currentStateInfo, StateInfo* lastStateInfo, ExtMove* moveBuffer);
}

#endif //SRC_MCTS_H
