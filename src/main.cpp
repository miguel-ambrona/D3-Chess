/*
  Chess Unwinnability Analyzer, an implementation of a decision procedure for checking
  whether a certain player can deliver checkmate (i.e. win) in a given chess position.

  This software leverages Stockfish as a backend for chess-related functions.
  Stockfish is free software provided under the GNU General Public License
  (see <http://www.gnu.org/licenses/>) and so is this tool.
  The full source code of Stockfish can be found here:
  <https://github.com/official-stockfish/Stockfish>.

  Chess Unwinnability Analyzer is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU GPL for more details.
*/

#include "thread.h"
#include "uci.h"
#include "util.h"
#include "semistatic.h"
#include "dynamic.h"

int main(int argc, char* argv[]) {

  std::cout << "Chess Unwinnability Analyzer (CHA) version 2.1" << std::endl;

  CommandLine::init(argc, argv);
  UCI::init(Options);
  Bitboards::init();
  Position::init();
  Bitbases::init();
  SemiStatic::init();

  Threads.set(size_t(Options["Threads"]));

  CHA::loop(argc, argv);

  Threads.set(0);
  return 0;
}


