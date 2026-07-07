/*
 * Provides hashing helpers used by the core engine.
 * It implements a simple deterministic hash function for internal use.
 */

#include "hash.h"

unsigned int hash(const char *key, int size) {
    unsigned int h = 0;
    while (*key) {
        h = h * 31 + (unsigned char)*key++;
    }
    return h % (unsigned int)size;
}
