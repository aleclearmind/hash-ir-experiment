all: libhashir.so example.ll

libhashir.so: hashir.cpp hashir.h
	clang++ -ggdb -Wall -Werror $(shell llvm-config --libs) $(shell llvm-config --cxxflags) $< -shared -o $@ -fPIC -std=c++2a

example.ll: example.c
	clang -Xclang -disable-O0-optnone -S -emit-llvm $< -o - | opt -S -sroa -o $@

run: libhashir.so example.ll
	opt -load ./libhashir.so -hash-ir example.ll  -S -o /dev/null

run-instcombine: libhashir.so example.ll
	opt -load ./libhashir.so -instcombine -hash-ir example.ll  -S -o /dev/null

clean:
	rm -f libhashir.so
	rm -f example.ll
