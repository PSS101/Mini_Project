/*
 * Declares the hashing helper API for the core engine.
 * It exposes the shared hash function used across modules.
 */

#ifndef HASH_UTIL_H
#define HASH_UTIL_H

unsigned int hash(const char *key, int size);

#endif
