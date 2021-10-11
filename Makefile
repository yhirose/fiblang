all: fib
	./fib fib.fib

fib: fib.cc peglib.h
	clang++ -std=c++17 -o fib fib.cc -Wall -Wextra

peglib.h:
	wget https://raw.githubusercontent.com/yhirose/cpp-peglib/master/peglib.h

