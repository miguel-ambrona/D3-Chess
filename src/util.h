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

#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

namespace UTIL {

void unmove(Square* presquares, PieceType p, Color c, Square s);
Bitboard neighbours(Square s);
Bitboard neighbours_distance_2(Square s);
Square find_king(Position& pos, Color c);
int nb_blocked_pawns(Position& pos);
bool has_lonely_pawns(Position& pos);
bool semi_blocked_target(Position& pos, Square& target);

bool is_corner(Square s);

void trivial_progress(Position& pos, StateInfo& st, int repetitions);

}  // namespace UTIL

namespace KnightDistance {

int knight_distance(Square x, Square y);

void init();
int get(Square x, Square y);

}  // namespace KnightDistance

#endif  // #ifndef UTIL_H_INCLUDED
