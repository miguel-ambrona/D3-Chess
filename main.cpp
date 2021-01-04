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

  SemiStatic::System system = SemiStatic::System();
  system.init();

  //  std::cout << system.index(KING, BLACK, SQ_H8, SQ_H8) << std::endl;

  //  std::cout << SemiStatic::attack_index(BLACK, SQ_H8) << std::endl;

  Position pos;
  StateListPtr states(new std::deque<StateInfo>(1));

  std::string fen = "2k5/8/8/1p6/1Pp1p1pB/p1P1PpP1/P4P2/5K1N w - - 0 1";
  fen = "2k5/8/6bB/1p3bB1/1Pp1p1p1/2P1PpP1/2bB1P2/2B2K2 w - - 0 1";
  fen = "2k5/8/8/1p6/1Pp1p1p1/2P1PpP1/2bB1P2/2B2K2 w - - 0 1";
  fen = "b7/3k4/4bB2/1p6/1Pp1p1p1/2P1PpP1/2K2P2/8 b - - 0 10";
  fen = "8/8/8/2k3p1/p1p1p1P1/P1P1P1P1/3K4/8 w - - 11 60 5inggIqz";
  
  pos.set(fen, false, &states->back(), Threads.main());

  //  for (int i = 0; i < 10000; ++i)
  //    system.solve(pos);

  //  system.solve(pos);

  //  system.saturate(pos);

  //Bitboard region = system.king_region(pos, WHITE);
  //std::cout << region << std::endl;
  //std::cout << Bitboards::pretty(region) << std::endl;

  //std::cout << Bitboards::pretty(system.king_region(pos, BLACK)) << std::endl;

  //  for (int j = 0; j < 10000; ++j)
  //    {
  //      system.saturate(pos);
  //      bool b = system.is_unwinnable(pos, WHITE);
  //    }

  std::cout << "Is unwinnable: " << system.is_unwinnable(pos, BLACK) << std::endl;

  Threads.set(0);
  return 0;
}


