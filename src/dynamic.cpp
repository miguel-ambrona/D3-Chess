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

#include <sstream>
#include <fstream>
#include <chrono>

#include "timeman.h"
#include "tt.h"
#include "uci.h"
#include "util.h"
#include "semistatic.h"
#include "dynamic.h"

namespace {

  // We will reward variations that make pieces closer to a mating position in a corner.
  // The corner will be in the relative 8-th rank of the intended Winner and the corner color
  // depends on the existing pieces in the position.
  // E.g. if the corner is decided to be dark and WHITE is supposed to win, the corner will be H8.
  // We want Loser's king to be on H8, Winner's king on H6 (or G6), have a Loser's piece on G8,
  // blocking the exit and any Winner's piece pointing to H8, delivering mate.
  // The next function sets the desired square for the moving piece, based on the above details.

  inline Square set_target(Position& pos, PieceType movedPiece, Color winner){

    // We want to go to a dark corner if Winner has a dark-squared bishop
    // or Loser has a light-squared bishop (and Winner doesn't).

    bool darkCorner = (DarkSquares & pos.pieces(winner, BISHOP))
      || (popcount(pos.pieces(winner, BISHOP)) == 0 && (~DarkSquares & pos.pieces(~winner, BISHOP)));

    bool king = (movedPiece == KING);
    bool isWinnersTurn = (pos.side_to_move() == winner);

    // Assume for a moment that Winner is WHITE and the needed corner is dark (i.e. H8)
    Square target = isWinnersTurn ? (king ? SQ_H6 : SQ_H8) : (king ? SQ_H8 : SQ_G8);

    // Correct the file in case we need a light corner (the corner becomes A8)
    if (!darkCorner)
      target = flip_file(target);

    // Correct the rank in case Winner was BLACK (the corner will become A1 or H1)
    if (winner == BLACK)
      target = flip_rank(flip_file(target));

    return target;
  }

  // Decide whether a piece is getting closer to a given square (only meaninful for "slow" pieces).
  // (It will be used to check if the position is getting closer to the targetted mate.)

  bool going_to_square(Move m, Square s, PieceType p){

    if (p == KING)
      return distance<Square>(to_sq(m), s) < distance<Square>(from_sq(m), s);

    else if (p == KNIGHT)
      return KnightDistance::get(to_sq(m), s) < KnightDistance::get(from_sq(m), s);

    else
      return false;
  }

  // Check if it is essential that Loser promotes in order for Winner to be able to checkmate.
  // This function may result in false positives that is, the output can be 'true' even if there
  // is a mating sequence that does not involve promotions.
  // (We do not care about such sequences and will reward pawn pushes if the output is 'true'.)

  bool need_loser_promotion(Position& pos, Color winner){

    Bitboard minorPieces = pos.pieces(KNIGHT, BISHOP);// | pos.pieces(BISHOP);

    // Winner has just a knight and Loser only has pawns and/or queen(s)
    if (popcount(pos.pieces(winner)) == 2 && pos.count<KNIGHT>(winner) == 1
        && (popcount(pos.pieces(~winner) & (minorPieces | pos.pieces(ROOK))) == 0))
      return true;

    // Winner has just (same colored) bishops and Loser has no knights or bishops of the opp. color.
    Bitboard bishopsColor = DarkSquares & pos.pieces(winner, BISHOP) ? DarkSquares : ~DarkSquares;
    if (popcount(pos.pieces(winner)) == pos.count<BISHOP>(winner) + 1
        && popcount(~bishopsColor & pos.pieces(BISHOP)) == 0
        && popcount(pos.pieces(~winner) & pos.pieces(KNIGHT)) == 0)
        return true;

    return false;
  }

  // Statically (without moving pieces) check whether it is impossible for Winner to checkmate.
  // This function never gives false positives! (But we cannot expect it to be complete, of course.)
  // This function calls 'need_loser_promotion' after having assured that Loser has no pawns.
  // Note that if Loser has no pawns, 'need_loser_promotion' will never result in false positives!

  bool impossible_to_win(Position& pos, Color winner){

    // Winner has just the king
    if (popcount(pos.pieces(winner)) == 1)
      return true;

    // A promotion by Loser is needed, but Loser has no pawns.
    return popcount(pos.pieces(~winner, PAWN)) == 0 && need_loser_promotion(pos, winner);
  }

  inline bool plenty_of_material(int material){
    return material >= 7*BishopValueMg;
  }

  inline bool little_material(int material){
    return material <= 3*BishopValueMg;
  }

  inline bool critical_material(int material){
    return material <= BishopValueEg;
  }

  // Type to classify variations in the search performed in 'find_mate', the search will be deeper
  // in REWARDed variations and shorter in PUNISHed ones.

  enum VariationType { NORMAL, REWARD, PUNISH };

  // The following is the most important function of this project!
  // It performs an exhaustive search (with many tricks) over the tree of moves, that ends as soon
  // as a checkmate (delivered by the intended winner) is found or the maximum depth is reached.
  // The function returns the ply depth at which checkmate was found or -1 if no mate was found.

  template <CHA::SearchMode MODE, CHA::SearchTarget TARGET>
  bool find_mate(Position& pos, CHA::Search& search, Depth depth, bool pastProgress){

    Color winner = search.intended_winner();
    Color loser = ~winner;

    // To store an entry from the transposition table (TT)
    TTEntry* tte = nullptr;
    bool found;
    Depth movesLeft = search.max_depth() - depth;

    if (MODE == CHA::FULL)
    {
      // If the position is found in TT with more depth, we can safetly ignore this branch
      tte = TT.probe(pos.key(), found);
      if (found && (tte->depth() >= movesLeft))
        return false;
    }

    // Insufficient material to win
    if (impossible_to_win(pos, winner))
      return false;

    // Checkmate!
    if (MoveList<LEGAL>(pos).size() == 0 && pos.checkers() && pos.side_to_move() == loser){
      search.set_winnable();
      return true;
    }

    // Search limits
    if (depth >= search.max_depth() || search.is_local_limit_reached())
    {
      search.interrupt();
      return false;
    }

    // Store this position in the TT (since we will then analyze it at depth 'movesLeft')
    if (MODE == CHA::FULL)
      tte->save(pos.key(), VALUE_NONE, false, BOUND_NONE, movesLeft, MOVE_NONE, VALUE_NONE);

    // Check if Loser has to promote a piece, because Winner has not enough material
    bool needLoserPromotion = need_loser_promotion(pos, winner);
    bool isWinnersTurn = pos.side_to_move() == winner;

    // Iterate over all legal moves
    for (const ExtMove& m : MoveList<LEGAL>(pos))
    {
      VariationType variation = NORMAL;

      if (TARGET == CHA::ANY) {
        PieceType movedPiece = type_of(pos.moved_piece(m));
        Square target = set_target(pos, movedPiece, winner);

        if (isWinnersTurn) {
          if (pos.advanced_pawn_push(m) || pos.capture(m) || going_to_square(m, target, movedPiece))
            variation = REWARD;
        }
        else {

          if (needLoserPromotion) {
            PieceType promoted = promotion_type(m);  // It could be of NO_PIECE_TYPE
            bool heavyPromotion = promoted == QUEEN || promoted == ROOK;// || promoted == BISHOP;
            variation = (movedPiece == PAWN && !heavyPromotion) ? REWARD : PUNISH;
          }

          if (going_to_square(m, target, movedPiece))
            variation = REWARD;

          if (pos.capture(m))
            variation = PUNISH;
        }
      }

      // Apply the move
      StateInfo st;
      pos.do_move(m, st);

      Depth newDepth = depth + 1;

      if (TARGET == CHA::ANY) {
        // Do not reward any variations while Loser has queen(s) if it was their turn
        if (!isWinnersTurn && popcount(pos.pieces(loser, QUEEN)) > 0)
          variation = (variation = REWARD) ? NORMAL : variation;

        switch (variation) {
          case REWARD: newDepth--; break;
          case PUNISH: newDepth = std::min(search.max_depth(), newDepth + 2); break;
          default:
            if (pastProgress) // If the previous player made some progress, reward this
              newDepth--;
        }
      }

      // Continue the search from the new position
      search.annotate_move(m);
      search.step();

      int checkMate = find_mate<MODE,TARGET>(pos, search, newDepth, variation == REWARD);

      search.undo_step();
      pos.undo_move(m);

      if (checkMate)
        return true;

    } // end of iteration over legal moves

    return false;
  }

      // Careful, this function modifies the position, we only want it for extreme completeness:
      //else if (SemiStatic::is_unwinnable_after_one_move(pos, intendedWinner))
      //  search.set_unwinnable();

  CHA::SearchResult full_analyze(Position& pos, CHA::Search& search) {

    bool mate;
    search.init();

    // Apply a quick search of depth 2 (may be deeper on rewarded variations)
    search.set(2, 5000);
    mate = find_mate<CHA::QUICK,CHA::ANY>(pos, search, 0, false);

    if (!search.is_interrupted() && !mate)
      search.set_unwinnable();

    if (search.get_result() == CHA::UNDETERMINED)
      if (SemiStatic::is_unwinnable(pos, search.intended_winner(), 0))
        search.set_unwinnable();

    if (search.get_result() == CHA::UNDETERMINED)
    {
      TT.clear();

      // Apply iterative deepening (find_mate may look deeper than maxDepth on rewarded variations)
      for (int maxDepth = 2; maxDepth <= 1000; maxDepth++)
      {
        search.set(maxDepth, 10000); // Set local limit to 10000, empirically good
        mate = find_mate<CHA::FULL,CHA::ANY>(pos, search, 0, false);

        if (!search.is_interrupted() && !mate)
          search.set_unwinnable();

        if (search.get_result() != CHA::UNDETERMINED || search.is_limit_reached())
          break;
      }
    }

    return search.get_result();
  }

  CHA::SearchResult quick_analyze(Position& pos, CHA::Search& search) {

    bool mate;
    search.init();
    search.set_limit(2000);

    for (int maxDepth = 2; maxDepth <= 1000; maxDepth++)
    {
      search.set(maxDepth, 1000);
      mate = find_mate<CHA::QUICK,CHA::ANY>(pos, search, 0, false);

      if (!search.is_interrupted() && !mate)
        search.set_unwinnable();

      if (search.get_result() != CHA::UNDETERMINED || search.is_limit_reached())
        break;
    }

    if (!search.is_interrupted() && !mate)
      search.set_unwinnable();

    if (search.get_result() == CHA::UNDETERMINED)
      if (SemiStatic::is_unwinnable(pos, search.intended_winner(), 0))
        search.set_unwinnable();

    return search.get_result();
  }

  CHA::SearchResult find_shortest(Position& pos, CHA::Search& search) {

    bool mate;
    search.init();

    if (SemiStatic::is_unwinnable(pos, search.intended_winner(), 0))
      search.set_unwinnable();

    TT.clear();
    for (int depth = 2; depth <= 1000; depth++)
    {
      search.set(depth, search.get_limit());
      mate = find_mate<CHA::FULL,CHA::SHORTEST>(pos, search, 0, false);

      if (!search.is_interrupted() && !mate)
        search.set_unwinnable();

      if (search.get_result() != CHA::UNDETERMINED || search.is_limit_reached())
        break;
    }

    return search.get_result();
  }

  // We expect input commands to be a line of text containing a FEN followed by the intended winner
  // ('white' or 'black') or nothing (the default intended winner is last player who moved)

  Color parse_line(Position& pos, StateInfo* si, std::string& line){

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

} // namespace

// CHA::print_result() shows prints one line of information about the search.

void CHA::Search::print_result() const {

  if (result == WINNABLE){
    std::cout << "winnable";
    for (int i = 0; i < std::min(mateLen, MAX_VARIATION_LENGTH); i++)
      std::cout << " " << UCI::move(checkmateSequence[i], false);
    std::cout << "#";
  }

  else if (result == UNWINNABLE)
    std::cout << "unwinnable";

  else
    std::cout << "interrupted";

  std::cout << " nodes " << (totalCounter + counter);
}

/// CHA::loop() waits for a command from stdin or the tests file and analyzes it.

void CHA::loop(int argc, char* argv[]) {

  KnightDistance::init();
  Position pos;
  std::string token, line;
  StateListPtr states(new std::deque<StateInfo>(1));
  bool runningTests = false;
  bool skipWinnable = false;
  bool findShortest = false;
  bool quickAnalysis = false;
  uint64_t globalLimit = 500000;

  for (int i = 1; i < argc; ++i){
    if (std::string(argv[i]) == "test")
      runningTests = true;

    if (std::string(argv[i]) == "-u")
      skipWinnable = true;

    if (std::string(argv[i]) == "-min")
      findShortest = true;

    if (std::string(argv[i]) == "-quick")
      quickAnalysis = true;

    if (std::string(argv[i]) == "-limit"){
      std::istringstream iss(argv[i+1]);
      iss >> globalLimit;
    }
  }

  static CHA::Search search = CHA::Search();
  search.set_limit(globalLimit);

  std::ifstream infile("../tests/lichess-30K-games.txt");

  while (runningTests ? getline(infile, line) : getline(std::cin, line)) {

    if (line == "quit")
      break;

    SearchResult result;
    Color winner = parse_line(pos, &states->back(), line);
    search.set_winner(winner);

    auto start = std::chrono::high_resolution_clock::now();

    if (findShortest)
      result = find_shortest(pos, search);

    if (quickAnalysis)
      result = quick_analyze(pos, search);

    else
      result = full_analyze(pos, search);

    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);

    // On quick mode, we only print [unwinnable] ([undetermined] are all guessed to be [winnable])
    // On full mode, we print all cases except possibly [winnable].
    if ((!quickAnalysis || result == UNWINNABLE) && (!skipWinnable || result != CHA::WINNABLE))
    {
      search.print_result();
      std::cout << " time " << 12 << " (" << line << ")" << std::endl;
    }
  }

  Threads.stop = true;
}
