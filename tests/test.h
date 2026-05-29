#pragma once 

#include "mm.h"
#include <stdio.h>
#include <stdint.h>

extern int failures;

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAILED: %s in %s at %s:%d\n", \
                #cond, __func__, __FILE__, __LINE__); \
        failures++; \
    } \
} while(0)

