#  Stockfish is free software provided under the GNU General Public License
#  (see <http://www.gnu.org/licenses/>) and so is this tool.
#  The full source code of Stockfish can be found here:
#  <https://github.com/official-stockfish/Stockfish>.

stockfish:
	sed -ie '/^OBJS/i SRCS += stockfish.cpp' Stockfish/src/Makefile
	cp stockfish.* Stockfish/src/
	cd Stockfish/src && make build -j ARCH=x86-64
	cd Stockfish/src && g++ -Wl,--no-as-needed -lpthread -shared -o libstockfish.so -fPIC *.o

get-stockfish:
	git clone https://github.com/official-stockfish/Stockfish.git
	cd Stockfish && git checkout 51deae8

install:
	cp Stockfish/src/libstockfish.so /usr/local/lib
	mkdir -p /usr/local/include/stockfish
	cd Stockfish/src && find . -name "*.h" -exec cp --parents \{\} /usr/local/include/stockfish/ \;

test:
	g++ test.cpp -I/usr/local/include/stockfish -lstockfish -o test

.PHONY: test
