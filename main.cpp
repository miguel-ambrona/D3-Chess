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

#include "thread.h"
#include "uci.h"
#include "d3chess.h"
#include "semistatic.h"

int main(int argc, char* argv[]) {

  std::cout << "Dead Draw Detector version 2.0" << std::endl;

  CommandLine::init(argc, argv);
  UCI::init(Options);
  Bitboards::init();
  Position::init();
  Bitbases::init();

  Threads.set(size_t(Options["Threads"]));

  D3Chess::loop(argc, argv);

  Threads.set(0);
  return 0;
}


