build:
	g++ lantern.cpp -o bin/lantern -std=c++17 -O3 -ffast-math
run_tests:
	./bin/lantern tests/operations.lntrn
	./bin/lantern tests/conditions.lntrn
	./bin/lantern tests/loops.lntrn

