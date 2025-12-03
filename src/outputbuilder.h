#ifndef OUTPUTBUILDER_H
#define OUTPUTBUILDER_H

#include <stdint.h>

// error codes
#define ERR_OK 0
#define ERR_SYNTAX 1
#define ERR_UNKNOWN_CHAR 2
#define ERR_TOKEN_OVERFLOW 3
#define ERR_NODE_POOL 4

int build_truth_table(const char *expr, uint8_t outputs[8]);

#endif
