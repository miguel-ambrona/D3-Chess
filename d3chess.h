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

#ifndef D3CHESS_H_INCLUDED
#define D3CHESS_H_INCLUDED

class Position;

namespace D3Chess {

  constexpr int MAX_VARIATION_LENGTH = 2000;

  // Search class stores information relative to the helpmate search

  class Search {
  public:
    Search() = default;

    void init();
    void set(Color intendedWinner, Depth maxDepth, bool allowTricks);

    Color intended_winner() const;
    Depth actual_depth() const;
    Depth max_depth() const;

    void annotate_move(Move m);
    void interrupt();
    void step();
    void undo_step();

    bool tricks_allowed() const;
    bool is_interrupted() const;
    int get_counter() const;
    int get_total_counter() const;

    void print_result(int mate, bool showMate) const;

  private:
    // Data members
    Move checkmateSequence[MAX_VARIATION_LENGTH];
    Color winner;
    Depth depth;
    Depth maxSearchDepth;
    bool interrupted;
    bool tricks;
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

  inline bool Search::is_interrupted() const {
    return interrupted;
  }

  inline int Search::get_counter() const {
    return counter;
  }

  inline int Search::get_total_counter() const {
    return totalCounter;
  }

void loop(int argc, char* argv[]);

} // namespace D3Chess

#endif // #ifndef D3CHESS_H_INCLUDED
