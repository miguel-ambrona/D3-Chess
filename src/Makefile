#  Chess Unwinnability Analyzer, an implementation of a decision procedure for
#  checking whether a certain player can deliver checkmate (i.e. win) in a given
#  chess position.
#
#  This software leverages Stockfish as a backend for chess-related functions.
#  Stockfish is free software provided under the GNU General Public License
#  (see <http://www.gnu.org/licenses/>) and so is this tool.
#  The full source code of Stockfish can be found here:
#  <https://github.com/official-stockfish/Stockfish>.
#
#  Chess Unwinnability Analyzer is distributed in the hope that it will be
#  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU GPL for more
#  details.

cha:
	g++ -o cha util.cpp semistatic.cpp dynamic.cpp cha.cpp -lpthread -O3 -I/usr/local/include/stockfish -lstockfish

test:
	g++ -o test util.cpp semistatic.cpp dynamic.cpp test.cpp -lpthread -O3 -I/usr/local/include/stockfish -lstockfish

run-test:
	echo "---------------- Test vectors ----------------" > /tmp/test.output
	cat ../tests/test-vector.txt | ./test >> /tmp/test.output
	tail -n 11 /tmp/test.output
	echo "\n---------------- Test lichess ----------------" >> /tmp/test.output
	curl -C - -o /tmp/lichess-65536.txt https://chasolver.org/lichess-65536.txt
	cat /tmp/lichess-65536.txt | ./test >> /tmp/test.output
	tail -n 11 /tmp/test.output
	diff ../tests/test.output /tmp/test.output

promote-output:
	cp /tmp/test.output ../tests/test.output

cha-lib:
	g++ -shared -o libcha.so util.cpp semistatic.cpp dynamic.cpp cha.cpp -lpthread -O3 -I/usr/local/include/stockfish -lstockfish -fPIC

install:
	cp libcha.so /usr/local/lib
	mkdir -p /usr/local/include/cha
	cp *.h /usr/local/include/cha/

.PHONY: cha test
