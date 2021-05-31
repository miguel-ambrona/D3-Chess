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

enum SearchResult { WINNABLE, UNWINNABLE, INTERRUPTED };

void CHA::Search::init(){
  totalCounter = 0;
  counter = 0;
}

void CHA::Search::set(Color intendedWinner, Depth maxDepth, bool allowTricks, bool quickAnalysis){

  winner = intendedWinner;
  depth = 0;
  maxSearchDepth = maxDepth;
  interrupted = false;
  unwinnable = false;
  tricks = allowTricks;
  quick = quickAnalysis;
  totalCounter += counter;
  counter = 0;
}

uint64_t GLOBAL = 0;

void CHA::Search::print_result(int mateLen) const {

  // This function should only be called when the search has been completed

  if (mateLen >= 0){
    std::cout << "winnable";
    for (int i = 0; i < std::min(mateLen, MAX_VARIATION_LENGTH); i++)
      std::cout << " " << UCI::move(checkmateSequence[i], false);
    std::cout << "#";
  }

  else if (!interrupted || unwinnable)
    std::cout << "unwinnable";

  else
    std::cout << "interrupted";

  GLOBAL += totalCounter + counter;
  std::cout << " nodes " << (totalCounter + counter);
}

namespace {

  Score king_safety(Position& pos, Color c){

    static Pawns::Entry* pe;
    pe = Pawns::probe(pos);

    if (c == WHITE)
      return pe->king_safety<WHITE>(pos);

    else
      return pe->king_safety<BLACK>(pos);
  }

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

  int find_mate(Position& pos, Depth depth, CHA::Search& search, bool pastProgress){

    Color winner = search.intended_winner();
    Color loser = ~winner;

    // To store an entry from the transposition table (TT)
    TTEntry* tte = nullptr;
    bool found;
    Depth movesLeft = search.max_depth() - depth;

    if (!search.quick_search())
    {
      // If the position is found in TT with more depth, we can safetly ignore this branch
      tte = TT.probe(pos.key(), found);
      if (found && (tte->depth() >= movesLeft))
        return -1;
    }

    // Insufficient material to win
    if (impossible_to_win(pos, winner))
      return -1;

    // Checkmate!
    if (MoveList<LEGAL>(pos).size() == 0 && pos.checkers() && pos.side_to_move() == loser)
      return 0;

    // Search limits
    uint64_t counterLimit = search.max_depth() * (search.quick_search() ? 1000 : 2000);
    if (depth >= search.max_depth() || search.get_counter() > counterLimit)
    {
      search.interrupt();
      return -1;
    }

    // Store this position in the TT (since we will then analyze it at depth 'movesLeft')
    if (!search.quick_search())
      tte->save(pos.key(), VALUE_NONE, false, BOUND_NONE, movesLeft, MOVE_NONE, VALUE_NONE);

    // Check if Loser has to promote a piece, because Winner has not enough material
    bool needLoserPromotion = need_loser_promotion(pos, winner);

    bool isWinnersTurn = pos.side_to_move() == winner;
    Value winMaterial = pos.non_pawn_material(winner);

    // Iterate over all legal moves
    for (const ExtMove& m : MoveList<LEGAL>(pos))
    {

      PieceType movedPiece = type_of(pos.moved_piece(m));
      VariationType variation = NORMAL;

      Square target = set_target(pos, movedPiece, winner);

      if (isWinnersTurn)
      {
        if (pos.advanced_pawn_push(m))
          variation = REWARD;

        if (pos.capture(m))
          variation = REWARD;

        if (going_to_square(m, target, movedPiece))
          variation = REWARD;
      }

      else
      {
        if (needLoserPromotion)
        {
          PieceType promoted = promotion_type(m);  // It could be of NO_PIECE_TYPE
          bool heavyPromotion = promoted == QUEEN || promoted == ROOK;// || promoted == BISHOP;

          variation = (movedPiece == PAWN && !heavyPromotion) ? REWARD : PUNISH;
        }

        if (going_to_square(m, target, movedPiece))
          variation = REWARD;

        if (pos.capture(m))
          variation = PUNISH;
      }

      // Apply the move
      StateInfo st;
      pos.do_move(m, st);

      // Do not reward any variations while Loser has queen(s) if it is their turn
      if (!isWinnersTurn && popcount(pos.pieces(loser, QUEEN)) > 0)
        variation = (variation = REWARD) ? NORMAL : variation;

      Depth newDepth = depth + 1;

      if (!search.tricks_allowed())
        variation = NORMAL;

      if (variation == REWARD)
        newDepth--;

      else if (variation == PUNISH)
        newDepth = std::min(search.max_depth(), newDepth + 2);

      // If the previous player made some progress, reward this
      else if (pastProgress)
        newDepth--;

      // Continue the search from the new position
      search.annotate_move(m);
      search.step();
      int checkMate = find_mate(pos, newDepth, search, variation == REWARD);

      if (checkMate >= 0)
        return checkMate + 1;

      search.undo_step();
      pos.undo_move(m);

    } // end of iteration over legal moves

    return -1;
  }

  // Use 'skipWinnable' to not print anything (useful when running many tests).
  // Set 'allowTricks = false' when searching for the shortest mate.

  SearchResult analyze(Position& pos, Color intendedWinner, int parameters, uint64_t searchLimit) {

    int mate;
    static CHA::Search search = CHA::Search();
    search.init();

    bool skipWinnable = parameters & 1;
    bool allowTricks = parameters & 2;
    bool quickAnalysis = parameters & 4;

    // Apply a quick search of depth 2
    search.set(intendedWinner, 2, allowTricks, true);
    mate = find_mate(pos, 0, search, false);

    // If the position has not been resolved (no mate was found, but also not proven unwinnable)
    if (mate < 0 && search.is_interrupted())
    {
      if (SemiStatic::is_unwinnable(pos, intendedWinner, 0))
      {
        search.set_unwinnable();
        std::cout << " blocked";
      }

      else if (SemiStatic::is_unwinnable_after_one_move(pos, intendedWinner))
        search.set_unwinnable();
    }

    if (mate < 0 && !search.is_unwinnable() && !quickAnalysis)
    {
      TT.clear();
      // Apply iterative deepening (find_mate may look deeper than maxDepth on rewarded variations)
      for (int maxDepth = 2; maxDepth <= 1000; maxDepth++){

        search.set(intendedWinner, maxDepth, allowTricks, quickAnalysis);
        mate = find_mate(pos, 0, search, false);

        // If the search was finished, but not interrupted, it is because victory is impossible
        if (mate >= 0 || !search.is_interrupted())
          break;

        // Remove this limit if you really want to solve the problem (it may be costly sometimes)
        if (search.get_total_counter() > (quickAnalysis ? 100000 : searchLimit))
          break;
      }
    }

    if ((mate <= 0 || !skipWinnable) && (!quickAnalysis || search.is_unwinnable()))
      search.print_result(mate);

    if (mate >= 0)
      return WINNABLE;

    else if (search.is_unwinnable())
      return UNWINNABLE;

    else
      return INTERRUPTED;
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


/// CHA::loop() waits for a command from stdin or the tests file and analyzes it.

void CHA::loop(int argc, char* argv[]) {

  KnightDistance::init();
  Position pos;
  std::string token, line;
  StateListPtr states(new std::deque<StateInfo>(1));
  bool runningTests = false;
  bool skipWinnable = false;
  bool allowTricks = true;
  bool quickAnalysis = false;
  uint64_t searchLimit = 1000000;

  for (int i = 1; i < argc; ++i){
    if (std::string(argv[i]) == "test")
      runningTests = true;

    if (std::string(argv[i]) == "-u")
      skipWinnable = true;

    if (std::string(argv[i]) == "-min")
      allowTricks = false;

    if (std::string(argv[i]) == "-quick")
      quickAnalysis = true;

    if (std::string(argv[i]) == "-limit"){
      std::istringstream iss(argv[i+1]);
      iss >> searchLimit;
    }
  }

  // If searching for the minimum helpmate, disable 'quickAnalysis'
  if (!allowTricks)
    quickAnalysis = false;

  std::ifstream infile("../tests/lichess-30K-games.txt");

  int i = 0;
  while (runningTests ? getline(infile, line) : getline(std::cin, line)) {

    if (line == "quit")
      break;

    i++;
    if (i % 3000 == 0 && runningTests)
      std::cout << i << std::endl;

    Color intendedWinner = parse_line(pos, &states->back(), line);
    int parameters =  skipWinnable + (allowTricks << 1) + (quickAnalysis << 2);

    auto start = std::chrono::high_resolution_clock::now();

    SearchResult result = analyze(pos, intendedWinner, parameters, searchLimit);

    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);

    if ((!skipWinnable || result != WINNABLE) && (!quickAnalysis || result == UNWINNABLE))
      std::cout << " time " << 12 << " (" << line << ")" << std::endl;

  }
  std::cout << "TOTAL: " << GLOBAL << std::endl;

  Threads.stop = true;
}
