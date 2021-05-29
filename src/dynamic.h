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

#ifndef CHA_H_INCLUDED
#define CHA_H_INCLUDED

namespace CHA {

  constexpr int MAX_VARIATION_LENGTH = 2000;

  // Search class stores information relative to the helpmate search

  class Search {
  public:
    Search() = default;

    void init();
    void set(Color intendedWinner, Depth maxDepth, bool allowTricks, bool quickAnalysis);

    Color intended_winner() const;
    Depth actual_depth() const;
    Depth max_depth() const;

    void annotate_move(Move m);
    void interrupt();
    void set_unwinnable();
    void step();
    void undo_step();

    bool tricks_allowed() const;
    bool quick_search() const;
    bool is_interrupted() const;
    bool is_unwinnable() const;
    uint64_t get_counter() const;
    uint64_t get_total_counter() const;

    void print_result(int mate) const;

  private:
    // Data members
    Move checkmateSequence[MAX_VARIATION_LENGTH];
    Color winner;
    Depth depth;
    Depth maxSearchDepth;
    bool interrupted;
    bool unwinnable;
    bool tricks;
    bool quick;
    uint64_t counter;
    uint64_t totalCounter;
  };

  inline Color Search::intended_winner() const {
    return winner;
  }

  inline Depth Search::actual_depth() const {
    return depth;
  }

  inline Depth Search::max_depth() const {
    return maxSearchDepth;
  }

  inline void Search::annotate_move(Move m){
    if (depth < MAX_VARIATION_LENGTH)
      checkmateSequence[depth] = m;
  }

  inline void Search::interrupt() {
    interrupted = true;
  }

  inline void Search::set_unwinnable() {
    unwinnable = true;
  }

  inline void Search::step() {
    counter++;
    depth++;
  }

  inline void Search::undo_step() {
    depth--;
  }

  inline bool Search::tricks_allowed() const {
    return tricks;
  }

  inline bool Search::quick_search() const {
    return quick;
  }

  inline bool Search::is_interrupted() const {
    return interrupted;
  }

  inline bool Search::is_unwinnable() const {
    return unwinnable;
  }

  inline uint64_t Search::get_counter() const {
    return counter;
  }

  inline uint64_t Search::get_total_counter() const {
    return totalCounter;
  }

  void loop(int argc, char* argv[]);

} // namespace CHA

#endif // #ifndef CHA_H_INCLUDED
