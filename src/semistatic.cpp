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

void SemiStatic::System::init() {

  int i, j;
  Square presquares[8];

  for (PieceType p = PAWN; p <= KING; ++p)
    for (Color c : { WHITE, BLACK })
      for (Square s = SQ_A1; s <= SQ_H8; ++s)
        for (Square t = SQ_A1; t <= SQ_H8; ++t) {
          i = index(p,c,s,t);
          UTIL::unmove(presquares,p,c,t);

          for (j = 0; j < 8; ++j) {
            if (presquares[j] < 0)
              equations[i][j] = -1;
            else
              equations[i][j] = index(p,c,s,presquares[j]);
          }
        }
}

void SemiStatic::System::saturate(Position& pos) {

  // Initialize the variables

  for (int j = 0; j < N_MOVE_VARS; ++j)
    variables[j] = false;

  for (int j = 0; j < 4*128; ++j)
    variables[N_MOVE_VARS + j] = false;

  Square occupied[64];
  int n = 0;

  for (Square s = SQ_A1; s <= SQ_H8; ++s) {
    Piece pc = pos.piece_on(s);
    PieceType p = type_of(pc);
    if (p == NO_PIECE_TYPE) {
      variables[clear_index(WHITE,s)] = true;
      variables[clear_index(BLACK,s)] = true;
      continue;
    }

    Color c = color_of(pc);
    variables[clear_index(~c,s)] = true;

    variables[index(p,c,s,s)] = true;
    occupied[n] = s;
    n++;
  }

  // Saturate the system

  bool change = true;
  while (change) {
    change = false;

    for (int k = 0; k < n; ++k) {
      Square source = occupied[k];
      Piece pc = pos.piece_on(source);
      PieceType p = type_of(pc);
      Color c = color_of(pc);

      // Update Clear variables: a piece can be cleared from a squared if it can
      // move or it can be captured in that square)

      for (Square aux = SQ_A1; aux <= SQ_H8; ++aux) {
        PieceType aux_piece = type_of(pos.piece_on(aux));
        if (source == aux)
          continue;

        if (variables[index(p,c,source,aux)] ||
            (aux_piece != NO_PIECE_TYPE &&
             variables[index(aux_piece,~c,aux,source)]))
          if (!variables[clear_index(c,source)]) {
            change = true;
            variables[clear_index(c,source)] = true;
            break;
          }
      }

      // Update Reach and Capture variable:
      // Reach(c,s) is true if a non-king c-colored piece can reach square s
      // Capture(c,s) is true if some c-colored piece could capture on s

      for (Square target = SQ_A1; target <= SQ_H8; ++target)
        if (variables[index(p,c,source,target)]) {
          if (p != KING && !variables[reach_index(c,target)]) {
            change = true;
            variables[reach_index(c,target)] = true;
          }

          // We update pawn captures later
          if (p != PAWN && !variables[capture_index(c,target)]) {
            change = true;
            variables[capture_index(c,target)] = true;
          }
        }

      // Update the Movement variables

      for (Square target = SQ_A1; target <= SQ_H8; ++target) {

        // If the target square contains a piece of color c and cannot
        // be cleared yet, continue
        if (!variables[clear_index(c,target)])
          continue;

        // Check for KING attacks (if it moves to target)
        if (p == KING) {
          bool target_attacked = false;
          Bitboard attackers = pos.attackers_to(target) & pos.pieces(~c);

          for (int sq = 0; sq < 64; ++sq)
            if ((attackers & (1ULL << sq)) && !variables[clear_index(~c,static_cast<Square>(sq))]) {
              target_attacked = true;
              break;
            }
          if (target_attacked)
            continue;
        }

        int i = index(p,c,source,target);

        for (int j = 0; j < 8; ++j) {

          int var = equations[i][j];
          if (var < 0 || variables[i])
            break;

          // Update the Movement variable

          if (variables[var]){
            if (p == PAWN) {

              // Pawn push cannot be performed if there is an obstacle in target
              if (j == 0) {
                if (!variables[clear_index(~c,target)])
                  continue;

                // or if there is a pawn in target which could not leave its
                // file and the source pawn could also not leave its file

                Piece tpiece = pos.piece_on(target);

                if (type_of(tpiece) == PAWN && color_of(tpiece) != c
                    && file_of(source) == file_of(target)) {
                  bool confronting = true;
                  for (Square aux = SQ_A1; aux <= SQ_H8; ++aux) {

                    if (file_of(source) != file_of(aux)) {
                      if (variables[index(p,c,source,aux)] ||
                          variables[index(PAWN,~c,target,aux)]) {
                        confronting = false;
                        break;
                      }
                    }
                    else if ((rank_of(source) < rank_of(aux) &&
                                rank_of(aux) <= rank_of(target))
                          || (rank_of(source) > rank_of(aux) &&
                                rank_of(aux) >= rank_of(target))) {

                      if (variables[capture_index(c,aux)]) {
                        confronting = false;
                        break;
                      }
                    }

                  }

                  if (confronting)
                    continue;
                }

              } // end push

              // Pawn capture cannot be performed
              if (j > 0 && !variables[reach_index(~c,target)])
                continue;

              if (j > 0)
                variables[capture_index(c,target)] = true;
            }

            change = true;
            variables[i] = true;

            break;
          }
        }
      }

      // If the pawn can promote, it may go everywhere

      if (p == PAWN) {
        Square prom_rank = (c == WHITE) ? SQ_A8 : SQ_A1;
        for (int file = 0; file < 8; ++file)
          if (variables[index(p,c,source,static_cast<Square>(prom_rank+file))]) {
            for (int j = 0; j < 64; ++j) {
              int i = index(p,c,source,SQ_A1) + j;
              change = variables[i] ? change : true;
              variables[i] = true;
            }
            break;
          }
      }
    }

  } // end while
}

Bitboard SemiStatic::System::king_region(Position& pos, Color c) {

  Bitboard region = 0;
  Square s = UTIL::find_king(pos, c);
  for (Square t = SQ_A1; t <= SQ_H8; ++t)
    if (variables[index(KING,c,s,t)])
      region |= t;

  return region;
}

// Returns the position of the pieces of color c that can visit the region

Bitboard SemiStatic::System::visitors(Position& pos, Bitboard region, Color c) {

  Bitboard visitors = 0;
  for (Square s = SQ_A1; s <= SQ_H8; ++s) {
    Piece pc = pos.piece_on(s);
    PieceType p = type_of(pc);

    if (p == NO_PIECE_TYPE)
      continue;

    // We ignore pawn visitors that are limited in movement (is this sound?)
    if (p == PAWN && !variables[index(p,c,s,SQ_A1)])
      continue;

    Color color = color_of(pc);

    for (Square t = SQ_A1; t <= SQ_H8; ++t)
      if (color == c && (region & t) && variables[index(p,c,s,t)])
          visitors |= s;
  }

  return visitors;
}

bool SemiStatic::System::is_unwinnable(Position& pos, Color intendedWinner) {

  if (UTIL::has_lonely_pawns(pos))
      return false;

  Bitboard loserKingRegion = king_region(pos, ~intendedWinner);
  Bitboard visitors =
    SemiStatic::System::visitors(pos, loserKingRegion, intendedWinner)
    & ~pos.pieces(intendedWinner, KING);

  // std::cout << Bitboards::pretty(loserKingRegion);
  // std::cout << Bitboards::pretty(visitors);
  // std::cout << Bitboards::pretty(non_king_visitors);

  // If there are no visitors, the position is unwinnable
  if (!visitors)
    return true;

  // If there exist visitors of both square colors, declare the position
  // as potentially winnable
  if ((visitors & DarkSquares) && (visitors & ~DarkSquares))
    return false;

  // All visitors are all of the same square color; if they are not all bishops,
  // declare the position as potentially winnable
  for (Square s = SQ_A1; s <= SQ_H8; ++s)
    if ((visitors & s) && type_of(pos.piece_on(s)) != BISHOP)
      return false;

  Bitboard visitorsSquareColor =
    (visitors & DarkSquares) ? DarkSquares : ~DarkSquares;

  // For every candidate checkmating square s:
  for (Square s = SQ_A1; s <= SQ_H8; ++s) {
    // Check that at least a visitor can go to s and s is in the mating region
    Bitboard matingBishops =
      SemiStatic::System::visitors(pos, square_bb(s), intendedWinner)
      & ~pos.pieces(intendedWinner, KING);

    if (!matingBishops || !(loserKingRegion & s))
      continue;

    Bitboard escapingSquares = 0;
    Bitboard checkingSquares = 0;
    for (Square t = SQ_A1; t <= SQ_H8; ++t)
      if (distance<Square>(s,t) == 1 && (loserKingRegion & t)) {
        if (~visitorsSquareColor & t)
            escapingSquares |= t;
        else
          checkingSquares |= t;
      }

    // Check if Winner's king can collaborate on the checkmate
    bool activeWinnersKing =
      pos.pieces(intendedWinner, KING) &
      SemiStatic::System::visitors(pos, UTIL::neighbours(s), intendedWinner);

    // If there are two mating diagonals pointing to s, Winner must have at
    // least two bishops in the region (or their king); otherwise Loser's king
    // will have at least an escaping square
    bool twoDiagonals =
      checkingSquares & ((checkingSquares >> 2) | (checkingSquares >> 16));

    if (twoDiagonals && popcount(matingBishops) < 2 && !activeWinnersKing)
      continue;

    // Check if some escaping square cannot be reached by the blockers
    bool unblockable = false;
    for (Square e = SQ_A1; e <= SQ_H8; ++e)
      if (escapingSquares & (1ULL << e))
        if (!(~pos.pieces(KING) &
              SemiStatic::System::visitors(pos, square_bb(e),
                                           ~intendedWinner))) {
          unblockable = true;
          break;
        }

    // The position is unwinnable if loser has not enough blockers for
    // the escaping squares
    if (unblockable & !activeWinnersKing)
      continue;

    Bitboard blockers =
      SemiStatic::System::visitors(pos, escapingSquares, ~intendedWinner);

    Bitboard actualBlockers =
      blockers & ~pos.pieces(KING);

    // If there are as many blockers as escaping squares the position
    // may be winnable
    int blockersCnt = activeWinnersKing ? 1 : 0;
    for (Square blocker = SQ_A1; blocker <= SQ_H8; ++blocker)
      if ((actualBlockers & blocker) &&
          (SemiStatic::System::visitors(pos, escapingSquares, ~intendedWinner)))
        blockersCnt++;

    if (popcount(escapingSquares) <= blockersCnt)
      return false;
  }

  // If we made it so far it is because the Winner's single-colored bishops
  // cannot mate since all squares in the Loser's king region admit at least an
  // opposite color escaping square

  return true;
}

// Our global SemiStatic System variable.

static SemiStatic::System SYSTEM = SemiStatic::System();

// SemiStatic::init fills the equations relative to Movement variables
// (must be executed only once).

void SemiStatic::init() {
  SYSTEM.init();
}

// Check if the position is semistatically unwinnable.

bool SemiStatic::is_unwinnable(Position& pos, Color intendedWinner) {

  // Checkmate or Stalemate
  if (MoveList<LEGAL>(pos).size() == 0)
    return !pos.checkers() || pos.side_to_move() == intendedWinner;

  // If en passant is possible, return false
  for (const auto& m : MoveList<LEGAL>(pos))
    if (type_of(m) == ENPASSANT)
      return false;

  SYSTEM.saturate(pos);
  return SYSTEM.is_unwinnable(pos, intendedWinner);
}

// Check if the position is unwinnable in all positions at depth 1 ply

bool SemiStatic::is_unwinnable_after_one_move(Position& pos,
                                              Color intendedWinner) {

  // Checkmate or Stalemate
  if (MoveList<LEGAL>(pos).size() == 0)
    return !pos.checkers() || pos.side_to_move() == intendedWinner;

  StateInfo st;
  for (const auto& m : MoveList<LEGAL>(pos)) {
    pos.do_move(m,st);
    if (!is_unwinnable(pos, intendedWinner))
      return false;
    pos.undo_move(m);
  }
  return true;
}
