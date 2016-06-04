#ifndef SRC_MCTS_PV_H
#define SRC_MCTS_PV_H

#include <vector>
#include <string>
#include "types.h"
#include "mcts.h"

struct MCTS_PV {
public:
    std::vector<Move> moves;
    int depth;
    int time;
    int score;
    int64_t nodesVisited;


    MCTS_PV(const std::vector<Move>& _moves, int _depth, int _time, int _score, int64_t _nodesVisited) :
            moves(_moves),
            depth(_depth),
            time(_time),
            score(_score),
            nodesVisited(_nodesVisited) {}
};

MCTS_PV mctsPv(MCTS_Node* node);
std::string mcts_pv_print(MCTS_Node& root);
void mcts_check_time();

#endif
