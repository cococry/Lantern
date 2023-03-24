build:
	g++ lantern.cpp -o bin/lantern -Wall -Wextra -Werror -std=c++17 -O3 -ffast-math -fmax-errors=1
run_tests: 
	./bin/lantern tests/operations.lntrn
	./bin/lantern tests/conditions.lntrn
	./bin/lantern tests/loops.lntrn
	./bin/lantern tests/memory.lntrn
