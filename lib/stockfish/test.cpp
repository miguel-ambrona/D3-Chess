#include "stockfish.h"
#include <iostream>
#include <stdio.h>

int main() {

  init_stockfish();

  Position pos;
  StateListPtr states(new std::deque<StateInfo>(1));

  pos.set("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", false, &states->back(),
          Threads.main());
  std::cout << pos;
  std::cout << "Number of legal moves: " << MoveList<LEGAL>(pos).size()
            << std::endl;

  return 0;
}
