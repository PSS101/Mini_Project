/*
 * main.c  —  Dev CLI entry point
 *
 * Usage:
 *   ./miniredis              (no persistence)
 *   ./miniredis miniredis.aof (with AOF persistence)
 */

#include <stddef.h>
#include "interface/dev_cli.h"

int main(int argc, char *argv[]) {
    const char *aof_path = (argc >= 2) ? argv[1] : NULL;
    start_dev_cli(aof_path);
    return 0;
}
