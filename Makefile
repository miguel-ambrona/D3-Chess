#  Dead Draw Detector, an implementation of a decision procedure for checking
#  whether certain player can deliver checkmate (i.e. win) in a given chess position.
#  (A dead draw position can be defined as one in which neither player can win.)
#
#  This software leverages Stockfish as a backend for chess-related functions.
#  Stockfish is free software provided under the GNU General Public License
#  (see <http://www.gnu.org/licenses/>) and so is this tool.
#  The full source code of Stockfish can be found here:
#  <https://github.com/official-stockfish/Stockfish>.
#
#  Dead Draw Detector is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
#  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU GPL for more details.

cha:
	cp main.cpp Stockfish/src/
	cp util.* Stockfish/src/
	cp semistatic.* Stockfish/src/
	cp dynamic.* Stockfish/src/
	cd Stockfish/src && make build -j ARCH=x86-64
	cp Stockfish/src/stockfish ./cha

get-stockfish:
	git clone https://github.com/official-stockfish/Stockfish.git
	sed -ie '/^OBJS/i SRCS += dynamic.cpp semistatic.cpp util.cpp' Stockfish/src/Makefile

.PHONY: cha clean

clean:
	rm ./cha
