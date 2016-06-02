#ifndef SRC_MCTS_H
#define SRC_MCTS_H

#include <vector>
#include "types.h"
#include "position.h"
#include "movepick.h"
#include "mcts_chess_playing.h"
#include "mcts_prior.h"

namespace Search {
    MCTS_Edge* select_child_UCT(MCTS_Node* node);
    void do_move_mcts(Position& pos, MCTS_Node*& node, StateInfo*& currentSt, MCTS_Edge* childEdge);
    void undo_move_mcts(Position& pos, MCTS_Node*& node, StateInfo*& currentSt);
    PlayingResult rollout(Position& position, StateInfo*& currentStateInfo, StateInfo* endingStateInfo);
}

typedef int NumVisits;
typedef double EvalType;    // first check whether things work and only then change it to float

class MCTS_Node;

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

    // Don't forget to initialize overallEval with the Prior

    void update_stats(int rolloutResult, double evalResult) {
        rolloutsSum += rolloutResult;
        numRollouts += 1;
        evalSum += evalResult;
        numEvals += 1;
        overallEval = compute_overall_eval();
    }

    MCTS_Edge(Move move, float prior, MCTS_Node* parent) : node(this), move(move), prior(prior), parent(parent) {
        overallEval = prior;
    }

private:
    float compute_overall_eval() {
        return (0.5f * (float(rolloutsSum) / float(numRollouts)) + 0.5f * (float(evalSum) / float(numEvals)));
    }
};

struct UnopenedMove {
    Move move;
    float expPrior;
    float relativePrior;
    float prior; // In [0, 1], devided by the sum of all e^x

    UnopenedMove(Move move, float expPrior, float prior) : move(move), expPrior(expPrior), prior(prior) {}
};

struct UnopenedMoves {
    std::vector<UnopenedMove> unopened_moves;
    double sumUnopenedPriorExps;

    UnopenedMoves() : unopened_moves(0), sumUnopenedPriorExps(0) {}

    inline void initialize(Position& pos, ExtMove* buffer) {
        ExtMove* end = generate<LEGAL>(pos, buffer);
        unsigned long numMoves = end - buffer;
        // Calculate e^(x/t - max) for all elements in the buffer.
        calc_exp_evals(pos, buffer, int(numMoves));
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
        unopened_moves.erase(std::remove(unopened_moves.begin(), unopened_moves.end(), move), unopened_moves.end());
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

    ~MCTS_Node() {
        if (edges != nullptr) {
            delete[] edges;
        }
    }

    inline bool fully_opened() {
        return initialized && unopened_moves.empty();
    }

    bool inMate() {
        return initialized && unopened_moves.empty() && edges.empty();
    }

    void update_child_stats(MCTS_Edge* childEdge) {
        totalVisits++;
        maxVisits = std::max(maxVisits, childEdge->numRollouts);
    }

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

    MCTS_Edge* open_child(Position& pos, ExtMove* moveBuffer) {
        // Precondition: not terminal, leaf => possible moves not empty
        initialize(pos, moveBuffer);
        // unopened_moves not empty.
        // sample move according to prior probabilities / take maximal probability
        UnopenedMove move = sampleMove(pos, unopened_moves.unopened_moves);
        // remove it from unopened_moves and insert to edges.
        unopened_moves.remove(move);
        edges.push_back(MCTS_Edge(move.move, move.prior, this));
        return &edges[edges.size() - 1];
    }
};

#endif //SRC_MCTS_H
