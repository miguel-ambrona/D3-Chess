# Chess Helpmate Analyzer 2.0<img src="https://miguel-ambrona.github.io/img/cha.png" width="70px" align="right">

Chess Helpmate Analyzer (CHA) is a free and open-source implementation of a decision procedure
for *checking whether there exists a sequence of legal moves that allows a certain player to checkmate their opponent*
in a given chess position.

This tool uses [Stockfish](https://github.com/official-stockfish/Stockfish) as a back end
for move generation and chess-related functions.


## What is this useful for?

This tool can be used to rigorously (and automatically) apply Article 6.9 of the
[FIDE Laws of Chess](https://www.fide.com/FIDE/handbook/LawsOfChess.pdf):

> ...if a player does not complete the prescribed number of moves in the allotted time,
> the game is lost by the player. However, the game is drawn if the position is such that
> the opponent cannot checkmate the player's king by any possible series of legal moves.

Which in shorter English would read as:
*"If you run out of time but your opponent can't mate you, it's a draw!"*

This is a (relatively unknown) generalization of the folklore rule that says that when you
just have the king, you cannot win anymore, not even on the clock.

See [this page](https://elrubiongamma.ddns.net/chess-helpmate-analyzer/about.html)
for more details about the problem of correctly applying Article 6.9 and to know more about this tool.


## This tool

CHA 2.0 can decide unwinnability in **all chess positions** and is **efficient** enough
to be used by chess servers with a minimal computational overhead.

 * CHA 2.0 is designed to be **sound**, this means that (ignoring possible implementation bugs)
 **CHA 2.0 will never declare a position as "unwinnable" if the position is indeed winnable**.

 * CHA 2.0 is also **complete** (if run without a depth limit), this means that it will always find
 a helpmate sequence if it exists. However, it may require a prohibitive amount of time to
 perform the analysis on some positions. The good news is that I do not know (yet) of any position
 (reachable from the [starting position](https://en.wikipedia.org/wiki/Rules_of_chess#Initial_setup))
 that cannot be solved by CHA 2.0 running with its default
 depth limit (which enforces termination after a few seconds on a modern processor).


## Tests

Running CHA 2.0 on the final position of a test set 5 million games from the
[Lichess game database](https://database.lichess.org/)
(that were won on time by one of the players) led to identifying
[321 of them](https://github.com/miguel-ambrona/D3-Chess/blob/main/tests/unfair.txt)
which were unfairly classified (they should have ended in a draw).
The tool required an average of 296 μs per position (and never more than 4 ms)
running on a personal laptop, with processor Intel Core i7 of 10th generation.

<img src="https://raw.githubusercontent.com/miguel-ambrona/D3-Chess/d739f88c7e134a62f3df29248651223f2496cd13/tests/results.svg">

Observe that **over 99.92% of the positions were analyzed in less than 1.5 ms**.
These numbers make it completely feasible for an online chess server to run this test on every
game that ends in a timeout. Or even after every single move during the game, to end the
game immediately if a
[dead draw](https://en.wikipedia.org/wiki/Rules_of_chess#Dead_position)
is detected (rigorously applying Article 6.9 of the FIDE Laws of Chess).


## Installation & Usage

After cloning the repository and from the src/ directory,
get Stockfish by running ```make get-stockfish```.
Then, run ```make``` to compile the tool.

You can test the tool by running ```./cha test -quick```.

Otherwise, simply run ```./cha``` to
start a process which waits for commands from stdin.
A command must be a valid [FEN](https://en.wikipedia.org/wiki/Forsyth%E2%80%93Edwards_Notation)
position (or you will get a *segmentation fault*, hehe)
followed by the intended winner (*'white'* or *'black'*).
If no intended winner is specified, the **default intended winner will be
the last player to make a move**
(i.e., the player who may still have time on the clock if the position comes from a timeout).

For example:

> Chess Helpmate Analyzer (CHA) version 2.0<br>
> 2k5/8/3b4/8/3NK3/8/8/8 w - - 0 1 black<br>
> Found checkmate in 30 plies (Total positions searched: 350)

There are a few of options you may choose when calling ./cha:

* ```--show-info``` will print additional information like a checkmating sequence, if found.

* ```-quick``` will perform a quick analysis (sound but not complete). However, I do not know (yet)
of any position from a real game that the *quick* analysis cannot handle.

* ```-min``` will search for the minimum helpmate sequence (at the cost of disabling many
optimizations). This can be used for solving helpmate problems.
(Incompatible with the ```-quick``` option.)

Enjoy!


## License

Since this software uses Stockfish, it is also distributed under the
**GNU General Public License version 3** (GPL v3).