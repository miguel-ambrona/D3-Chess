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

#ifndef DYNAMIC_H_INCLUDED
#define DYNAMIC_H_INCLUDED

namespace DYNAMIC {

enum SearchResult { WINNABLE, UNWINNABLE, UNDETERMINED };

enum SearchMode { FULL, QUICK };
enum SearchTarget { ANY, SHORTEST };

enum SearchFlag { PRE_STATIC, STATIC, POST_STATIC };

constexpr int MAX_VARIATION_LENGTH = 2000;

// Search class stores information relative to the helpmate search

class Search {
 public:
  Search() = default;

  void init();
  void set(Depth maxDepth, Depth initDepth, uint64_t localNodesLimit);

  void set_limit(uint64_t nodesLimit);
  void set_winner(Color intendedWinner);

  Color intended_winner() const;
  Depth actual_depth() const;
  Depth max_depth() const;

  void annotate_move(Move m);
  void increase_cnt();
  void step();
  void undo_step();
  void set_winnable();
  void set_unwinnable();
  void set_flag(SearchFlag searchFlag);
  void interrupt();

  bool is_interrupted() const;
  bool is_local_limit_reached() const;
  bool is_limit_reached() const;

  SearchResult get_result() const;
  SearchFlag get_flag() const;
  uint64_t get_limit() const;
  uint64_t get_nb_nodes() const;

  void print_result() const;

 private:
  // Data members
  Move checkmateSequence[MAX_VARIATION_LENGTH];
  Color winner;

  Depth depth;
  Depth maxSearchDepth;
  Depth mateLen;
  SearchResult result;
  SearchFlag flag;
  bool interrupted;
  uint64_t counter;
  uint64_t totalCounter;
  uint64_t localLimit;
  uint64_t globalLimit;
};

inline void Search::init() {
  totalCounter = 0;
  counter = 0;
  flag = PRE_STATIC;
}

inline void Search::set(Depth maxDepth, Depth initDepth,
                        uint64_t localNodesLimit) {
  depth = initDepth;
  maxSearchDepth = maxDepth;
  mateLen = 0;
  result = UNDETERMINED;
  interrupted = false;
  localLimit = localNodesLimit;
  totalCounter += counter;
  counter = 0;
}

inline void Search::set_limit(uint64_t nodesLimit) { globalLimit = nodesLimit; }

inline void Search::set_winner(Color intendedWinner) {
  winner = intendedWinner;
}

inline Color Search::intended_winner() const { return winner; }

inline Depth Search::actual_depth() const { return depth; }

inline Depth Search::max_depth() const { return maxSearchDepth; }

inline void Search::annotate_move(Move m) {
  if (depth < MAX_VARIATION_LENGTH) checkmateSequence[depth] = m;
}

inline void Search::increase_cnt() { counter++; }

inline void Search::step() { depth++; }

inline void Search::undo_step() { depth--; }

inline void Search::set_winnable() {
  result = WINNABLE;
  mateLen = depth;
}

inline void Search::set_unwinnable() { result = UNWINNABLE; }

inline void Search::set_flag(SearchFlag searchFlag) { flag = searchFlag; }

inline void Search::interrupt() { interrupted = true; }

inline bool Search::is_interrupted() const { return interrupted; }

inline bool Search::is_local_limit_reached() const {
  return counter > maxSearchDepth * localLimit;
}

inline bool Search::is_limit_reached() const {
  return totalCounter > globalLimit;
}

inline SearchResult Search::get_result() const { return result; }

inline uint64_t Search::get_limit() const { return globalLimit; }

inline uint64_t Search::get_nb_nodes() const { return totalCounter + counter; }

inline SearchFlag Search::get_flag() const { return flag; }

SearchResult full_analysis(Position&, Search&);

SearchResult quick_analysis(Position&, Search&);

SearchResult find_shortest(Position&, Search&);

}  // namespace DYNAMIC

#endif  // #ifndef DYNAMIC_H_INCLUDED
