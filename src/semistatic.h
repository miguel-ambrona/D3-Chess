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

#ifndef SEMISTATIC_H_INCLUDED
#define SEMISTATIC_H_INCLUDED

// This file is designed to determine which pieces can move in a given chess
// position and the squares they can go to. The analysis is static in the sense
// that it is performed based solely on the current position of the pieces.
// However, it may allow to conclude that a certain piece can potentially go to
// a certain square, even if all the paths to the target are currently blocked
// in some way. Or that a piece can NEVER reach a certain square, not matter how
// the pieces are moved. Consequently, we coin this analysis "semi-static".
// This analysis is particularly useful for identifying "blocked" positions.
//
// DISCLAIMER:
//   We require that this analysis be SOUND in the sense that negative
//   statements are correct, e.g. if it is concluded that "the piece on e3
//   CANNOT go to a2", this is really the case. On the other hand, our algorithm
//   may NOT be COMPLETE: it may not identify all impossibilities. For example,
//   even if it is concluded that "the piece on e3 CAN potentially go to a2", it
//   may actually be impossible to reach a2 from e3, given the dynamic
//   characteristics of the position.

namespace SemiStatic {

  constexpr int N_MOVE_VARS = 49152;   // 2 * 6 * 64 * 64
                                       // (color * piece_type * from_sq * to_sq)
  constexpr int N_PROM_VARS    = 128;  // 2 * 64 (color * from_sq)
  constexpr int N_CLEAR_VARS   = 128;  // 2 * 64 (color * square)
  constexpr int N_REACH_VARS   = 128;  // 2 * 64 (color * square)
  constexpr int N_CAPTURE_VARS = 128;  // 2 * 64 (color * square)

  // Equations for clear and reach variables are handeled independently:

  constexpr int N_EQS = 49280;  // N_MOVE_VARS + N_PROM_VARS
  constexpr int N_VARS = 49664; // N_MOVE_VARS + 128 * 4

  class System {
  public:
    System() = default;

    void init();

    int index(PieceType p, Color c, Square source, Square target) const;
    void saturate(Position& pos);
    Bitboard king_region(Position& pos, Color c);
    Bitboard visitors(Position& pos, Bitboard region, Color c);
    bool is_unwinnable(Position& pos, Color intendedWinner);

  private:
    // Data members
    int equations[N_EQS][8]; // Each equation has at most 8 disjuncts.
    bool variables[N_VARS];
  };

  inline int System::index(PieceType p, Color c, Square source,
                           Square target) const {
    return (p - 1) * (1 << 13) + ((c << 12) | (source << 6) | (int)target);
  }

  inline int color_square_index(Color c, Square s) {
    return (c << 6) | int(s);
  }

  inline int prom_index(Color c, Square s) {
    return N_MOVE_VARS + color_square_index(c,s);
  }

  inline int clear_index(Color c, Square s) {
    return N_MOVE_VARS + N_PROM_VARS + color_square_index(c,s);
  }

  inline int reach_index(Color c, Square s) {
    return N_MOVE_VARS + N_PROM_VARS + N_CLEAR_VARS + color_square_index(c,s);
  }

  inline int capture_index(Color c, Square s) {
    return N_MOVE_VARS + N_PROM_VARS + N_CLEAR_VARS + N_REACH_VARS +
      color_square_index(c,s);
  }

  void init();

  bool is_unwinnable(Position& pos, Color intendedWinner);
  bool is_unwinnable_after_one_move(Position& pos, Color intendedWinner);

} // namespace SemiStatic

// The main idea behind this analysis is to build and solve a system of
// equations over Boolean variables of the form X(s->t) for a given source
// square 's' and a target square 't'. For example, X(e3->a2) will take value 1
// if the piece currently on e3 can potentially (after several moves) land on
// a2, and will take value 0 otherwise. The system of equations consists of a
// number of logical implications relating these variables. For instance,
// assuming there is a white knight on e3 and square a2 is currently empty, we
// will consider the implication:
//
//   X(e3->a2) => X(e3->b4) OR X(e3->c3) OR X(e3->c1) ,
//
// which represents the fact that: if the knight can go from e3 to a2, it must
// also be able to go from e3 to b4, c3 or c1 (i.e. one of the squares from
// which a2 can be reached). We say this is a "move-predecessor" implication.
//
// In order to be sound, our algorithm must include all move-predecessor
// implications. In order to make our algorithm as complete as possible, we will
// consider additional variables:
//
//  * Clear(s,c)   : square 's' can be cleared of (or does not contain) pieces
//  of color 'c'.
//  * Reach(s,c)   : square 's' can be reached by (or contains) some (non-king)
//  piece of color 'c'.
//  * Capture(s,c) : square 's' can be reached by a piece of color 'c' on a
//  capturing move.
//
// These variables are modeled by the implications:
//
//   Clear(s,c) => OR_{aux : aux != s} X(s->aux)  (If c-colored piece at 's'.)
//                 OR_{aux : ~c-colored piece at aux} X(aux->s)
//
//   Reach(s,c) => OR_{aux : non-king c-colored piece at aux} X(aux->s)
//
// This way, the first implication can be made slighly more "complete"
// (capturing the case where a2 is not empty) if written as:
//
//   X(e3->a2) => Clear(a2,white) AND { X(e3->b4) OR X(e3->c3) OR X(e3->c1) }
//   (#1) .
//
// Reach variables are useful to model pawn captures, for instance, if there is
// a white pawn on c4:
//
//   X(c4->a7) => Clear(a7,white) AND
//                { X(c4->*8) OR X(c4->a6) OR { X(c4->b6) AND Reach(a7,black) }}
//
//
// In order to understand how we implement and solve the above system (although
// our actual implementation may differ slighly from this model, for the sake of
// performance) think of the system as a graph where every Boolean varaible is
// represented by a node (there may be additional auxiliary nodes not
// corresponding to any variable). Nodes are connected (according to the system
// of implications) using two different types of arrows: MUST-arrows and
// OPT-arrows. Following the above example, implication #1 would lead to arrows:
//
//                           X(e3->b4)
//                               |
//                           OPT |
//                    MUST       v       OPT
//    Clear(a7,white) ---->  X(e3->a2)  <---- X(e3->c3)
//                               ^
//                           OPT |
//                               |
//                           X(e3->c1)
//
// (Note that there will be more arrows pointing to the above nodes, the above
// only capture #1.) Also observe how the direction of the arrows is "reversed"
// from the direction of the implication.
//
// Solving the system can be interpreted as saturating the graph in the
// following sense:
//
//   1. Highlight all nodes X(s->s) for all squares 's' that are occupied in the
//   initial position.
//      Highlight all nodes Clear(s,c) for all (s,c) if 's' does not contain a
//      piece of color 'c'.
//
//   2. If a node V is not highlighted and ALL its MUST-predecessors are
//   highlighted and
//      AT LEAST ONE of its OPT-predecessors are highlighted, highlight V.
//
//   3. Repeat (2) until no more nodes can be highlighted.
//
// If the graph has been built considering all move-predecessor implications, we
// can be certain that if X(s->t) is not highlighted in the saturated graph, it
// is impossible for the piece on square 's' to reach square 't'.

#endif  // #ifndef SEMISTATIC_H_INCLUDED
