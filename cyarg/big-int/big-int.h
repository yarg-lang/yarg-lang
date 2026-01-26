//
//  big-int.h
//  BigInt
//
//  Created by dlm on 18/01/2026.
//

#include <stdint.h>
#include <stdbool.h>

#define INT_MAX_DIGITS 64

typedef struct Int
{
    bool neg_;
    bool overflow_;
    uint8_t d_; // num digits 1 - 64
    uint8_t m_; // max digits always 64 in this implementation
    union
    {
        uint16_t h_[INT_MAX_DIGITS];
        uint32_t w_[INT_MAX_DIGITS / 2];
    };
} Int;

typedef enum
{
    INT_LT,
    INT_EQ,
    INT_GT
} IntComp;


// constructors
void int_init(Int *);
void int_set_i(int64_t, Int *);
void int_set_s(char const *, Int *);
void int_set_t(Int const *, Int *);

// funtions
void int_add(Int const *, Int const *, Int *);
void int_sub(Int const *, Int const *, Int *);
void int_shift(int, Int *); // by half words
void int_mul(Int const *, Int const *, Int *);
void int_div(Int const *, Int const *, Int *i, Int *r); // r may be nil and not returned
void int_neg(Int *);

// comparisons
bool int_is_zero(Int const *);
IntComp int_is(Int const *, Int const *);
IntComp int_is_abs(Int const *, Int const *);

// output
char const *int_to_s(Int const *, char *, int n);
int64_t int_to_i64(Int const *);
uint64_t int_to_u64(Int const *);
int32_t int_to_i32(Int const *);
uint32_t int_to_u32(Int const *);

// testing
void int_invariant(Int const *);
void int_make_random(Int *); // constructor
void int_print(Int const *); // todo deprecated: remove once added to language
void int_for_bc(Int const *); // todo deprecated: remove once added to language
void int_dump(Int const *); // todo deprecated: remove once added to language
void int_run_tests(void); // todo deprecated: remove once added to language
