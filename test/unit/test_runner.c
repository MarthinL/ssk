#include <stdio.h>

// Forward declaration from test_sequences.c
void run_all_tests(void);

// Forward declaration from test_hand_crafted.c
void run_hand_crafted_tests(void);

int main(void) {
    printf("Running SSK test sequences...\n");
    run_all_tests();
    
    printf("\nRunning hand-crafted SSKDecoded tests...\n");
    run_hand_crafted_tests();

    return 0;
}

