// main.c - simple host-side tester
#include <stdio.h>
#include <stdint.h>
#include "outputbuilder.h"

static void print_truth_table(const char *expr)
{
    uint8_t outputs[8];
    int err = build_truth_table(expr, outputs);

    printf("Expression: \"%s\"\n", expr);
    if (err != ERR_OK)
    {
        printf("  Error: %d\n\n", err);
        return;
    }

    printf("  A B C | F\n");
    printf("  -------+---\n");
    for (int row = 0; row < 8; ++row)
    {
        int A = (row >> 2) & 1;
        int B = (row >> 1) & 1;
        int C = (row >> 0) & 1;
        printf("  %d %d %d | %d\n", A, B, C, outputs[row]);
    }

    printf("  outputs[] = { ");
    for (int i = 0; i < 8; ++i)
    {
        printf("%d", outputs[i]);
        if (i != 7)
            printf(", ");
    }
    printf(" }\n\n");
}

int main(void)
{
    // Valid expressions 
    print_truth_table("A");
    print_truth_table("B");
    print_truth_table("C");
    print_truth_table("A & B");
    print_truth_table("A | B");
    print_truth_table("A & !B");
    print_truth_table("!(A & B) | C");
    print_truth_table("(A | B) & !C");
    print_truth_table("!A | C");
    print_truth_table(" ( A & B ) | C "); // with spaces

    // Invalid expressions - should return non-zero errors 
    print_truth_table("A && B"); // invalid char '&'
    print_truth_table("A &");    // incomplete
    print_truth_table("& A");    // starts with operator
    print_truth_table("(A & B"); // missing ')'
    print_truth_table("X | A");  // unknown variable

    return 0;
}
