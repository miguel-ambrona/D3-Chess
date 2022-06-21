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

// We expect input commands to be a line of text containing a FEN followed by
// the intended winner ('white' or 'black') or nothing (the default intended
// winner is the last player who moved)

Color parse_line(Position& pos, StateInfo* si, std::string& line) {
  std::string fen, token;
  std::istringstream iss(line);

  while (iss >> token && token != "black" && token != "white")
    fen += token + " ";

  pos.set(fen, false, si, Threads.main());

  if (token == "white")
    return WHITE;

  else if (token == "black")
    return BLACK;

  else
    return ~pos.side_to_move();
}

// loop() waits for a command from stdin or tests file and analyzes it.

void loop(int argc, char* argv[]) {
  CHA::init();

  Position pos;
  std::string token, line;
  StateListPtr states(new std::deque<StateInfo>(1));
  bool runningTests = false;
  bool skipWinnable = false;
  bool findShortest = false;
  bool quickAnalysis = false;
  bool adjudicateTimeout = false;
  uint64_t globalLimit = 500000;

  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "test") {
      runningTests = true;
      quickAnalysis = true;
    }

    if (std::string(argv[i]) == "-u") skipWinnable = true;

    if (std::string(argv[i]) == "-min") findShortest = true;

    if (std::string(argv[i]) == "-quick") quickAnalysis = true;

    if (std::string(argv[i]) == "-timeout") adjudicateTimeout = true;

    if (std::string(argv[i]) == "-limit") {
      std::istringstream iss(argv[i + 1]);
      iss >> globalLimit;
    }
  }

  static DYNAMIC::Search search = DYNAMIC::Search();
  search.set_limit(globalLimit);

  std::ifstream infile("../tests/lichess-30K-games.txt");

  uint64_t totalPuzzles = 0;
  uint64_t totalTime = 0;
  uint64_t maxTime = 0;
  uint64_t totalTimeSquared = 0;

  while (runningTests ? getline(infile, line) : getline(std::cin, line)) {
    if (line == "quit") break;

    DYNAMIC::SearchResult result;
    Color winner = parse_line(pos, &states->back(), line);
    search.set_winner(winner);

    auto start = std::chrono::high_resolution_clock::now();

    if (findShortest)
      result = DYNAMIC::find_shortest(pos, search);

    else if (quickAnalysis)
      result = DYNAMIC::quick_analysis(pos, search);

    else
      result = DYNAMIC::full_analysis(pos, search);

    auto stop = std::chrono::high_resolution_clock::now();
    auto diff =
        std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start);
    uint64_t duration = diff.count();

    if (adjudicateTimeout) {
      if (result == DYNAMIC::UNWINNABLE)
        std::cout << "1/2-1/2" << std::endl;

      else if (winner == WHITE)
        std::cout << "1-0" << std::endl;

      else
        std::cout << "0-1" << std::endl;
    } else {
      // On quick mode, we only print [unwinnable] ([undetermined] are all
      // guessed to be [winnable]).
      // On full mode, we print all cases except possibly [winnable].
      if ((!quickAnalysis || result == DYNAMIC::UNWINNABLE) &&
          (!skipWinnable || result != DYNAMIC::WINNABLE)) {
        search.print_result();
        std::cout << " time " << duration / 1000 << " (" << line << ")" << std::endl;
      }

      // if (duration > 100 * 1000 * 1000)
      // std::cout << "Hard: " << line << std::endl;
    }

    totalPuzzles++;
    totalTime += duration;
    totalTimeSquared += duration * duration;
    if (duration > maxTime) maxTime = duration;
  }

  uint64_t avg = totalTime / totalPuzzles;
  uint64_t variance = (totalTimeSquared / totalPuzzles) - (avg * avg);

  std::cout << "Analyzed " << totalPuzzles << " "
            << "positions in " << totalTime / 1000 / 1000 << " ms "
            << "(avg: " << avg / 1000.0 << " us; "
            << "std: " << sqrt(variance) / 1000 << " us; "
            << "max: " << maxTime / 1000 << " us)" << std::endl;

  Threads.stop = true;
}

int main(int argc, char* argv[]) {
  init_stockfish();
  std::cout << "Chess Unwinnability Analyzer (CHA) version 2.5.2" << std::endl;

  CommandLine::init(argc, argv);
  loop(argc, argv);

  Threads.set(0);
  return 0;
}
