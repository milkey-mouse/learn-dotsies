# Generate XOR filters from wordfreq data

`generate.c` in this directory can convert from wordfreq's ad-hoc msgpack-based
frequency data to a folder of "cumulative" XOR filters which include all words
up to a certain probability. For example, filter #100 might contain the "first"
5% of the English language representable with the fewest words (the single word
"the" in this case), and filter #200 will include "the" *and* a couple thousand
less common words. (This way we only need to keep one filter loaded at a time.)

Learn Dotsies comes with a prebuilt set of filters from the en_large dataset.
Running this program yourself might be useful to generate filters for another
language or dataset (perhaps Learn Dotsies could be useful for real languages
as well?).

# Running

This script has a few system dependencies which can likely be installed through
your system package manager:

- [msgpack-c](https://github.com/msgpack/msgpack-c)
- [zlib](https://zlib.net/)

A Git submodule is used for a "vendored" copy of
[xor_singleheader](https://github.com/FastFilter/xor_singleheader). To ensure
this submodule is available, clone this repository with `git clone --recursive`
or run `git submodule update --init --recursive` after cloning.

To automatically build and run the `generate` program on an English dataset,
run `make` in this directory. To build `generate`, run `make generate`. If you
(somehow) don't have Make installed, `generate` can still be compiled manually:

    gcc -std=c99 -Wall -O3 -lm -lmsgpackc -lz generate.c murmur3.c -o generate
