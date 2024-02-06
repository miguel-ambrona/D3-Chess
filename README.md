# [Chess Unwinnability Analyzer (CHA)](https://chasolver.org) <img src="https://miguel-ambrona.github.io/img/cha.png" width="70px" align="right">

[![Tests](https://github.com/miguel-ambrona/D3-Chess/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/miguel-ambrona/D3-Chess/actions/workflows/c-cpp.yml)

CHA is a free and open-source implementation of a
decision procedure for *checking whether there exists a sequence of legal moves
that allows a certain player to checkmate their opponent*
in a given chess position.

(Although the unwinnability decision algorithm implemented in this tool is
completely original, we use
[Stockfish](https://github.com/official-stockfish/Stockfish) as a back end
for move generation and chess-related functions.)

## What is this tool useful for?

This tool can be used to rigorously (and automatically) applying Article 6.9 of
the [FIDE Laws of Chess](https://www.fide.com/FIDE/handbook/LawsOfChess.pdf):

> ...if a player does not complete the prescribed number of moves in the
> allotted time, the game is lost by the player. However, the game is drawn
> if the position is such that the opponent cannot checkmate the player's king
> by any possible series of legal moves.

Which in shorter English would read as:
*"If you run out of time but your opponent can't mate you, it's a draw!"*

This is a (relatively unknown) generalization of the folklore rule that says
that when you just have the king, you cannot win anymore, not even on the clock.

This tool can also be used to correctly applying Article 5.2.2 of the FIDE Laws,
which establishes that a game is finished as soon as the position becomes dead,
i.e. unwinnable for both players, and no more moves should be played.

See [this page](https://chasolver.org)
for more details about the problem of correctly applying Article 6.9 and to
know more about this tool.


## Results

We have evaluated CHA over the entire
[Lichess Open Database](https://database.lichess.org/)
of standard rated games, which includes more than 5 billion games to date (January 2024).
More concretely, we have applied CHA to the final position of all games that
ended in a timeout and that were classified as 1-0 or 0-1.

Our analysis led to identifying a total of
[136416](https://raw.githubusercontent.com/miguel-ambrona/D3-Chess/main/tests/unfair.txt)
games that were unfairly classified.
Namely, games that were lost by the player who ran out of time, but their
opponent could not have checkmated them by any possible sequence of legal moves.

## Full vs Quick version of CHA

In order to minimize the computational impact of running CHA, we propose a less
complete, but faster version of our algorithm. Our quick version may terminate
without having found a helpmate sequence in complex positions, declaring them
as "probably winnable".
Despite not being complete, our quick algorithm (since CHA v2.5.2) can correctly
identify all unfairly classified games from the Lichess Open Database.

Below, we present a comparison of the performance of the two versions of CHA
when analyzing all the non-drawn timeouts of over 30 million games form Lichess.
The experiments were performed on a personal laptop
1.80GHz Intel-Core i7-10510U CPU, running Ubuntu 20.04 LTS.

|                                 |    Full CHA   |    Quick CHA   |
|--------------------------------:|:-------------:|:--------------:|
|   Average #positions per second |      3000     |    200,000+    |
|       Average time per position |     336 μs    |     4.46 μs    |
|              Standard deviation |     333 μs    |     7.45 μs    |
|       Maximum time per position |     315 ms    |     2.17 ms    |
| Unwinnable positions identified |  2446 (100%)  |  2446 (100%)   |
|            Total execution time |   3 h 05 min  |   2 min 27 s   |


## Installation & Usage

After cloning the repository:

1. Install Stockfish. From the `lib/stockfish/` directory, run
`make get-stockfish` followed by `make` and `make install`.

2. Compile CHA. From the `src/` directory, run `make`.

You can test the tool by running `./cha test`.

Otherwise, simply run `./cha` to start a process which waits for commands
from stdin. A command must be a valid
[FEN](https://en.wikipedia.org/wiki/Forsyth%E2%80%93Edwards_Notation)
position, followed by the intended winner (**white** or **black**).
If no intended winner is specified, the *last player to make a move is the
default intended winner* (i.e., the player who may still have time on the clock
if the position comes from a timeout).

On every query, CHA will produce one line output including:

1. [**winnable** _string_ **|** **unwinnable** **|** **undetermined**], the
result of the evaluation.<br>
When "winnable", a helpmate sequence in UCI format is provided.

1. **nodes** _int_, the total number of positions evaluated.

1. **time** _int_, the total execution time (measured in microseconds).

For example:

> **./cha**<br>
> Chess Unwinnability Analyzer (CHA) version 2.6.1<br>
> **Bb2kb2/bKp1p1p1/1pP1P1P1/pP6/6P1/P7/8/8 b - -**<br>
> winnable e8d8 b7a6 d8e8 a8b7 e8d8 b7c8 d8e8 c8d7 e8d8 d7e8 d8c8 g4g5 c8d8 e8f7 d8c8 f7g8 c8d8 a6b7 d8e8 b7c8 a5a4 g8f7# nodes 14175 time 22198 (Bb2kb2/bKp1p1p1/1pP1P1P1/pP6/6P1/P7/8/8 b - -)<br>
> **7b/1k5B/7b/8/1p1p1p1p/1PpP1P1P/2P3K1/N7 b - - black**<br>
> unwinnable nodes 10101 time 7332 (7b/1k5B/7b/8/1p1p1p1p/1PpP1P1P/2P3K1/N7 b - - black)

There are a few of options you may choose when calling ./cha:

* ```-u``` will only show an output on (u)nwinnable positions, ignoring all
winnable. (Undetermined cases are also displayed if any.)

* ```-quick``` will perform a quick analysis trying to prove that the position
is unwinnable, only producing an output if so is the case.

* ```-min``` will search for the minimum helpmate sequence (at the cost of
disabling many optimizations). This can be used for solving helpmate problems.

* ```-limit```, followed by an integer, can be used to change the #nodes limit
in the search.

Other examples:

> **./cha -min -limit 1000000**<br>
> Chess Unwinnability Analyzer (CHA) version 2.6.1<br>
> **8/4K2k/4P2p/8/3b1q2/8/8/8 b - - white**<br>
> winnable f4b8 e7f7 d4h8 e6e7 b8f8 e7f8n# nodes 473539 time 126129 (8/4K2k/4P2p/8/3b1q2/8/8/8 b - - white)

Enjoy!

## Test vectors

If you are building your own tool for solving chess unwinnability, you may
want to see how it performs on our
[test vectors](https://github.com/miguel-ambrona/D3-Chess/blob/main/tests/test-vector.txt),
a list of challenging positions queried by the community to our web tool
[https://chasolver.org/analyzer](https://chasolver.org/analyzer.html).
Each position comes with the corresponding classification with respect
to unwinnability.

## Acknowledgments

We would like to thank everyone who has contributed to this project.

Special thanks:
[https://chasolver.org/acks.html](https://chasolver.org/acks.html).

## License

Since this software uses Stockfish, it is also distributed under the
[*GNU General Public License version 3* (GPL v3)](https://github.com/miguel-ambrona/D3-Chess/blob/main/LICENSE).
