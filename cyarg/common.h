#ifndef cyarg_common_h
#define cyarg_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UINT8_COUNT (UINT8_MAX + 1)
// see: https://stackoverflow.com/questions/43787672/the-max-number-of-digits-in-an-int-based-on-number-of-bits
#define DECIMAL_DIGITS_BOUND(t) (241 * sizeof(t) / 100 + 1)

#include "print.h"

void fatalVMError(const char* format, ...);

#endif
