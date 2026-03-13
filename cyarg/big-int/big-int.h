//
//  big-int.h
//  BigInt
//
//  Created by dlm on 18/01/2026.
//
#ifndef cyarg_bigint_bigint_h
#define cyarg_bigint_bigint_h

#include <stdint.h>
#include <stdbool.h>

#define INT_DIGITS_FOR_INT8 1
#define INT_DIGITS_FOR_INT16 1
#define INT_DIGITS_FOR_INT32 2
#define INT_DIGITS_FOR_INT64 4
#define INT_DIGITS_FOR_ADDRESS 4 // target address size could be 32 or 64
#define INT_DIGITS_FOR_S(STRLEN) ((1651124 + (int)(STRLEN) * 342808) / 1651125) // multiply by 1/log10(65536) - fails at 1224 decimal digits (255 (16-bit) digits)
#define INT_STRLEN_FOR_INT254 1226 // ceil(log10(pow(65536, 254))) + 1 (for null) + 1 for '-'

typedef struct
{
    bool neg_;
    bool overflow_;
    uint8_t d_; // num (16-bit) digits 1..m_
    uint8_t m_; // max (16-bit) digits - allocated size, always even
    uint32_t w_[];
} Int;

typedef struct // smallest concrete type of int - can be a global or a local or const, any IntConcreteX *obj may be cast to (Int *)
{
    bool neg_;
    bool overflow_;
    uint8_t d_;
    uint8_t m_; // {2}
    uint32_t w_[2 / 2];
} IntConcrete2;

typedef struct // concrete type of int which can hold uint64_t/int64_t - can be a global or a local
{
    bool neg_;
    bool overflow_;
    uint8_t d_;
    uint8_t m_; // {4}
    uint32_t w_[4 / 2];
} IntConcrete4;

typedef struct // largest concrete type of int - can be a global or a local - use sparingly as takes up 512 bytes
{
    bool neg_;
    bool overflow_;
    uint8_t d_;
    uint8_t m_; // {254}
    uint32_t w_[254 / 2];
} IntConcrete254;

typedef enum
{
    INT_LT,
    INT_EQ,
    INT_GT
} IntComp;

typedef enum
{
    INT_BELOW,
    INT_WITHIN,
    INT_ABOVE
} IntRange;


// constructors
void int_init(Int *);
Int *int_init_concrete2(IntConcrete2 *);
Int *int_init_concrete4(IntConcrete4 *);
Int *int_init_concrete254(IntConcrete254 *);
void int_set_i(int64_t, Int *);
void int_set_u(uint64_t, Int *);
void int_set_s(char const *, Int *);
void int_set_t(Int const *, Int *);

// funtions
void int_add(Int const *, Int const *, Int *);
void int_sub(Int const *, Int const *, Int *);
void int_shift(int, Int *); // by half words
void int_mul(Int const *, Int const *, Int *);
void int_div(Int const *, Int const *, Int *q, Int *r); // r may be nil  // todo -- also allow q == 0
void int_neg(Int *);

// comparisons
bool int_is_zero(Int const *);
IntComp int_is(Int const *, Int const *);
IntComp int_is_abs(Int const *, Int const *);
IntRange int_is_range(Int const *, int64_t, uint64_t);

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
void int_run_tests(void); // todo deprecated: remove once added to language

#endif
