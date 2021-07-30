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

#include "timeman.h"
#include "uci.h"
#include "util.h"

constexpr int NONE = 128;  // High enough to go outside of the board

int PAWN_INCS[8]   = { -8, -7, -9, NONE, NONE, NONE, NONE, NONE };
int KNIGHT_INCS[8] = { 17, 15, 10, 6, -6, -10, -15, -17 };
int BISHOP_INCS[8] = { 9, 7, -7, -9, NONE, NONE, NONE, NONE };
int ROOK_INCS[8]   = { 8, 1, -1, -8, NONE, NONE, NONE, NONE };
int QUEEN_INCS[8]  = { 9, 8, 7, 1, -1, -7, -8, -9 };
int KING_INCS[8]   = { 9, 8, 7, 1, -1, -7, -8, -9 };

int *INCREMENTS[6] = { PAWN_INCS, KNIGHT_INCS, BISHOP_INCS,
                       ROOK_INCS, QUEEN_INCS, KING_INCS };

bool overflow(Square source, Square target) {
  return abs((source % 8) - (target % 8)) > 2 ||
         target < SQ_A1 || target > SQ_H8;
}

void UTIL::unmove(Square *presquares, PieceType p, Color c, Square s) {

  int i = 0;
  int direction = (c == WHITE) ? 1 : -1;

  for (int j = 0; j < 8; ++j) {
    Square prev = (Square) (s + direction * INCREMENTS[p-1][j]);
    if (overflow(s, prev))
      continue;

    presquares[i] = prev;
    i++;
  }
  while (i < 8)
    presquares[i++] = (Square) -1;
}

Bitboard UTIL::neighbours(Square s) {

  Bitboard sNeighbours = 0;
  Square presquares[8];
  unmove(presquares, KING, WHITE, s);  // Color does not matter
  for (int j = 0; j < 8; ++j) {
    if (presquares[j] < 0)
      break;
    sNeighbours |= presquares[j];
  }
  return sNeighbours;
}

Square UTIL::find_king(Position& pos, Color c) {

  Square s;
  Bitboard king = pos.pieces(c, KING);
  for (s = SQ_A1; s <= SQ_H8; ++s)
    if (king & s)
      break;
  return s;
}

// A pawn is said to be "lonely" if there are no opponent pawns in its file

bool UTIL::has_lonely_pawns(Position& pos) {

  Bitboard whitePawns = pos.pieces(WHITE, PAWN);
  Bitboard blackPawns = pos.pieces(BLACK, PAWN);

  int whitePawnOcc = 0;
  int blackPawnOcc = 0;

  for (Square s = SQ_A1; s <= SQ_H8; ++s) {
    if ((whitePawns & s) && s < SQ_A7)
      whitePawnOcc |= (1 << s % 8);

    if ((blackPawns & s) && s > SQ_H2)
      blackPawnOcc |= (1 << s % 8);
  }

  return whitePawnOcc != blackPawnOcc;
}

bool UTIL::is_corner(Square s) {
  return (s == SQ_A1 || s == SQ_H1 || s == SQ_A8 || s == SQ_H8);
}

// The next function computes the knight distance between two squares.
// Note that this can be calculated from just the rank distance and
// the file distance between the squares, following the tables:
//
//      0 2 4 6            1 3 5 7            1 3 5 7
//     ---------          ---------          ---------
//  0 | 0 2 2 4        1 | 2 2 4 4        0 | 3 3 3 5
//  2 |   4 2 4        3 |   2 4 4        2 | 1 3 3 5
//  4 |     4 4        5 |     4 4        4 | 3 3 3 5
//  6 |       4        7 |       6        6 | 3 3 5 5
//
// Exceptionally, distance(SQ_A8, SQ_B7) = 4 cannot be computed from the
// tables, as well as the symmetric cases in other corners.

int KnightDistance::knight_distance(Square x, Square y) {

  std::pair<int, int> idx =
    std::minmax(distance<File>(x, y), distance<Rank>(x, y));

  // Handle the exceptional cases

  if (idx.first == 1 && idx.second == 1 &&
      (UTIL::is_corner(x) || UTIL::is_corner(y)))  return 4;

  // First and second tables
  if (idx.first % 2 == idx.second % 2) {
    if (idx.first == 0 && idx.second == 0)  return 0;
    if (idx.first == 0 && idx.second == 2)  return 2;
    if (idx.first == 0 && idx.second == 4)  return 2;
    if (idx.first == 2 && idx.second == 4)  return 2;

    if (idx.first == 1 && idx.second == 1)  return 2;
    if (idx.first == 1 && idx.second == 3)  return 2;
    if (idx.first == 3 && idx.second == 3)  return 2;
    if (idx.first == 7 && idx.second == 7)  return 6;

    return 4;
  }

  // Third table
  else {
    if (idx.second == 7)                    return 5;
    if (idx.first == 1 && idx.second == 2)  return 1;
    if (idx.first == 5 && idx.second == 6)  return 5;

    return 3;
  }
}

// In practice, instead of calling the above function all the time, we can store
// the distances between any two squares in an array. Is this really faster?

static int KnightDistanceTable[4096];  // 64 * 64 = 4096

inline unsigned index(Square x, Square y) {
  return int(x) | (y << 6);
}

void KnightDistance::init() {
  for (Square x = SQ_A1; x <= SQ_H8; ++x) {
    for (Square y = SQ_A1; y <= SQ_H8; ++y)
      KnightDistanceTable[index(x,y)] = knight_distance(x,y);
  }
}

int KnightDistance::get(Square x, Square y) {
  return KnightDistanceTable[index(x, y)];
}

