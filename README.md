# D3-Chess

Dead Draw Detector (D3) is an implementation of a decision procedure for checking whether
there exists a sequence of legal moves that allows certain player to checkmate their
opponent in a given chess position.

This tool uses [Stockfish](https://github.com/official-stockfish/Stockfish) as a back end
for move generation and chess-related functions.

## Why is this useful?

According to Article 6.9 of the
[FIDE Laws of Chess](https://www.fide.com/FIDE/handbook/LawsOfChess.pdf):

> ...if a player does not complete the prescribed number of moves in the allotted time,
> the game is lost by the player. However, the game is drawn if the position is such that
> the opponent cannot checkmate the player's king by any possible series of legal moves.

Which in shorter English would read as:
*"If you run out of time but your opponent can't mate you, it's a draw!"*

This is a (relatively unknown) generalization of the folklore rule that says that:
*"When you just have the king, you cannot win anymore, not even on the clock."*

### Easy for humans?

It is usually relatively easy for a human to tell whether a position can be won or not
(although not always, see the amazing composition
by Andrew Buchanan, StrateGems 2002 that can be found
[here](https://chess.stackexchange.com/questions/22555/is-the-dead-position-problem-solvable)).
For example, consider the following positions:

<img src="https://miguel-ambrona.github.io/img/draw-bishops.png">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<img src="https://miguel-ambrona.github.io/img/bishop-vs-rook.png">

It is not hard to see that neither player can deliver checkmate in the left-hand side diagram,
since the position is blocked and the bishops are not useful to make any progress.

On the other hand, actually the right-hand (side) :blush:, one can verify that it is impossible for
White to checkmate Black, since, after every (bishop) check, either the black king can go to a light
square or the rook can block the check.

In *over-the-board* tournaments, the arbiter can check whether the position is
*unwinnable* (for the player who still has time on the clock) and call it a draw
if so is the case. But, what happens in online chess?

Well, online chess servers only perform simple static checks on the position after timeout
and **call it a victory** if the position is out of the scope of such a simple analysis,
**unfairly classifying** many games.
For example [here](https://lichess.org/87JajHeg#99) White should not have won.
Or [here](https://lichess.org/PIL4PUtT#87), Black should not have won on time
after promoting to a queen (interestingly, any underpromotion would have been better).
Had you ever seen a position where **any underpromotion** is better than queening!?

### Hard for machines

The problem of **automatically checking** that a position cannot be won by a given player
is believed to be **extremely hard**. For example, think of the above diagrams.

The second one could be easily handled by a conditional that declares the position as
*unwinnable* if the intended winner has just a bishop and the opponent has only rooks
(queens, or bishops of the opposite color).

But in the first one... How do you model notions like *"the position is blocked"* or
*"the bishops are not helpful"*.
One could think that these notions could be programmed and they would not be wrong!
I am aware of some attempts that identify such *"blocked"* positions, e.g.
[this one](https://github.com/jakobvarmose/deadposition2) by Jakob Varmose is quite
interesting and promising.
However, it does not identify **all** unwinnable positions.

Other attempts are more general. For example, the
[helpmate solver](https://github.com/ddugovic/Stockfish/blob/master/src/types.h#L159)
by Daniel Dugovic can (potentially) identify any unwinnable position,
since it performs an exhaustive search on the tree of moves.
Nevertheless, such a tool (designed to solve [helpmate problems](https://en.wikipedia.org/wiki/Helpmate))
can hardly be utilized to identify the above positions as unwinnable,
since it would be *"computationally too expensive"*,
as argued by the author himself [here](https://github.com/ornicar/lila/issues/6804).

## This project

To the best of my knowledge, **D3-Chess** is the first tool that can decide unwinnability
in **all** chess positions and is **efficient** enough to be used by chess servers with a
minimal computational overhead.

I have tested the tool on a set of 5 million games from the
[lichess game database](https://database.lichess.org/) that
were won on time by one of the players, identifying
[319 of them](https://github.com/miguel-ambrona/D3-Chess/blob/main/examples/unfair.txt)
which were unfairly classified.
One could argue that 319 out of 5M (roughly 1 every 15K) is not much, but
doesn't "0 out of 5M" sound better?

The tool required an average of 1.7ms per position (and never more than 2s,
in extremely rare positions), running on my personal laptop, with processor
Intel Core i7 of 10th generation.
I believe these numbers make it completely feasible for an online chess server
to run this test on every game that ends in a timeout.

### How come this is possible?

The algorithm implemented in this tool (function ``find_mate``
from the [d3chess.cpp file](https://github.com/miguel-ambrona/D3-Chess/blob/main/d3chess.cpp))
performs an exhaustive search over
all possible variations emerging from the given position.
However, it uses many tricks and optimizations that allow to prune the tree,
and heuristics that reward variations
which are more likely to lead to a checkmate
(they are explored in more depth).

Everything started from the idea that *transpositions can be completely ignored*.
More precisely, we can store every visited position in a
[table](https://en.wikipedia.org/wiki/Transposition_table) and every time we
encounter a position that is already in the table, we can ignore it (not keep exploring
the tree from it) since that search has already been done before.
I thought this idea was so promising that is was worth being implemented.

For example, in the left-hand side diagram from above, after moving the pieces in
all possible ways, we can assure that at most 11 * 13 * 20 * 23 * 2 = 131560
different positions can be reached. (11 squares for the light-squared bishop, 13 for
the dark-squared one, 20 squares for the white king, 23 for the black king and
a factor of 2 having the turn into account.)
Consequently, with a transposition table we can decide that the position is drawn
after approximately 130K evaluations (that may seem like a lot, but this is actually one of
the most complex examples the tool will face) which is nothing for a computer.
Our actual tool will perform more evaluations that just 130K due to the
[iterative deepening](https://en.wikipedia.org/wiki/Iterative_deepening_depth-first_search),
but it really pays off in positions that are not drawn.
(Remember that you also want to find checkmate really quickly when it exists, or the search
can become overwhelming.)

I decided to implement the above idea ([Stockfish](https://github.com/official-stockfish/Stockfish)
was a great help and I am so thankful for that),
and it seemed to work quite nicely, especially after the further optimizations and tricks.


## Installation & Usage

After cloning the repository, get Stockfish by running ```make get-stockfish```.
Then, run ```make``` to compile the tool.

You can test our tool on a small number of by running ```./d3 test```.

Otherwise, simply run ```./d3``` to start the tool.
This will start a process which waits for commands from stdin.
A command must be a valid [FEN](https://en.wikipedia.org/wiki/Forsyth%E2%80%93Edwards_Notation)
position (or you will get a *segmentation fault*, hehe)
followed by the intended winner (*'white'* or *'black'*).
If no intended winner is specified, the **default** intended winner will be
**the last player to make a move**
(i.e., the player who may still have time on the clock if the position comes from a timeout).

For example:

> Dead Draw Detector version 1.0<br>
> 2k5/8/3b4/8/3NK3/8/8/8 w - - 0 1 black<br>
> Found checkmate in 30 plies (Total positions searched: 350)

There are a couple of options you may choose when calling ./d3:

* ```--show-info``` will print additional information like a checkmating sequence, if found.

* ```-min``` will search for the minimum helpmate sequence (at the cost of disabling many
optimizations). This can be used for solving helpmate problems.

Enjoy!


## License

Since this software uses Stockfish, it is also distributed under the
**GNU General Public License version 3** (GPL v3).