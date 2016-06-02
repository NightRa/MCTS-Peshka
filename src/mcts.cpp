#include "mcts.h"
#include "search.h"

namespace Search {
    bool is_terminal(Position &position);

    PlayingResult getGameResult(Position &pos, MCTS_Node* node, bool isCheck);

    double mctsSearch(Position &pos, MCTS_Node &root) {
        // Updated by check_time()

        int iteration = 0;
        while (!Signals.stop) {
            iteration++;

            MCTS_Node *node = &root;

            int rolloutResult;
            double evalResult;
            bool isCheck = false; //fill with something!
            PlayingResult gameResult = getGameResult(pos, node, isCheck);

            while (gameResult == ContinueGame && !node->fully_opened()) {
                MCTS_Edge *child = select_child_UCT(node);
                node = &child->node;
                pos.do_move(child->move, st, gives_check);
                gameResult = getGameResult(pos, node, isCheck);
            }
            if (gameResult != ContinueGame) {

                rolloutResult = gameResult;
                evalResult = rolloutResult;

            } else { // at leaf / not fully opened.
                MCTS_Edge *childEdge = node->open_child(pos);


                node = &childEdge->node;
                pos.do_move(childEdge->move, st, gives_check);

                rolloutResult = rollout(pos);
                evalResult = eval(pos, pos.side_to_move());
            }

            // Back propagation
            while (node->incoming_edge != nullptr) {
                node->incoming_edge->update_stats(rolloutResult, evalResult);
                pos.undo_move(node->incoming_edge->move);
                node = node->incoming_edge->parent;
                // Update max stats in the parent
                node->update_child_stats(rolloutResult, evalResult);
                // Nega-max
                rolloutResult = -rolloutResult;
                evalResult = -evalResult;
            }
        }
    }

    PlayingResult getGameResult(Position &pos, MCTS_Node* node, bool isCheck) {
        if ((pos.pieces() &~ pos.pieces(PAWN) &~ pos.pieces(KING)) != 0)
            return Win;
        if (node->edges.size() + node->unopened_moves.size() == 0){
            if (isCheck)
                return Lose;
            else
                return Tie;
        }

        if (pos.is_draw())
            return Tie;
        return ContinueGame;
    }
}
