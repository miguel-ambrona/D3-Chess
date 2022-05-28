/*
  Chess Unwinnability Analyzer, an implementation of a decision procedure for
  checking whether a certain player can deliver checkmate (i.e. win) in a given
  chess position.

  This software leverages Stockfish as a backend for chess-related functions.
  Stockfish is free software provided under the GNU General Public License
  (see <http://www.gnu.org/licenses/>) and so is this tool.
  The full source code of Stockfish can be found here:
  <https://github.com/official-stockfish/Stockfish>.

  Chess Unwinnability Analyzer is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU GPL for more
  details.
*/

#include "stockfish.h"
#include "util.h"
#include "semistatic.h"
#include "dynamic.h"

namespace {

  // We will reward variations that make pieces closer to a mating position in a
  // corner. The corner will be in the relative 8-th rank of the intended Winner
  // and the corner color depends on the existing pieces in the position.
  // E.g. if the corner is decided to be dark and WHITE is supposed to win, the
  // corner will be H8. We want Loser's king to be on H8, Winner's king on H6
  // (or G6), have a Loser's piece on G8, blocking the exit and any Winner's
  // piece pointing to H8, delivering mate. The next function sets the desired
  // square for the moving piece, based on the above details.

  inline Square set_target(Position& pos, PieceType movedPiece, Color winner) {

    // We want to go to a dark corner if Winner has a dark-squared bishop
    // or Loser has a light-squared bishop (and Winner doesn't).

    bool darkCorner = (DarkSquares & pos.pieces(winner, BISHOP)) ||
                      (popcount(pos.pieces(winner, BISHOP)) == 0 &&
                       (~DarkSquares & pos.pieces(~winner, BISHOP)));

    bool king = (movedPiece == KING);
    bool isWinnersTurn = (pos.side_to_move() == winner);

    // Assume for a moment that the target corner is H8
    Square target =
      isWinnersTurn ? (king ? SQ_H6 : SQ_H8) : (king ? SQ_H8 : SQ_G8);

    // Correct the file in case we need a light corner (the corner becomes A8)
    if (!darkCorner)
      target = flip_file(target);

    // Correct the rank in case Winner was BLACK (the corner becomes A1 or H1)
    if (winner == BLACK)
      target = flip_rank(flip_file(target));

    return target;
  }

  // Decide whether a piece is getting closer to a given square (only meaninful
  // for "slow" pieces). (It will be used to check if the position is getting
  // closer to the targetted mate.)

  bool going_to_square(Move m, Square s, PieceType p, bool checkBishops) {

    if (p == KING || (checkBishops && p == BISHOP))
      return distance<Square>(to_sq(m), s) < distance<Square>(from_sq(m), s);

    else if (p == KNIGHT)
      return KnightDistance::get(to_sq(m), s) <
             KnightDistance::get(from_sq(m), s);

    else
      return false;
  }

  // Check if it is essential that Loser promotes in order for Winner to be able
  // to checkmate. This function may result in false positives that is, the
  // output can be 'true' even if there is a mating sequence that does not
  // involve promotions. (We do not care about such sequences and will reward
  // pawn pushes if the output is 'true'.)

  bool need_loser_promotion(Position& pos, Color winner) {

    Bitboard minorPieces = pos.pieces(KNIGHT, BISHOP);

    // Winner has just a knight and Loser only has pawns and/or queen(s)
    if (popcount(pos.pieces(winner)) == 2 && pos.count<KNIGHT>(winner) == 1 &&
        (popcount(pos.pieces(~winner) & (minorPieces | pos.pieces(ROOK))) == 0))
      return true;

    // Winner has just (same colored) bishops and Loser has no knights or
    // bishops of the opposite color.
    Bitboard bishopsColor =
      DarkSquares & pos.pieces(winner, BISHOP) ? DarkSquares : ~DarkSquares;
    if (popcount(pos.pieces(winner)) == pos.count<BISHOP>(winner) + 1 &&
        popcount(~bishopsColor & pos.pieces(BISHOP)) == 0 &&
        popcount(pos.pieces(~winner) & pos.pieces(KNIGHT)) == 0)
      return true;

    return false;
  }

  // Statically (without moving pieces) check whether it is impossible for
  // Winner to checkmate. This function never gives false positives! (But we
  // cannot expect it to be complete, of course.) This function calls
  // [need_loser_promotion] after having assured that Loser has no pawns.
  // Note that if Loser has no pawns, 'need_loser_promotion' will never result
  // in false positives!

  bool impossible_to_win(Position& pos, Color winner) {

    // Winner has just the king
    if (popcount(pos.pieces(winner)) == 1)
      return true;

    // A promotion by Loser is needed, but Loser has no pawns.
    return popcount(pos.pieces(~winner, PAWN)) == 0 &&
           need_loser_promotion(pos, winner);
  }

  // Type to classify variations in the search performed in 'find_mate', the
  // search will be deeper in REWARDed variations and shorter in PUNISHed ones.

  enum VariationType { NORMAL, REWARD, PUNISH };

  // [find-mate] performs an exhaustive search (with many tricks) over the tree
  // of moves, that ends as soon as a checkmate (delivered by the intended
  // winner) is found or the maximum depth is reached. The function returns the
  // ply depth at which checkmate was found or -1 if no mate was found.

  template <DYNAMIC::SearchMode MODE, DYNAMIC::SearchTarget TARGET>
  bool find_mate(Position& pos, DYNAMIC::Search& search, Depth depth,
                 bool pastProgress, bool wasSemiBlocked) {

    Color winner = search.intended_winner();
    Color loser = ~winner;

    // To store an entry from the transposition table (TT)
    TTEntry* tte = nullptr;
    bool found;
    Depth movesLeft = search.max_depth() - depth;

    // If the position is found with more depth, we can ignore this branch
    if (MODE == DYNAMIC::FULL) {
      tte = TT.probe(pos.key(), found);
      if (found && (tte->depth() >= movesLeft))
        return false;
    }

    // Insufficient material to win
    if (impossible_to_win(pos, winner))
      return false;

    // Checkmate!
    if (MoveList<LEGAL>(pos).size() == 0 && pos.checkers() &&
        pos.side_to_move() == loser) {
      search.set_winnable();
      return true;
    }

    // Search limits
    if (depth >= search.max_depth() || search.is_local_limit_reached()) {
      search.interrupt();
      return false;
    }

    // Store this position in the TT (we then analyze it at depth 'movesLeft')
    if (MODE == DYNAMIC::FULL)
      tte->save(pos.key(), VALUE_NONE, false, BOUND_NONE, movesLeft, MOVE_NONE,
                VALUE_NONE);

    // Check if Loser has to promote, because Winner has not enough material
    bool needLoserPromotion = need_loser_promotion(pos, winner);
    bool isWinnersTurn = pos.side_to_move() == winner;

    Bitboard KRQ = pos.pieces(KNIGHT) | pos.pieces(ROOK) | pos.pieces(QUEEN);
    bool onlyPawnsAndBishops = !KRQ;
    Square unblocking_target;
    bool semiBlocked = UTIL::semi_blocked_target(pos, unblocking_target);

    // Iterate over all legal moves
    for (const ExtMove& m : MoveList<LEGAL>(pos)) {
      VariationType variation = NORMAL;

      if (TARGET == DYNAMIC::ANY) {
        PieceType movedPiece = type_of(pos.moved_piece(m));
        Square target = set_target(pos, movedPiece, winner);

        if (isWinnersTurn) {
          if (pos.advanced_pawn_push(m) || pos.capture(m) ||
              going_to_square(m, target, movedPiece, false))
            variation = REWARD;
        }
        else {
          if (needLoserPromotion) {
            PieceType promoted = promotion_type(m);  // Possibly NO_PIECE_TYPE
            bool heavyProm = (promoted == QUEEN || promoted == ROOK);
            variation = (movedPiece == PAWN && !heavyProm) ? REWARD : PUNISH;
          }

          if (going_to_square(m, target, movedPiece, false))
            variation = REWARD;

          if (pos.capture(m))
            variation = PUNISH;
        }
      }

      // Heuristic for semi-blocked positions
      if (onlyPawnsAndBishops
          && UTIL::nb_blocked_pawns(pos) >= 4
          && !UTIL::has_lonely_pawns(pos)) {

        PieceType movedPiece = type_of(pos.moved_piece(m));

        if (semiBlocked || wasSemiBlocked) {

          if (pos.capture(m) && isWinnersTurn)
            variation = REWARD;

          else if (movedPiece == KING) {
            variation = NORMAL;

            if (semiBlocked && going_to_square(m, unblocking_target, movedPiece, false))
              variation = REWARD;
          }

          else
            variation = PUNISH;
        }

        // Not semi-blocked
        else {
          Square target = set_target(pos, movedPiece, winner);
          if (going_to_square(m, target, movedPiece, true) &&
              popcount(pos.pieces(loser, BISHOP)) > 1)
            variation = REWARD;
        }
      }

      // Apply the move
      StateInfo st;
      pos.do_move(m, st);

      Depth newDepth = depth + 1;

      if (TARGET == DYNAMIC::ANY) {
        // Do not reward while Loser has queen(s) if it was their turn
        if (!isWinnersTurn && popcount(pos.pieces(loser, QUEEN)) > 0)
          variation = (variation = REWARD) ? NORMAL : variation;

        // Do not reward after a certain depth
        if (search.actual_depth() > 300)
          variation = (variation = REWARD) ? NORMAL : variation;

        switch (variation) {
          case REWARD:
            newDepth--;
            break;
          case PUNISH:
            newDepth = std::min(search.max_depth(), newDepth + 2);
            break;
          default:
            if (pastProgress) // Reward if the previous player made progress
              newDepth--;
        }
      }

      // Continue the search from the new position
      search.annotate_move(m);
      search.step();

      int checkMate =
        find_mate<MODE,TARGET>(pos, search, newDepth,
                               variation == REWARD,
                               (semiBlocked || wasSemiBlocked));

      search.undo_step();
      pos.undo_move(m);

      if (checkMate)
        return true;

    } // end of iteration over legal moves

    return false;
  }

  bool dynamically_unwinnable(Position& pos, Depth depth, Color winner,
                              DYNAMIC::Search& search) {

    // Insufficient material to win
    if (impossible_to_win(pos, winner))
      return true;

    // Checkmate!
    if (MoveList<LEGAL>(pos).size() == 0 && pos.checkers()) {
      if (pos.side_to_move() == winner){
        return true;
      } else {
        search.set_winnable();
        return false;
      }
    }

    // Maximum depth reached
    if (depth <= 0)
      return false;

    // Iterate over all legal moves
    for (const ExtMove& m : MoveList<LEGAL>(pos)) {

      StateInfo st;
      pos.do_move(m, st);
      search.annotate_move(m);
      search.step();
      bool unwinnable = dynamically_unwinnable(pos, depth-1, winner, search);
      pos.undo_move(m);
      search.undo_step();

      if (!unwinnable)
        return false;

    } // end of iteration over legal moves

    return true;
  }

}

DYNAMIC::SearchResult DYNAMIC::full_analysis(Position& pos, DYNAMIC::Search& search) {

  bool mate;
  search.init();

  // Apply a quick search of depth 2 (may be deeper on rewarded variations)
  search.set(2, 5000);
  mate = find_mate<DYNAMIC::QUICK,DYNAMIC::ANY>(pos, search, 0, false, false);

  if (!search.is_interrupted() && !mate)
    search.set_unwinnable();

  if (search.get_result() == DYNAMIC::UNDETERMINED) {
    StateInfo st;
    UTIL::trivial_progress(pos, st, 100);
    if (SemiStatic::is_unwinnable(pos, search.intended_winner()))
      search.set_unwinnable();
  }

  if (search.get_result() == DYNAMIC::UNDETERMINED) {
    TT.clear();

    // Apply iterative deepening (find_mate may look deeper than maxDepth on
    // rewarded variations)
    for (int maxDepth = 2; maxDepth <= 1000; maxDepth++) {
      search.set(maxDepth, 10000); // Local limit of 10000 is empirically good
      mate = find_mate<DYNAMIC::FULL,DYNAMIC::ANY>(pos, search, 0, false, false);

      if (!search.is_interrupted() && !mate)
        search.set_unwinnable();

      if (search.get_result() != DYNAMIC::UNDETERMINED ||
          search.is_limit_reached())
        break;
    }
  }

  // Careful, the following function modifies the position, we want it for
  // extreme completeness:
  // FIXME:
  // Improve it to arbitrary depth (not just one).
  // Are there examples where more than depth 1 will be needed?
  if (search.get_result() == DYNAMIC::UNDETERMINED)
    if (SemiStatic::is_unwinnable_after_one_move(pos, search.intended_winner()))
      search.set_unwinnable();

  return search.get_result();
}

DYNAMIC::SearchResult DYNAMIC::quick_analysis(Position& pos, DYNAMIC::Search& search) {

  search.init();
  search.set(0,0);
  bool unwinnable;
  Bitboard KRQ = pos.pieces(KNIGHT) | pos.pieces(ROOK) | pos.pieces(QUEEN);
  bool onlyPawnsAndBishops = !KRQ;
  bool almostOnlyPawnsAndBishops = popcount(KRQ) <= 1;

  StateInfo st;
  UTIL::trivial_progress(pos, st, 100);
  unwinnable = dynamically_unwinnable(pos, 9, search.intended_winner(), search);

  bool blockedCandidate =
    UTIL::nb_blocked_pawns(pos) >= 1 &&
    !UTIL::has_lonely_pawns(pos);

  if (blockedCandidate && !unwinnable && onlyPawnsAndBishops)
    if (SemiStatic::is_unwinnable(pos, search.intended_winner()))
      unwinnable = true;

  if (blockedCandidate && !unwinnable &&
      (almostOnlyPawnsAndBishops && (pos.checkers() || pos.pieces(KNIGHT))))
    if (SemiStatic::is_unwinnable_after_one_move(pos, search.intended_winner()))
      unwinnable = true;

  if (unwinnable)
    search.set_unwinnable();

  return search.get_result();
}

DYNAMIC::SearchResult DYNAMIC::find_shortest(Position& pos, DYNAMIC::Search& search) {

  bool mate;
  search.init();

  if (SemiStatic::is_unwinnable(pos, search.intended_winner()))
    search.set_unwinnable();

  TT.clear();
  for (int depth = 1; depth <= 1000; depth++) {
    search.set(depth, search.get_limit());
    mate = find_mate<DYNAMIC::FULL,DYNAMIC::SHORTEST>(pos, search, 0, false, false);

    if (!search.is_interrupted() && !mate)
      search.set_unwinnable();

    if (search.get_result() != DYNAMIC::UNDETERMINED || search.is_limit_reached())
      break;
  }

  return search.get_result();
}

// DYNAMIC::print_result() prints one line of information about the search.

void DYNAMIC::Search::print_result() const {

  if (result == WINNABLE) {
    std::cout << "winnable";
    for (int i = 0; i < std::min(mateLen, MAX_VARIATION_LENGTH); i++)
      std::cout << " " << UCI::move(checkmateSequence[i], false);
    std::cout << "#";
  }

  else if (result == UNWINNABLE)
    std::cout << "unwinnable";

  else
    std::cout << "undetermined";

  std::cout << " nodes " << (totalCounter + counter);
}
