# Learn Dotsies

**Coming soon!**

Learn Dotsies is a browser extension to help you learn [dotsies](https://dotsies.org/).
It replaces a portion of words on a webpage with dotsies versions. It starts with words that appear often in English text.
You can slowly increase the percentage of words written in dotsies until you can understand 100% dotsies text.

![Screenshot of Learn Dotsies]()

# Credits

[Dotsies](https://dotsies.org/) was invented by Craig Muth.

We don't use the original Dotsies font because its licensing is unclear. It's obviously meant to be used, but
it was never given a Creative Commons license or similar open font license. Instead we generate custom Dotsies
fonts at runtime using [opentype.js](https://github.com/opentypejs/opentype.js).

[XOR filters](https://lemire.me/blog/2019/12/19/xor-filters-faster-and-smaller-than-bloom-filters/) are used
to efficiently select common words. [xor_singleheader](https://github.com/FastFilter/xor_singleheader) is used
to build the filters and a custom JS implementation is used at runtime.

The extension uses a (slightly weird, for performance's sake) [JS implementation](./learn-dotsies/murmur3.js)
of [MurmurHash3](https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp) courtesy of
[whitequark](https://github.com/whitequark/murmurhash3-js).

English word frequency data is derived from [wordfreq](https://github.com/LuminosoInsight/wordfreq), which
in turn is derived from [Exquisite Corpus](https://github.com/LuminosoInsight/exquisite-corpus), which uses
[many different data sources](https://github.com/LuminosoInsight/wordfreq#sources-and-supported-languages).
[msgpack-c](https://github.com/msgpack/msgpack-c) is used to unpack this data when building XOR filters.

# License

This extension is licensed under the [Creative Commons CC0 license](./LICENSE)—effectively dedicating it to the
public domain, if you are in a jurisdiction which allows it. The libraries linked above are under various other
licenses; see their LICENSE files for details.
