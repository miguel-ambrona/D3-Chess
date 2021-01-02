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

#include <cstdlib>

#include "uci.h"
#include "semistatic.h"
#include "timeman.h"


Square pawn_unpush(Color c, Square s){
  return (Square) ((c == WHITE) ? s - 8 : s + 8);
}

bool overflow(Square source, Square target){
  return abs((source % 8) - (target % 8)) > 2 || target < SQ_A1 || target > SQ_H8;
}

void pawn_uncapture(Square *presquares, Color c, Square s){

  int direction = (c == WHITE) ? -1 : +1;
  int i = 0;
  for (int inc : { 7, 9 })
  {
    Square prev = (Square) (s + direction * inc);
    if (!overflow(s, prev))
    {
      presquares[i] = prev;
      i++;
    }
  }
  presquares[i] = (Square)-1;
}

constexpr int NONE = 128;  // High enough to go outside of the board

int PAWN_INCS[8]   = { -7, -8, -9, NONE, NONE, NONE, NONE, NONE };
int KNIGHT_INCS[8] = { 17, 15, 10, 6, -6, -10, -15, -17 };
int BISHOP_INCS[8] = { 9, 7, -7, -9, NONE, NONE, NONE, NONE };
int ROOK_INCS[8]   = { 8, 1, -1, -8, NONE, NONE, NONE, NONE };
int QUEEN_INCS[8]  = { 9, 8, 7, 1, -1, -7, -8, -9 };
int KING_INCS[8]   = { 9, 8, 7, 1, -1, -7, -8, -9 };

int INCREMENTS[6] = { PAWN_INCS, KNIGHT_INCS, BISHOP_INCS, ROOK_INCS, QUEEN_INCS, KING_INCS };

void unmove(Square *presquares, PieceType p, Color c, Square s){

  int i = 0;
  int direction = (color == WHITE) ? 1 : -1;

  for (int j = 0; j < 8; ++j)
  {
    Square prev = (Square) (s + direction * INCREMENTS[p-1][j]);
    if (overflow(s, prev))
      continue;

    presquares[i] = prev;
    i++;
  }
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

          switch (p) {

          case PAWN:
            // Promotion
            equations[i][0] = prom_index(c,s);

            // Pawn push
            equations[i][1] = index(p,c,s,pawn_unpush(c,t));
            equations[i][2] = clear_index(~c,t);

            // Pawn captures
            pawn_uncapture(presquares,c,t);
            for (j = 0; j < 2; ++j)
            {
              if (presquares[j] < 0)
                break;

              equations[i][3+2*j] = index(p,c,s,presquares[j]);
              equations[i][4+2*j] = reach_index(~c,t);
            }
            // Cut the sequence
            equations[i][3+2*j] = -1;
            continue;

          case KNIGHT:
            unmove(presquares,KNIGHT_INCS,t);

          case BISHOP:
            unmove(presquares,BISHOP_INCS,t);

          case ROOK:
            unmove(presquares,ROOK_INCS,t);

          case QUEEN:
            unmove(presquares,QUEEN_INCS,t);

          case KING:
            unmove(presquares,KING_INCS,t);

          default:
            continue;
          }

          for (j = 0; j < 8; ++j)
          {
            if (presquares[j] < 0)
              break;

            equations[i][j] = index(p,c,s,presquares[j]);
          }
          
        }
}
