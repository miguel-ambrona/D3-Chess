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

#ifndef CHA_H_INCLUDED
#define CHA_H_INCLUDED

#include "stockfish.h"

namespace CHA {

// To be called once to initializate data structures used by CHA
void init();

// [is_unwinnable(pos, c)] is [true] if the [pos] is unwinnable for player [c]
bool is_unwinnable(Position&, Color);

// [is_dead(pos)] is [true] if [pos] is a dead position
bool is_dead(Position&);

}  // namespace CHA

#endif  // #ifndef CHA_H_INCLUDED
