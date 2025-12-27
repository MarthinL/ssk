#ifdef TRIVIAL

/*
 * Test runner for TRIVIAL implementation.
 * Tests will validate basic bijection and correctness properties.
 */

/* TODO: Implement trivial test runner */

#include <stdio.h>

// Forward declaration from test_sequences.c
int run_all_tests(void);

// Forward declaration from test_hand_crafted.c
int run_hand_crafted_tests(void);

int main(void) {
    printf("Running TRIVIAL mode tests...\n");
    int fail1 = run_all_tests();
    int fail2 = run_hand_crafted_tests();
    
    int total_fail = fail1 + fail2;
    if (total_fail > 0) {
        printf("\nTotal failures: %d\n", total_fail);
        return 1;
    }
    return 0;
}

#else // NON TRIVIAL

#include <stdio.h>
#include "ssk.h"
#include "cdu.h"

// Forward declaration from test_sequences.c
int run_all_tests(void);

// Forward declaration from test_hand_crafted.c
int run_hand_crafted_tests(void);

int main(void) {
    // Initialize required components
    cdu_init();
    ssk_combinadic_init();
    
    printf("Running SSK test sequences...\n");
    printf("Running SSK test sequences...\n");
    int fail1 = run_all_tests();
    
    printf("\nRunning hand-crafted AbV tests...\n");
    int fail2 = run_hand_crafted_tests();

    int total_fail = fail1 + fail2;
    if (total_fail > 0) {
        printf("\nTotal failures: %d\n", total_fail);
        return 1;
    }
    return 0;
}

#endif // (NON) TRIVIAL
