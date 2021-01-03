/*
  Dead Draw Detector, an implementation of a decision procedure for checking
  whether certain player can deliver checkmate (i.e. win) in a given chess position.
  (A dead draw position can be defined as one in which neither player can win.)

  This software leverages Stockfish as a backend for chess-related functions.
  Stockfish is free software provided under the GNU General Public License
  (see <http://www.gnu.org/licenses/>) and so is this tool.
  The full source code of Stockfish can be found here:
  <https://github.com/official-stockfish/Stockfish>.

  Dead Draw Detector is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU GPL for more details.
*/

#include "uci.h"
#include "semistatic.h"
#include "timeman.h"

constexpr int NONE = 128;  // High enough to go outside of the board

int PAWN_INCS[8]   = { -8, -7, -9, NONE, NONE, NONE, NONE, NONE };
int KNIGHT_INCS[8] = { 17, 15, 10, 6, -6, -10, -15, -17 };
int BISHOP_INCS[8] = { 9, 7, -7, -9, NONE, NONE, NONE, NONE };
int ROOK_INCS[8]   = { 8, 1, -1, -8, NONE, NONE, NONE, NONE };
int QUEEN_INCS[8]  = { 9, 8, 7, 1, -1, -7, -8, -9 };
int KING_INCS[8]   = { 9, 8, 7, 1, -1, -7, -8, -9 };

int *INCREMENTS[6] = { PAWN_INCS, KNIGHT_INCS, BISHOP_INCS, ROOK_INCS, QUEEN_INCS, KING_INCS };

bool overflow(Square source, Square target){
  return abs((source % 8) - (target % 8)) > 2 || target < SQ_A1 || target > SQ_H8;
}

void unmove(Square *presquares, PieceType p, Color c, Square s){

  int i = 0;
  int direction = (c == WHITE) ? 1 : -1;

  for (int j = 0; j < 8; ++j)
  {
    Square prev = (Square) (s + direction * INCREMENTS[p-1][j]);
    if (overflow(s, prev))
      continue;

    presquares[i] = prev;
    i++;
  }
  if (i < 8)
    presquares[i] = (Square) -1;
}

void SemiStatic::System::init() {

  int i, j;
  Square presquares[8];

  for (PieceType p = PAWN; p <= KING; ++p)
    for (Color c : { WHITE, BLACK })
      for (Square s = SQ_A1; s <= SQ_H8; ++s)
        for (Square t = SQ_A1; t <= SQ_H8; ++t)
        {
          i = index(p,c,s,t);
          unmove(presquares,p,c,t);

          for (j = 0; j < 8; ++j)
          {
            if (presquares[j] < 0)
              break;

            equations[i][j] = index(p,c,s,presquares[j]);
          }
        }
}

void SemiStatic::System::solve(Position& pos) {

  // Initialize the variables

  for (int j = 0; j < 4*128; ++j)
    variables[N_MOVE_VARS + j] = false;

  Square occupied[64];
  int n = 0;

  for (Square s = SQ_A1; s <= SQ_H8; ++s)
  {
    Piece pc = pos.piece_on(s);
    PieceType p = type_of(pc);
    if (p == NO_PIECE_TYPE)
    {
      variables[clear_index(WHITE,s)] = true;
      variables[clear_index(BLACK,s)] = true;
      continue;
    }

    Color c = color_of(pc);
    variables[clear_index(~c,s)] = true;

    for (int j = 0; j < 64; ++j)
      variables[index(p,c,s,SQ_A1) + j] = false;

    variables[index(p,c,s,s)] = true;
    occupied[n] = s;
    n++;
  }

  // Saturate the system

  bool change = true;
  while (change)
  {
    change = false;

    for (int k = 0; k < n; ++k)
    {
      Square source = occupied[k];
      Piece pc = pos.piece_on(source);
      PieceType p = type_of(pc);
      Color c = color_of(pc);

      // Update Clear variables
      // (A piece can be cleared from a squared if it can move or it can be captured)

      for (Square aux = SQ_A1; aux <= SQ_H8; ++aux)
        if ((source != aux && variables[index(p,c,source,aux)])
            || (variables[index(p,~c,aux,source)]))
        {
          change = variables[clear_index(c,source)] ? change : true;
          variables[clear_index(c,source)] = true;
        }

      // Update Reach variables
      // (Reach(c,s) is true if a non-king c-colored piece can reach square s)

      for (Square target = SQ_A1; target <= SQ_H8; ++target)
        if (p != KING && variables[index(p,c,source,target)])
        {
          change = variables[reach_index(c,target)] ? change : true;
          variables[reach_index(c,target)] = true;
        }

      // Update the Movement variables

      for (Square target = SQ_A1; target <= SQ_H8; ++target)
      {
        // If the target square cannot be cleared yet, continue
        if (!variables[clear_index(c,target)])
          continue;

        // Check for KING attacks (if it moves to target)
        if (p == KING)
        {
          bool target_attacked = false;
          Bitboard attackers = pos.attackers_to(target) & pos.pieces(~c);
          for (int sq = 0; sq < 64; ++sq)
            if ((attackers & ((Bitboard)1 << sq)) && !variables[clear_index(~c,(Square)sq)])
            {
              target_attacked = true;
              break;
            }
          if (target_attacked)
            continue;
        }

        int i = index(p,c,source,target);

        for (int j = 0; j < 8; ++j)
        {
          int var = equations[i][j];
          if (var < 0)
            break;

          // Update the Movement variable

          if (variables[var]){
            if (p == PAWN)
            {
              // Pawn push cannot be performed
              if (j == 0 && !variables[clear_index(~c,target)])
                continue;

              // Pawn capture cannot be performed
              if (j > 0 && !variables[reach_index(~c,target)])
                continue;
            }
            change = variables[i] ? change : true;
            variables[i] = true;
          }
        }
      }

      // If the pawn can promote, it may go everywhere

      if (p == PAWN)
      {
        Square prom_rank = (c == WHITE) ? SQ_A8 : SQ_A1;
        for (int file = 0; file < 8; ++file)
          if (variables[index(p,c,source,(Square)(prom_rank+file))])
            for (int j = 0; j < 64; ++j)
            {
              int i = index(p,c,source,SQ_A1) + j;
              change = variables[i] ? change : true;
              variables[i] = true;
            }
      }
    }

  } // end while
}

void SemiStatic::System::king_region(Position& pos, Color c) {
  solve(pos);
  Square s;
  for (s = SQ_A1; s <= SQ_H8; ++s)
  {
    Piece pc = pos.piece_on(s);
    if (type_of(pc) == KING && color_of(pc) == c)
      break;
  }

  for (Square t = SQ_A1; t <= SQ_H8; ++t)
    if (variables[index(KING,c,s,t)])
      std::cout << t << " ";
}
