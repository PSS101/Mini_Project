/*
 * test_runner.c — Simple test framework and main entry point
 *
 * Runs all test suites: core data structures, commands, persistence.
 * Reports pass/fail counts with colored output.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Test framework macros & globals                                     */
/* ------------------------------------------------------------------ */

int g_tests_run    = 0;
int g_tests_passed = 0;
int g_tests_failed = 0;
const char *g_current_suite = "";

#define RESET  "\033[0m"
#define RED    "\033[31m"
#define GREEN  "\033[32m"
#define YELLOW "\033[33m"
#define CYAN   "\033[36m"

#define TEST_SUITE(name)  do { \
    g_current_suite = (name); \
    printf("\n" CYAN "━━━ %s " RESET "\n", (name)); \
} while(0)

#define ASSERT_TRUE(expr) do { \
    g_tests_run++; \
    if (expr) { g_tests_passed++; printf(GREEN "  ✓ " RESET "%s:%d: %s\n", __FILE__, __LINE__, #expr); } \
    else      { g_tests_failed++; printf(RED   "  ✗ " RESET "%s:%d: %s\n", __FILE__, __LINE__, #expr); } \
} while(0)

#define ASSERT_EQ_INT(a, b) do { \
    int _a = (a), _b = (b); \
    g_tests_run++; \
    if (_a == _b) { g_tests_passed++; printf(GREEN "  ✓ " RESET "%s == %s (%d)\n", #a, #b, _a); } \
    else          { g_tests_failed++; printf(RED   "  ✗ " RESET "%s == %s (got %d, expected %d)\n", #a, #b, _a, _b); } \
} while(0)

#define ASSERT_EQ_STR(a, b) do { \
    const char *_a = (a), *_b = (b); \
    g_tests_run++; \
    if (_a && _b && strcmp(_a, _b) == 0) { g_tests_passed++; printf(GREEN "  ✓ " RESET "%s == \"%s\"\n", #a, _b); } \
    else { g_tests_failed++; printf(RED "  ✗ " RESET "%s == \"%s\" (got \"%s\")\n", #a, _b, _a ? _a : "(null)"); } \
} while(0)

#define ASSERT_NULL(a) do { \
    const void *_a = (a); \
    g_tests_run++; \
    if (_a == NULL) { g_tests_passed++; printf(GREEN "  ✓ " RESET "%s is NULL\n", #a); } \
    else            { g_tests_failed++; printf(RED   "  ✗ " RESET "%s expected NULL, got %p\n", #a, _a); } \
} while(0)

#define ASSERT_NOT_NULL(a) do { \
    const void *_a = (a); \
    g_tests_run++; \
    if (_a != NULL) { g_tests_passed++; printf(GREEN "  ✓ " RESET "%s is not NULL\n", #a); } \
    else            { g_tests_failed++; printf(RED   "  ✗ " RESET "%s expected non-NULL\n", #a); } \
} while(0)

/* ------------------------------------------------------------------ */
/*  External test suite declarations                                    */
/* ------------------------------------------------------------------ */

void test_core(void);
void test_commands(void);
void test_persistence(void);

/* ------------------------------------------------------------------ */
/*  Main                                                                */
/* ------------------------------------------------------------------ */

int main(void) {
    printf(YELLOW "\n╔══════════════════════════════════════╗\n"
                    "║    Mini Redis — Test Suite           ║\n"
                    "╚══════════════════════════════════════╝\n" RESET);

    test_core();
    test_commands();
    test_persistence();

    printf("\n" YELLOW "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n" RESET);
    printf("  Total : %d\n", g_tests_run);
    printf(GREEN "  Pass  : %d\n" RESET, g_tests_passed);
    if (g_tests_failed > 0)
        printf(RED "  Fail  : %d\n" RESET, g_tests_failed);
    else
        printf("  Fail  : 0\n");
    printf(YELLOW "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n" RESET);

    return g_tests_failed > 0 ? 1 : 0;
}
