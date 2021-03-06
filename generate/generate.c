#include <fcntl.h>
#include <float.h>
#include <iconv.h>
#include <inttypes.h>
#include <math.h>
#include <msgpack.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#include "murmur3.h"

// TODO: use fuse filters instead? they promise slightly better bits/entry
#define XOR_MAX_ITERATIONS 1000
#include "xorfilter.h"

// note: you can probably get away with xor8 instead of xor16, because false
// positives aren't very bad in this context. in practice xor16 is fast and
// small enough (all xor16 filters combined are only 63 megabytes)
#define XOR_IMPL(NAME) xor16_##NAME

iconv_t utf8to16;

msgpack_object* map_find_key(msgpack_object_map map, char* key_to_find) {
    for (uint32_t i = 0; i < map.size; i++) {
        msgpack_object key = map.ptr[i].key;
        // only works for strings because that's all we need
        if (key.type == MSGPACK_OBJECT_STR &&
            strncmp(key_to_find, key.via.str.ptr, key.via.str.size) == 0) {
            return &map.ptr[i].val;
        }
    }
    return NULL;
}

bool verify_header(msgpack_object header) {
    msgpack_object_map header_map = header.via.map;
    if (header.type != MSGPACK_OBJECT_MAP) {
        return false;
    }

    msgpack_object* format = map_find_key(header_map, "format");
    if (format == NULL || format->type != MSGPACK_OBJECT_STR) {
        return false;
    }

    msgpack_object_str format_str = format->via.str;
    if (strncmp(format_str.ptr, "cB", format_str.size) != 0) {
        return false;
    }

    msgpack_object* version = map_find_key(header_map, "version");
    if (version == NULL || version->type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
        return false;
    }

    return version->via.u64 == 1;
}

bool serialize_xor_filter(XOR_IMPL(t) * filter, const char* name) {
    // i was originally going to gzip-compress this too, but it turns out these
    // filters have a very high entropy content, and the ratio stayed above 95%
    FILE* f = fopen(name, "wb");
    if (f == NULL) {
        return false;
    }

    uint64_t seed = filter->seed;
    uint64_t block_length = filter->blockLength;

    fwrite(&seed, sizeof(seed), 1, f);
    fwrite(&block_length, sizeof(block_length), 1, f);
    fwrite(filter->fingerprints,
           sizeof(filter->fingerprints[0]) * 3 * block_length, 1, f);
    fclose(f);

    return true;
}

// Convert a word frequency from the logarithmic centibel scale that we use
// internally, to a proportion from 0 to 1.
//
// On this scale, 0 cB represents the maximum possible frequency of 1.0.
// -100 cB represents a word that happens 1 in 10 times, -200 cB represents
// something that happens 1 in 100 times, and so on.
//
// In general, x cB represents a frequency of 10 ** (x/100).
double cB_to_freq(double centibel) { return pow(10, centibel / 100.0); }

bool read_cbpack(msgpack_object pack_obj, bool verbose) {
    // i don't know any language with 1024-char words, so this should work fine
    static char encoded[1024];

    msgpack_object_array pack = pack_obj.via.array;
    if (pack_obj.type != MSGPACK_OBJECT_ARRAY) {
        fputs("failed to unpack: top level is not an array\n", stderr);
        return false;
    }

    if (!verify_header(pack.ptr[0])) {
        fputs("failed to unpack: invalid header\n", stderr);
        return false;
    }

    size_t total_size = 0;
    for (uint32_t i = 1; i < pack.size; i++) {
        msgpack_object obj = pack.ptr[i];
        if (obj.type != MSGPACK_OBJECT_ARRAY) {
            fputs("failed to unpack: second level entry not an array", stderr);
            return false;
        }

        total_size += obj.via.array.size;
    }

    FILE* table = fopen("filters.json", "w");
    fputs("[\n", table);

    uint64_t* hashes = malloc(total_size * sizeof(uint64_t));
    uint64_t filter_size = 0;
    uint64_t collisions = 0;

    // the filters we generate are cumulative. so the first filter only
    // includes the most popular words. the next filter contains everything
    // in that one, plus some more lesser used words. this continues until
    // all words are included in the final filter.
    double cdf = 0;

    // we keep the last iteration's filter to use for duplicate detection
    XOR_IMPL(t) filter;
    // empty xor filter to use in the first loop iteration
    XOR_IMPL(allocate)(0, &filter);

    for (uint32_t i = 1; i < pack.size; i++) {
        // we already checked that this is an array when finding total_size
        msgpack_object_array sublist = pack.ptr[i].via.array;

        if (sublist.size == 0) {
            continue;
        }

        // printf("sublist %" PRIu32 "\n", i);
        // msgpack_object_print(stdout, sublist);

        for (uint32_t j = 0; j < sublist.size; j++) {
            msgpack_object word_obj = sublist.ptr[j];
            msgpack_object_str word = word_obj.via.str;
            if (word_obj.type != MSGPACK_OBJECT_STR) {
                fputs("failed to unpack: found non-string entry", stderr);
                return false;
            }

            // reset conversion state for new string
            iconv(utf8to16, NULL, 0, NULL, 0);

            char* in_buf = word.ptr;
            size_t in_size = word.size;
            char* out_buf = encoded;
            size_t out_size = 1024;
            if (iconv(utf8to16, &in_buf, &in_size, &out_buf, &out_size) == -1) {
                fprintf(stderr, "failed to convert '%.*s' from utf8 to utf16\n",
                        word.size, word.ptr);
            }

            // you may wonder why there are two separate hash values...
            // it's because javascript only really supports 32-bit ints, so we
            // have to make do with two 32-bit halves instead of 1 64-bit hash.
            // (a single 32-bit hash alone has too high chance of collisions.)
            int32_t hash1 = murmurhash3_32(encoded, 1024 - out_size, 69420);
            int32_t hash2 = murmurhash3_32(encoded, 1024 - out_size, 31337);
            uint64_t concat_hash = ((uint64_t)hash1 << 32) | (uint64_t)hash2;
            // printf("hash for %.*s: %" PRIi32 ", %" PRIi32 "\n", word.size,
            // word.ptr, hash1, hash2);

            if (XOR_IMPL(contain)(concat_hash, &filter)) {
                collisions++;
                if (verbose) {
                    fprintf(stderr,
                            "collision detected for hash %" PRIu64
                            ", ignoring '%.*s'\n",
                            concat_hash, word.size, word.ptr);
                }
            } else {
                hashes[filter_size++] = concat_hash;
            }
        }

        XOR_IMPL(free)(&filter);
        XOR_IMPL(allocate)(filter_size, &filter);
        if (!XOR_IMPL(populate)(hashes, filter_size, &filter)) {
            // i'm too lazy to fix this
            // to prevent this error i would simply not have errors in my data
            fputs("failed to build xor filter: duplicate keys detected",
                  stderr);
            XOR_IMPL(free)(&filter);
            free(hashes);
            return false;
        }

        printf("built filter for %" PRIu64 " words (%" PRIu64 " bytes)\n",
               filter_size, XOR_IMPL(size_in_bytes)(&filter));

        static char name[32];
        snprintf(name, sizeof(name), "filter_%" PRIu32 ".xorf", i);

        if (!serialize_xor_filter(&filter, name)) {
            perror("failed to write filter to file");
            XOR_IMPL(free)(&filter);
            free(hashes);
            return false;
        }

        double freq = cB_to_freq(-((double)i - 1));
        cdf += freq * sublist.size;

        // TODO: what if the last sublist is actually empty? fencepost problem
        // there will be a trailing comma which is against the JSON spec
        fprintf(table,
                "  {\"name\": \"%s\", \"freq\": %.*f, \"cdf\": %.*f}%s\n", name,
                DBL_DIG, freq, DBL_DIG, cdf, (i == pack.size - 1) ? "" : ",");
    }

    free(hashes);

    fputs("]\n", table);
    fclose(table);

    printf("built filters for %" PRIu64 " words with %" PRIu64 " collisions\n",
           filter_size, collisions);

    return true;
}

size_t unpacker_read_gz(msgpack_unpacker* unpacker, gzFile f, size_t at_least) {
    if (msgpack_unpacker_buffer_capacity(unpacker) < at_least) {
        msgpack_unpacker_reserve_buffer(unpacker, at_least);
    }

    size_t read_bytes = gzread(f, msgpack_unpacker_buffer(unpacker),
                               msgpack_unpacker_buffer_capacity(unpacker));
    msgpack_unpacker_buffer_consumed(unpacker, read_bytes);

    return read_bytes;
}

size_t get_uncompressed_size(int fd) {
    // gzip files store the uncompressed size (ISIZE) in the last 32 bits
    off_t old_pos = lseek(fd, 0, SEEK_CUR);
    if (old_pos && lseek(fd, -4, SEEK_END)) {
        char bytes[4];
        if (read(fd, &bytes, 4) == -1) {
            return 0;
        }
        lseek(fd, old_pos, SEEK_SET);
        return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
    } else {
        return 0;
    }
}

bool mkdirp(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        return mkdir(path, 0755) != -1;
    } else {
        // OK if directory exists already
        return S_ISDIR(st.st_mode);
    }
}

int main(int argc, char** argv) {
    if (argc < 3 || argc > 4) {
        fputs(
            "usage: generate <wordfreq.msgpack.gz> <output_dir> [--verbose]\n",
            stderr);
        return 1;
    }

    utf8to16 = iconv_open("utf16le", "utf8");

    int in = open(argv[1], O_RDONLY);
    if (in == -1) {
        perror("failed to open packed input");
        return 1;
    }

    if (!mkdirp(argv[2])) {
        perror("failed to create output directory");
        return 1;
    }

    if (chdir(argv[2]) == -1) {
        perror("could not change working directory to output directory");
        return 1;
    }

    bool verbose = argc == 4 && (strcmp("--verbose", argv[3]) == 0 ||
                                 strcmp("-v", argv[3]) == 0);

    size_t total_size = get_uncompressed_size(in);
    total_size = total_size ? total_size : MSGPACK_UNPACKER_INIT_BUFFER_SIZE;

    gzFile in_gz = gzdopen(in, "rb");
    if (in_gz == NULL) {
        fputs("failed to initialize zlib on input\n", stderr);
        return 1;
    }

    msgpack_unpacker unpacker;
    if (!msgpack_unpacker_init(&unpacker, total_size)) {
        fputs("failed to initialize msgpack unpacker\n", stderr);
        return 1;
    }

    while (!gzeof(in_gz)) {
        // in most cases, this is only called once, as total_size is accurate
        unpacker_read_gz(&unpacker, in_gz, 1024 * 1024);
    }

    msgpack_unpacked list;
    msgpack_unpacked_init(&list);
    if (msgpack_unpacker_next(&unpacker, &list) != MSGPACK_UNPACK_SUCCESS) {
        fputs("error unpacking first entry from msgpack\n", stderr);
        return 1;
    }

    bool ret = !read_cbpack(list.data, verbose);

    msgpack_unpacked_destroy(&list);
    msgpack_unpacker_destroy(&unpacker);
    gzclose(in_gz);
    iconv_close(utf8to16);

    return ret;
}
