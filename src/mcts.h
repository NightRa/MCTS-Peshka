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
    UnopenedMove* unopened_moves;
    int numMoves;
    double sumUnopenedPriorExps;

    UnopenedMoves() : unopened_moves(nullptr), numMoves(0), sumUnopenedPriorExps(0) {}

    inline void initialize(Position& pos, ExtMove* buffer) {
        ExtMove* end = generate<LEGAL>(pos, buffer);
        int numMoves = countValidMoves(buffer, int(end - buffer));
        // Calculate e^(x/t - max) for all elements in the buffer.
        calc_exp_evals(pos, buffer, numMoves);
        // And write the ExtMoves with the correct Priors to the moves vector.
        this->numMoves = numMoves;
        unopened_moves = new UnopenedMove[numMoves];

        double expSum = 0.0;
        for (int i = 0; i < numMoves; i++) {
            expSum += buffer[i].getPrior();
        }

        sumUnopenedPriorExps = expSum;

        for (int i = 0; i < numMoves; i++) {
            ExtMove& x = *buffer;
            float expPrior = x.getPrior(); // e^x
            UnopenedMove move(x.move, expPrior, float(expPrior / expSum));
            unopened_moves[i] = move;
            buffer++;
        }
    }

    void remove(UnopenedMove move) {
        UnopenedMove* it = std::find(unopened_moves, unopened_moves + numMoves, move);
        while (it < unopened_moves + numMoves - 1) {
            *it = *(it + 1);
            it++;
        }
        numMoves--;

        sumUnopenedPriorExps -= move.expPrior;
        for (int i = 0; i < size(); i++) {
            UnopenedMove& move_to_update = unopened_moves[i];
            move_to_update.relativePrior = float(move_to_update.expPrior / sumUnopenedPriorExps);
        }
    }

    inline bool empty() {
        return numMoves == 0;
    }

    inline int size() {
        return numMoves;
    }

};

struct MCTS_Edge;

class MCTS_Node {
public:
    // States: Uninitialized - Edges empty & unopened moves empty
    //         Not fully opened - Edges not empty & unopened moves not empty
    //         Fully opened - Edges not empty & unopened moves empty
    bool initialized;
    std::vector<MCTS_Edge*> edges;
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

    bool isLeaf() {
        return !initialized || !unopened_moves.empty();
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

    ~MCTS_Node();

private:
    void transfer(const MCTS_Node& node) {
        initialized = node.initialized;
        edges = node.edges;
        unopened_moves = node.unopened_moves;
        maxVisits = node.maxVisits;
        totalVisits = node.totalVisits;
        incoming_edge = node.incoming_edge;
    }
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

    int score() {
        return (int) ((overallEval * 30) * int(PawnValueEg));
    }

    void update_stats(int rolloutResult, double evalResult, float evalWeight) {
        rolloutsSum += rolloutResult;
        numRollouts += 1;
        evalSum += evalResult;
        numEvals += 1;
        overallEval = compute_overall_eval(evalWeight);
    }

    MCTS_Edge(Move _move, float _prior) : node(this), move(_move), prior(_prior) {
        overallEval = _prior;
    }

    MCTS_Edge() {}

    MCTS_Edge(const MCTS_Edge& other) {
        transfer(other);
    }

    MCTS_Edge& operator=(const MCTS_Edge& other) {
        transfer(other);
        return *this;
    }

private:
    void transfer(const MCTS_Edge& other) {
        node = other.node;
        node.incoming_edge = this; // This is the important bit!
        move = other.move;
        prior = other.prior;
        evalSum = other.evalSum;
        numEvals = other.numEvals;
        rolloutsSum = other.rolloutsSum;
        numRollouts = other.numRollouts;
        overallEval = other.overallEval;
    }


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
    void do_move_mcts(Position& pos, MCTS_Node*& node, StateInfo*& currentSt, MCTS_Edge* childEdge, MCTS_Node**& moveHistory);
    void undo_move_mcts(Position& pos, MCTS_Node*& node, StateInfo*& currentSt, MCTS_Node**& moveHistory);
    PlayingResult rollout(Position& pos, StateInfo*& currentStateInfo, StateInfo* lastStateInfo, ExtMove* moveBuffer);
}

#endif //SRC_MCTS_H
