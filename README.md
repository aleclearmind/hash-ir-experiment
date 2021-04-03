# Idea

You have `example.c`, suppose that some functions are given and some others are your "query".

```c
int lol(int a, int b) {
  return (a * 2) + (b * 4);
}

int lul(int a, int b) {
  return (a * 2) + (b * 4);
}

int lel(int a, int b) {
  return (b << 2) + (a << 1);
}
```

The idea is to hash all the `llvm::Value`, look for collisions, those colliding are matching data flows.

# Usage

To build:

```sh
make
```

To run on `example.c`:

```sh
make run
make run-instcombine
```

The first run will match `lol` and `lul`.
The second run will match `lel` too, thanks to `instcombine` normalizations.

# TODO

* Opportunitstically introduce SCEV
* Handle more stuff
* Normalize GEPs into raw pointer arithmetic
* Implement proper comparison function (the hash function might have collisions)
