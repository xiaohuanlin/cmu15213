#include "cachelab.h"
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>

struct address_t {
    long valid;
    long tag;
    unsigned long lru_count;
};

struct address_t** cache = NULL;
unsigned long lru_count = 0;

unsigned s = 0;
unsigned S = 0;
unsigned E = 0;
unsigned b = 0;
unsigned B = 0;

unsigned verbosity = 0;
char *tracefile = NULL;

unsigned eviction_count = 0;
unsigned miss_count = 0;
unsigned hit_count = 0;

void initCache() {
    cache = malloc(S * sizeof(struct address_t*));
    if (!cache) {
        exit(1);
    }

    for (unsigned i = 0; i < S; ++i) {
        cache[i] = malloc(E * sizeof(struct address_t));
        if (!cache[i]) {
            exit(1);
        }
        for (int j = 0; j < E; ++j) {
            cache[i][j].valid = 0;
            cache[i][j].tag = 0;
            cache[i][j].lru_count = 0xffffffffffffffff;
        }
    }
}

void accessData(long add) {
    // access and evict data
    long group = (add >> b) & ((1 << s) - 1);
    long tag = ((add & 0x7fffffffffffffff) >> (b+s));
    printf("group: %ld tag: %ld ", group, tag);

    int store_index = -1;
    int evict = 1;
    unsigned long min_lru_count = 0xffffffffffffffff;
    struct address_t cache_addr;

    for (unsigned j = 0; j < E; ++j) {
        cache_addr = cache[group][j];

        if (cache_addr.valid) {

            if (cache_addr.tag == tag) {
                // refresh lru_count
                cache[group][j].lru_count = ++lru_count;
                if (verbosity) {
                    printf("hit ");
                }
                ++hit_count;
                return;
            }

            if (evict && cache_addr.lru_count < min_lru_count) {
                store_index = j;
                min_lru_count = cache_addr.lru_count;
            }
        } else {
            evict = 0;
            store_index = j;
        }
    }

    if (verbosity) {
        printf("miss ");
    }
    ++miss_count;

    cache[group][store_index].valid = 1;
    cache[group][store_index].tag = tag;
    cache[group][store_index].lru_count = ++lru_count;
    if (evict) {
        if (verbosity) {
            printf("eviction ");
        }
        ++eviction_count;
    }
}

void parseData(char instructor, long add, int size) {
    if (verbosity) {
        printf("%c %lx,%d ", instructor, add, size);
    }
    if (instructor == 'L' || instructor == 'S') {
        accessData(add);
    } else if (instructor == 'M') {
        accessData(add);
        accessData(add);
    }

    printf("\n");
}

void replayCache() {
    FILE* f = fopen(tracefile, "r");

    char instructor;
    long address;
    int size;
    while (fscanf(f, "%c %lx,%d\n", &instructor, &address, &size) != EOF) {
        if (instructor != 'I') {
            parseData(instructor, address, size);
        }
    }

    fclose(f);
}

void freeCache() {
    for (int i = 0; i < S; ++i) {
        free(cache[i]);
    }
    free(cache);
}

void printHelp() {
    printf("Usage: ./csim-ref [-hv] -s <num> -E <num> -b <num> -t <file>\n"
        "Options:\n"
        "  -h         Print this help message.\n"
        "  -v         Optional verbose flag.\n"
        "  -s <num>   Number of set index bits.\n"
        "  -E <num>   Number of lines per set.\n"
        "  -b <num>   Number of block offset bits.\n"
        "  -t <file>  Trace file.\n\n"
        "Examples:\n"
        "  linux>  ./csim-ref -s 4 -E 1 -b 4 -t traces/yi.trace\n"
        "  linux>  ./csim-ref -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
    exit(0);
}

int main(int argc, char *argv[])
{
    int opt = 0;

    char flag = 0;
    while ((opt = getopt(argc, argv, "s:E:b:t:hv")) != -1) {
        switch (opt) {
            case 's':
                flag |= 1;
                s = atoi(optarg);
                S = 2 << s;
                break;
            case 'E':
                flag |= (1 << 1);
                E = atoi(optarg);
                break;
            case 'b':
                flag |= (1 << 2);
                b = atoi(optarg);
                B = 2 << b;
                break;
            case 't':
                flag |= (1 << 3);
                tracefile = optarg;
                break;
            case 'h':
                printHelp();
            case 'v':
                verbosity = 1;
                break;
            default:
                printHelp();
        }

    }

    if (flag != 0x0f) {
        printHelp();
    }

    initCache();
    replayCache();
    freeCache();
    printSummary(hit_count, miss_count, eviction_count);
    return 0;
}
