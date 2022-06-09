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
#include <sstream>

// Every lime must contained two characters followed by a space and a FEN
// These characters represent the expected evaluation of the position:
//   * WB : both players can potentially helpmate
//   * W- : only White can potentially helpmate
//   * -B : only Black can potentially helpmate
//   * -- : no player can potentially helpmate, dead draw

std::string parse_line(Position &pos, StateInfo *si, std::string &line) {

  std::string fen, token, expected;
  std::istringstream iss(line);

  iss >> expected;

  while (iss >> token)
    fen += token + " ";

  pos.set(fen, false, si, Threads.main());

  return expected;
}

void analyze(std::string &line, Color winner, Position &pos,
             StateListPtr &states, DYNAMIC::Search &search,
             uint64_t &totalNodes, uint64_t &maxNodes, uint64_t &totalPositions,
             uint64_t &totalSolved, uint64_t &totalPreStatic,
             uint64_t &totalStatic) {

  DYNAMIC::SearchResult result;
  std::string expected = parse_line(pos, &states->back(), line);

  search.set_winner(winner);
  result = DYNAMIC::full_analysis(pos, search);

  bool expected_winnable;
  if (winner == WHITE)
    expected_winnable = (expected[0] == 'W');
  else
    expected_winnable = (expected[1] == 'B');

  std::string winner_str = winner == WHITE ? "white" : "black";

  totalPositions++;

  if (result == DYNAMIC::UNDETERMINED) {
    search.print_result();
    std::cout << " (" << line << " " << winner_str << ")" << std::endl;
    return;
  }

  if ((result == DYNAMIC::UNWINNABLE && expected_winnable) ||
      (result == DYNAMIC::WINNABLE && !expected_winnable)) {
    std::cout << "Test failed! ";
    search.print_result();
    std::cout << " (" << line << " " << winner_str << ")" << std::endl;
  }

  uint64_t nodes = search.get_nb_nodes();
  totalNodes += nodes;
  if (search.get_flag() == DYNAMIC::PRE_STATIC)
    totalPreStatic++;

  if (search.get_flag() == DYNAMIC::STATIC)
    totalStatic++;

  if (nodes > maxNodes)
    maxNodes = nodes;
  totalSolved++;
}

// loop() waits for a test line from stdin and analyzes it.

void loop(int argc, char *argv[]) {

  KnightDistance::init();
  SemiStatic::init();

  Position pos;
  std::string token, line;
  StateListPtr states(new std::deque<StateInfo>(1));
  uint64_t globalLimit = 10000000;

  static DYNAMIC::Search search = DYNAMIC::Search();
  search.set_limit(globalLimit);

  uint64_t totalPositions = 0;
  uint64_t totalSolved = 0;
  uint64_t totalNodes = 0;
  uint64_t totalPreStatic = 0;
  uint64_t totalStatic = 0;
  uint64_t maxNodes = 0;

  while (getline(std::cin, line)) {

    if (line[0] == '#')
      continue;

    DYNAMIC::SearchResult result;
    std::string expected = parse_line(pos, &states->back(), line);

    analyze(line, WHITE, pos, states, search, totalNodes, maxNodes,
            totalPositions, totalSolved, totalPreStatic, totalStatic);

    analyze(line, BLACK, pos, states, search, totalNodes, maxNodes,
            totalPositions, totalSolved, totalPreStatic, totalStatic);
  }

  uint64_t totalPostStatic = totalSolved - (totalPreStatic + totalStatic);

  std::cout << "\nPOSITIONS COUNT:" << std::endl;
  std::cout << "     solved: " << totalSolved << "/" << totalPositions
            << std::endl;
  std::cout << "   unsolved: " << totalPositions - totalSolved << std::endl;
  std::cout << " pre-static: " << totalPreStatic << std::endl;
  std::cout << "     static: " << totalStatic << std::endl;
  std::cout << "post-static: " << totalPostStatic << std::endl;

  std::cout << "\nNODES COUNT:" << std::endl;
  std::cout << "total nodes: " << totalNodes << std::endl;
  std::cout << "nodes (avg): " << totalNodes / totalPositions << std::endl;
  std::cout << "nodes (max): " << maxNodes << std::endl;

  Threads.stop = true;
}

int main(int argc, char *argv[]) {

  init_stockfish();
  std::cout << "Chess Unwinnability Analyzer (CHA) version 2.5" << std::endl;

  CommandLine::init(argc, argv);
  loop(argc, argv);

  Threads.set(0);
  return 0;
}
