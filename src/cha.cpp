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
#include "cha.h"
#include <sstream>
#include <fstream>
#include <math.h>


void CHA::init() {
  KnightDistance::init();
  SemiStatic::init();
};

bool CHA::is_dead(Position& pos) {
  static DYNAMIC::Search search = DYNAMIC::Search();
  search.set_limit(5000000);

  search.set_winner(WHITE);
  DYNAMIC::SearchResult result = DYNAMIC::quick_analysis(pos, search);

  if (result != DYNAMIC::UNWINNABLE) return false;

  search.set_winner(BLACK);
  return DYNAMIC::UNWINNABLE == DYNAMIC::quick_analysis(pos, search);
};
