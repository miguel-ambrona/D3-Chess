# D3-Chess

Dead Draw Detector (D3) is an implementation of a decision procedure for checking whether
there exists a sequence of legal moves that allows certain player checkmate their
opponent in a given chess position.

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

Well, online chess server only perform simple static checks on the position after timeout
and **call it a victory** if the position is out of the scope of such a simple analysis,
**unfairly classifying** many games.
For example [here](https://lichess.org/87JajHeg#99) White should not have won.
Or [here](https://lichess.org/PIL4PUtT#87), Black should not have won on time
after promoting to a queen (interestingly, any underpromotion would have been better).


### Hard for machines

The problem of **automatically checking** that a position cannot be won by a given player
is believed to be **extremelly hard**. For example, think of the above diagrams.

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

### This project

To the best of my knowledge, **D3-Chess** is the first tool that can decide unwinnability
in **all** chess positions and is **efficient** enough to be used by chess servers with a
minimal computational overhead.

I tested the tool on a set of 5 million games from [lichess](https://lichess.org/) that
where won on time by one of the players, identifying
[319 of them](https://github.com/miguel-ambrona/D3-Chess/blob/main/examples/unfair.txt)
which were unfairly classified.
One could argue that 319 out of 5M (roughly 1 every 15K) is not much, but
doesn't "0 out of 5M" sound better?

The tool required an average of 1.7ms per position (and never more than 2s,
in extremelly rare positions), running on my personal laptop, with processor
Intel Core i7 of 10th generation.
I believe these numbers make it completely feasible for an online chess server
to run this test on every game that ends in a timeout.

## Installation

