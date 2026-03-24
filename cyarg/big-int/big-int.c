//
//  big-int.c
//  BigInt
//
//  Created by dlm on 18/01/2026.
//

#include "big-int.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h> // should cause runtime error in target

union TwoDigits
{
    struct
    {
        uint16_t l16_;
        uint16_t h16_;
    };
    int32_t i32_;
    uint32_t u32_;
};
typedef union TwoDigits TwoDigits;

union FourDigits
{
    struct
    {
        uint16_t a16_;
        uint16_t b_[3];
    };
    struct
    {
        uint32_t l32_;
        uint32_t h32_;
    };
    int64_t ll64_;
    uint64_t ull64_;
};
typedef union FourDigits FourDigits;

/*
 a +  b
 a + -b        => a - b
 -a +  b        => b - a
 -a + -b        => -(a + b)

 a -  b
 a - -b        => a + b
 -a -  b        => -(a + b)
 -a - -b        => b - a

 A - a
 a - A        => -(A - a)
 */

static void addPos(Int const *, Int const *, Int *);
static void int_sub_abs(Int const *, Int const *, Int *);
static void subAGtB(Int const *, Int const *, Int *);

void int_init(Int *i)
{
    assert(i->m_ != 0 && i->m_ % 2 == 0);
    i->neg_ = false;
    i->overflow_ = false;
    i->d_ = 1;
    i->w_[0] = 0;
}

Int *int_init_concrete2(IntConcrete2 *i)
{
    i->m_ = 2;
    int_init((Int *) i);
    return (Int *) i;
}

Int *int_init_concrete4(IntConcrete4 *i)
{
    i->m_ = 4;
    int_init((Int *) i);
    return (Int *) i;
}

Int *int_init_concrete254(IntConcrete254 *i)
{
    i->m_ = 254;
    int_init((Int *) i);
    return (Int *) i;
}


void int_set_i(int64_t to, Int *i)
{
    assert(i->m_ != 0 && i->m_ % 2 == 0);
    i->overflow_ = false;

    uint64_t v;
    if (to < 0)
    {
        if (to == INT64_MIN)
        {
            v = 9223372036854775808ull;
        }
        else
        {
            v = (uint64_t) -to;
        }
    }
    else
    {
        v = (uint64_t) to;
    }
    int_set_u(v, i);
    i->neg_ = to < 0;
}

void int_set_u(uint64_t to, Int *i)
{
    assert(i->m_ != 0 && i->m_ % 2 == 0);
    i->overflow_ = false;
    i->neg_ = false;

    uint32_t *ip = i->w_;
    for (; ip < &i->w_[i->m_ / 2] && to > 0;)
    {
        *ip++ = (uint32_t) to;
        to /= 4294967296u;
    }
    if (to > 0)
    {
        i->overflow_ = true;
    }
    i->d_ = (uint8_t)((ip - i->w_) * 2);
    if (i->d_ == 0)
    {
        i->w_[0] = 0u;
        i->d_ = 1;
    }
    else
    {
        uint16_t const *const h = (uint16_t *) i->w_;
        if (h[i->d_ - 1] == 0u) // overshot
        {
            i->d_--;
        }
    }
}

static IntConcrete2 const ten = {.m_ = 2, .d_ = 1, .w_[0] = 10};

void int_set_s(char const *s, Int *i)
{
    int_init(i);
    IntConcrete2 digit;
    int_init_concrete2(&digit);
    IntConcrete254 acc;
    int_init_concrete254(&acc);

    bool neg;
    if (*s == '-')
    {
        s++;
        neg = true;
    }
    else
    {
        neg = false;
    }
    while (*s != 0)
    {
        int_mul((Int *) &ten, i, (Int *) &acc); // todo mul by uint16 optimisation
        int d = *s++ - '0';
        int_set_i(d, (Int *) &digit); // todo add to uint16 optimisation
        int_add((Int *) &acc, (Int *) &digit, i);
    }
    i->neg_ = neg;
}

void int_set_t(Int const *v, Int *i)
{
    assert(i->m_ != 0 && i->m_ % 2 == 0 && i->m_ >= v->d_);
    i->neg_ = v->neg_;
    i->overflow_ = v->overflow_;
    i->d_ = v->d_;
    assert(i->m_ >= v->d_);
    memcpy(i->w_, v->w_, (v->d_ + 1) / 2 * sizeof v->w_[0]);
}

void int_add(Int const *a, Int const *b, Int *r)
{
    if (a->neg_)
    {
        if (b->neg_)
        {
            addPos(a, b, r);
            r->neg_ = true;
        }
        else
        {
            int_sub_abs(b, a, r);
        }
    }
    else
    {
        if (b->neg_)
        {
            int_sub_abs(a, b, r);
        }
        else
        {
            addPos(a, b, r);
            r->neg_ = false;
        }
    }
}

void addPos(Int const *a, Int const *b, Int *r)
{
    uint16_t carry = 0;
    uint16_t *rp = (uint16_t *) r->w_;
    uint16_t *const re = &rp[r->m_];
    uint16_t const *ap = (uint16_t const *) a->w_;
    uint16_t const *bp = (uint16_t const *) b->w_;
    uint16_t const *const ae = &ap[a->d_];
    uint16_t const *const be = &bp[b->d_];
    for (; ap < ae || bp < be; ap++, bp++)
    {
        TwoDigits ps;
        if (ap >= ae)
        {
            ps.u32_ = (uint32_t) *bp + (uint32_t) carry;
        }
        else if (bp >= be)
        {
            ps.u32_ = (uint32_t) *ap + (uint32_t) carry;
        }
        else
        {
            ps.u32_ = (uint32_t) *ap + (uint32_t) *bp + (uint32_t) carry;
        }
        if (rp < re)
        {
            *rp++ = ps.l16_;
            carry = ps.h16_;
        }
        else
        {
            if (ps.u32_ != 0)
            {
                r->overflow_ = true;
                return;
            }
        }
    }
    if (rp < re)
    {
        *rp = carry; // only needs to be done (if carry is zero) of odd number of digits but saves a test
        if (carry > 0)
        {
            rp++;
            if (rp < re) // only needs to be done of odd number of digits but saves a test
            {
                *rp = 0;
            }
        }
    }
    else
    {
        if (carry > 0)
        {
            r->overflow_ = true;
        }
    }
    r->d_ = (uint8_t)(rp - (uint16_t *) r->w_);
}

void int_sub(Int const *a, Int const *b, Int *r)
{
    if (a->neg_)
    {
        if (b->neg_)
        {
            int_sub_abs(b, a, r);
        }
        else
        {
            addPos(a, b, r);
            r->neg_ = true;
        }
    }
    else
    {
        if (b->neg_)
        {
            addPos(a, b, r);
            r->neg_ = false;
        }
        else
        {
            int_sub_abs(a, b, r);
        }
    }
}

void int_shift(int shift, Int *i)
{
    uint16_t *const h = (uint16_t *) i->w_;
    if (i->d_ != 1 || h[0] != 0u)
    {
        if (shift >= 0)
        {
            if (i->d_ + shift > i->m_)
            {
                i->overflow_ = true;
                return;
            }

            memmove(&h[shift], &h[0], i->d_ * sizeof h[0]);
            memset(&h[0], 0, shift * sizeof h[0]);
            i->d_ += shift;
            if (i->d_ < i->m_)
            {
                h[i->d_] = 0u; // only need to do if rd.d_ + shift && shift is odd - but why not
            }
        }
        else
        {
            assert(i->d_ + shift >= 0);

            memmove(&h[0], &h[-shift], ((int)i->d_ + shift) * sizeof h[0]);
            memset(&h[i->d_ + shift], 0, -shift *  sizeof h[0]);
            i->d_ += shift;
            if (i->d_ == 0)
            {
                i->d_ = 1;
                assert(h[0] == 0u);
            }
        }
    }
}

void int_sub_abs(Int const *a, Int const *b, Int *r)
{
    IntComp comp = int_is_abs(a, b);
    if (comp == INT_LT)
    {
        subAGtB(b, a, r);
        r->neg_ = true;
    }
    else
    {
        subAGtB(a, b, r);
        r->neg_ = false;
    }
}

static void subAGtB(Int const *a, Int const *b, Int *r)
{
    uint16_t borrow = 0;
    uint16_t *rp = (uint16_t *) r->w_;
    uint16_t *const re = &rp[r->m_];
    uint16_t const *ap = (uint16_t const *) a->w_;
    uint16_t const *const ae = &ap[a->d_];
    uint16_t const *bp = (uint16_t const *) b->w_;
    uint16_t const *const be = &bp[b->d_];
    for (; ap < ae; ap++, bp++)
    {
        uint16_t ad = *ap, bd;
        if (bp < be)
        {
            bd = *bp;
        }
        else
        {
            bd = 0u;
        }
        TwoDigits pd;
        pd.u32_ = (uint32_t) bd + (uint32_t) borrow;
        if (pd.u32_ > ad)
        {
            borrow = 1;
            pd.u32_ = 0x10000u + (uint32_t) ad - pd.u32_;
            assert(pd.h16_ == 0u);
        }
        else
        {
            borrow = 0;
            pd.u32_ = (uint32_t) ad - pd.u32_;
            assert(pd.h16_ == 0u);
        }
        if (rp < re)
        {
            *rp++ = pd.l16_;
        }
        else
        {
            if (pd.l16_ != 0u)
            {
                r->overflow_ = true;
                return;
            }
        }
    }
    uint16_t *const rh = (uint16_t *) r->w_;
    r->d_ = (uint8_t)(rp - rh);
    if (r->d_ % 2 == 1)
    {
        rh[r->d_] = 0; // todo optimize - don’t need to do this if the following is true
    }
    while (r->d_ > 1 && rh[r->d_ - 1] == 0)
    {
        r->d_--;
    }
}

void int_mul(Int const *a, Int const *b, Int *r)
{
    IntConcrete254 pp;
    int_init_concrete254(&pp);

    int_set_i(0, r);
    uint16_t *const rh = (uint16_t *) r->w_;
    uint16_t const *const ah = (uint16_t const *) a->w_;
    uint16_t const *const bh = (uint16_t const *) b->w_;

    uint16_t *rpn = rh;
    for (uint16_t const *ap = ah; ap < &ah[a->d_]; ap++)
    {
        uint16_t *rp = rpn;
        rpn = rp + 1;
//        printf("%ld->%ld", ap - a->h_, rp - r->h_);
        for (uint16_t const *bp = bh; bp < &bh[b->d_]; bp++, rp++)
        {
//            printf(".%ld/%ld", ap - a->h_, bp - b->h_);
            int_set_i((uint32_t) *ap * (uint32_t) *bp, (Int *) &pp); // todo cast to int32 to use M0 single instruction mul
            if (rp - rh + pp.d_ > r->m_)
            {
                r->overflow_ = true;
                break;
            }
            int_shift((int)(rp - rh), (Int *) &pp);
//            printf("%d:%d := %d %d\n", (int)pp.h16_, (int)pp.l16_, (int)*ap, (int)*bp);
            int_add(r, (Int *) &pp, r); // todo add shifted uint32 optimisation
            if (r->overflow_)
            {
                break;
            }
        }
    }
    r->neg_ = a->neg_ ^ b->neg_;
}

/*
        _____1774
 473046)839264859 => /5 as first attempt
         473046 D*16
        7568736 N-
        -------
         82391259 RN>D
         473046 D*1
         473046 RN-
         ------
         35086659 RN<D Shift
          473046 SD*7
         3311322 RN-
         -------
          1973439 RN<SD Shift
           473046 SD*3
          1419138
          -------
          0554301 RN>D
           473046 *1
          =======
            81255 RN<SD && D==SD Terminate
 */
void int_div(Int const *n, Int const *d, Int *q, Int *r)
{
    if (int_is_abs(n, d) == INT_LT)
    {
        if (r != 0)
        {
            int_set_t(n, r);
            int_init(q);
            // adjust for yarg’s weird %
            if (n->neg_)
            {
                int_add(r, d, r);
            }
        }
        return;
    }

    if (r != 0 && r->m_ < d->d_)
    {
        r->overflow_ = true;
        return;
    }

    bool nSign = n->neg_;
    bool dSign = d->neg_;

    IntConcrete254 qDigit;
    int_init_concrete254(&qDigit);
    IntConcrete254 reduceBy;
    int_init_concrete254(&reduceBy);
    IntConcrete254 reducingNumerator;
    int_init_concrete254(&reducingNumerator);
    IntConcrete254 shiftingDenominator;
    int_init_concrete254(&shiftingDenominator);

    uint16_t const *const dh = (uint16_t const *) d->w_;
    uint32_t testDenominator = dh[d->d_ - 1];
    assert(testDenominator != 0);
    testDenominator++;

    // Shift denominator to be one digit to the right of the numerator
    // if n and d are the same length, assume numerator is one longer. Increase wirh a leading zero.
    int_set_t(n, (Int *) &reducingNumerator);
    reducingNumerator.neg_ = false;
    int nDigit = reducingNumerator.d_ - 1;

    int_set_t(d, (Int *) &shiftingDenominator);
    shiftingDenominator.neg_ = false;
    int shift = reducingNumerator.d_ - shiftingDenominator.d_ - 1;
    if (shift == -1)
    {
        shift = 0;
        nDigit++;
    }
    else if (shift >= 1)
    {
        int_shift(shift, (Int *) &shiftingDenominator);
    }
    else
    {
        assert(shift == 0);
    }
    int_init(q);

    while (1)
    {
        if (int_is_abs((Int *) &reducingNumerator, (Int *) &shiftingDenominator) == INT_LT) // should never be true on first iteration
        {
            if (shift == 0)
            {
                break;
            }
            else
            {
                shift--;
                nDigit--;
                //shifts++;
                int_shift(-1, (Int *) &shiftingDenominator);
                int_shift(1, q);
                if (q->overflow_)
                {
                    return;
                }
            }
        }

        uint16_t const *const rnh = (uint16_t const *) reducingNumerator.w_;
        TwoDigits guessNumerator;
        if (nDigit < 1)
        {
            assert(nDigit == 0);
            guessNumerator.u32_ = rnh[0];
        }
        else
        {
            if (nDigit >= reducingNumerator.d_) // todo - combine with the preceding test
            {
                guessNumerator.h16_ = 0u;
            }
            else
            {
                guessNumerator.h16_ = rnh[nDigit];
            }
            guessNumerator.l16_ = rnh[nDigit - 1];
        }
        uint32_t guessMultiplier = guessNumerator.u32_ / testDenominator; // todo compare div_u32u32();
        if (guessMultiplier == 0u)
        {
            guessMultiplier = 1u;
        }

        int_set_i(guessMultiplier, (Int *) &qDigit);
        int_mul((Int *) &shiftingDenominator, (Int *) &qDigit, (Int *) &reduceBy);

        if (int_is_abs((Int *) &reducingNumerator, (Int *) &reduceBy) != INT_LT)
        {
            subAGtB((Int *) &reducingNumerator, (Int *) &reduceBy, (Int *) &reducingNumerator);
            addPos((Int *) &qDigit, q, q);
        }
        else // Guessed too high
            ;
    }
    q->neg_ = nSign ^ dSign;
    if (r != 0)
    {
        int_set_t((Int *) &reducingNumerator, r);
        r->neg_ = nSign;
        // adjust for yarg’s weird %
        if (nSign)
        {
            int_add(r, d, r);
        }
    }
}

void int_neg(Int *i)
{
    i->neg_ = !i->neg_;
}

bool int_is_zero(Int const *i)
{
    uint16_t const *const ih = (uint16_t const *) i->w_;
    return ih[0] == 0 && i->d_ == 1;
}

IntComp int_is(Int const *a, Int const *b)
{
    uint16_t const *const ah = (uint16_t const *) a->w_;
    uint16_t const *const bh = (uint16_t const *) b->w_;
    IntComp r;
    if (a->neg_ != b->neg_) // check for zero and -ve zero
    {
        if (a->d_ == 1 && b->d_ == 1 && ah[0] == 0 && bh[0]  == 0)
        {
            r = INT_EQ;
        }
        else
        {
            r = a->neg_ ? INT_LT : INT_GT;
        }
    }
    else // same sign
    {
        r = int_is_abs(a, b);
        if (a->neg_)
        {
            if (r == INT_LT)
            {
                r = INT_GT;
            }
            else if (r == INT_GT)
            {
                r = INT_LT;
            }
        }
    }
    return r;
}

IntComp int_is_abs(Int const *a, Int const *b)
{
    if (a->d_ == b->d_) // same length
    {
        int last = (a->d_ + 1) / 2 - 1;
        uint32_t const *ap = &a->w_[last];
        for (uint32_t const *bp = &b->w_[last]; bp >= &b->w_[0]; ap--, bp--)
        {
            uint32_t ad = *ap, bd = *bp;
            if (ad > bd)
            {
                return INT_GT;
            }
            else if (ad < bd)
            {
                return INT_LT;
            }
        }
        return INT_EQ;
    }
    else if (a->d_ > b->d_)
    {
        return INT_GT;
    }
    else // (a->d_ < b->d_)
    {
        return INT_LT;
    }
}

IntRange int_is_range(Int const *i, int64_t l, uint64_t u)
{
    IntRange r;
    IntConcrete254 t;

    int_init_concrete254(&t);

    int_set_u(u, (Int *) &t);
    if (int_is(i, (Int *) &t) == INT_GT)
    {
        r = INT_ABOVE;
    }
    else
    {
        int_set_i(l, (Int *) &t);
        if (int_is(i, (Int *) &t) == INT_LT)
        {
            r = INT_BELOW;
        }
        else
        {
            r = INT_WITHIN;
        }
    }

    return r;
}

static const IntConcrete2 tenToTheFour = {.m_ = 2, .d_ = 1, .w_[0] = 10000u};
static const IntConcrete4 tenToTheNineteen = {.m_ = 4, .d_ = 4, .w_ = {2313682944u, 2328306436u}}; // 10000000000000000000

char const *int_to_s(Int const *i, char *s, int n)
{
    char *out = &s[n - 1];
    *out = '\0';

    IntConcrete254 v;
    int_init_concrete254(&v);

    int_set_t(i, (Int *) &v);
    v.neg_ = false;

    static IntConcrete4 r;
    int_init_concrete4(&r);

    while (!int_is_zero((Int *) &v))
    {
        int_div((Int *) &v, (Int *) &tenToTheNineteen, (Int *) &v, (Int *) &r);

        FourDigits rem = {.l32_ = r.w_[0], .h32_ = r.d_ > 2 ? r.w_[1] : 0u};
        bool leading = int_is_zero((Int *) &v);
        for (int c = 0; c < 19 && out > s; c++)
        {
            char ch = (char) (rem.ull64_ % 10u + '0');
            if (!leading || rem.ull64_ != 0)
            {
                *--out = ch;
            }
            rem.ull64_ /= 10u;
        }
    }
    if (out == &s[n - 1]) // todo optimise - break to for loop above if rem and v are zero
    {
        *--out = '0';
    }
    else if (i->neg_)
    {
        *--out = '-';
    }

    return out;
}

static inline uint16_t const *min(uint16_t const *a, uint16_t const *b)
{
    if (a < b) return a; else return b;
}

int64_t int_to_i64(Int const *i)
{
    FourDigits r;
    r.ll64_ = 0;
    uint16_t const *const ih = (uint16_t const *) i->w_;
    uint16_t const *s = ih;
    uint16_t const *e = min(&ih[4], &ih[i->d_]);
    uint16_t *d = &r.a16_;
    while (s < e)
    {
        *d++ = *s++;
    }
    if (i->neg_)
    {
        if (r.ull64_ >= 0x8000000000000000u)
        {
            r.ll64_ = INT64_MIN;
        }
        else
        {
            r.ll64_ = -r.ll64_;
        }
    }
    else
    {
        if (r.ull64_ > 0x7FFFFFFFFFFFFFFFu)
        {
            r.ll64_ = 0x7FFFFFFFFFFFFFFF;
        }
    }
    return r.ll64_;
}

uint64_t int_to_u64(Int const *i)
{
    FourDigits r;
    r.ull64_ = 0;
    uint16_t const *const ih = (uint16_t const *) i->w_;
    uint16_t const *s = ih;
    uint16_t const *e = min(&ih[4], &ih[i->d_]);
    uint16_t *d = &r.a16_;
    while (s < e)
    {
        *d++ = *s++;
    }
    return r.ull64_;
}

int32_t int_to_i32(Int const *i)
{
    TwoDigits r;
    r.i32_ = 0;
    uint16_t const *const ih = (uint16_t const *) i->w_;
    uint16_t const *s = ih;
    uint16_t const *e = min(&ih[2], &ih[i->d_]);
    uint16_t *d = &r.l16_;
    while (s < e)
    {
        *d++ = *s++;
    }
    if (i->neg_)
    {
        if (r.u32_ >= 0x80000000u)
        {
            r.i32_ = -0x80000000;
        }
        else
        {
            r.i32_ = -r.u32_;
        }
    }
    else
    {
        if (r.u32_ > 0x7FFFFFFF)
        {
            r.u32_ = 0x7FFFFFFF;
        }
    }
    return r.i32_;
}

uint32_t int_to_u32(Int const *i)
{
    TwoDigits r;
    r.u32_ = 0;
    uint16_t const *const ih = (uint16_t const *) i->w_;
    uint16_t const *s = ih;
    uint16_t const *e = min(&ih[2], &ih[i->d_]);
    uint16_t *d = &r.l16_;
    while (s < e)
    {
        *d++ = *s++;
    }
    return r.u32_;
}

void int_invariant(Int const *i)
{
    assert((int) i->neg_ == 1 || (int)i->neg_ == 0);
    assert(!i->overflow_);
    assert(i->m_ % 2 == 0);
    assert(i->d_ >= 1 && i->d_ <= i->m_);
    // check top half-word is zero if length is odd
    uint16_t const *const ih = (uint16_t const *) i->w_;
    if (i->d_ % 2 == 1)
    {
        assert(ih[i->d_] == 0);
    }
    // check no leading zeros
    if (i->d_ > 1)
    {
        assert(ih[i->d_ - 1] != 0);
    }
}

static const int randMaxDigits = 254;

void int_make_random(Int *i)
{
    i->overflow_ = false;
    i->neg_ = rand() > RAND_MAX / 2;
    i->d_ = 1 + rand() / (RAND_MAX / randMaxDigits);
    i->m_ = i->d_ + i->d_ % 2;
    assert(i->d_ <= randMaxDigits);
    uint16_t *const ih = (uint16_t *) i->w_;
    for (int x = 0; x < i->d_; x++)
    {
        ih[x] = (uint16_t) rand();
    }
    while (ih[i->d_ - 1] == 0)
    {
        ih[i->d_ - 1] = (uint16_t) rand();
    }
    if (i->d_ < i->m_)
    {
        ih[i->d_] = 0u;
    }
}

void int_print(Int const *i)
{
    char s[INT_STRLEN_FOR_INT254];
    const char *out = int_to_s(i, s, INT_STRLEN_FOR_INT254);
    printf("%s", out);
}

void int_for_bc(Int const *a)
{
    int i = 0;
    if (a->neg_)
    {
        printf("(-");
    }
    uint16_t const *const ah = (uint16_t const *) a->w_;
    for (; i < a->d_ - 1; i++)
    {
        printf("(%d+65536*", ah[i]);
    }
    printf("%d", ah[i]);
    for (i = 0; i < a->d_ - 1; i++)
    {
        printf(")");
    }
    if (a->neg_)
    {
        printf(")");
    }
}

#ifdef CYARG_FEATURE_TEST_INT
int main(void)
{
    int_run_tests();
    return 0;
}
#endif

void int_run_tests(void)
{
    char ss[INT_STRLEN_FOR_INT254];

    IntConcrete2 cl;
    Int *l = int_init_concrete2(&cl);

    IntConcrete254 c[9];
    Int *a, *b, *s, *r, *t, *d, *q, *p, *t2;
    Int **all[9] = {&a, &b, &s, &r, &t, &d, &q, &p, &t2};
    for (int i = 0; i < 9; i++)
    {
        *(all[i]) = int_init_concrete254(&c[i]);
    }

//    goto div;
//    goto rand;
    int_set_i(0, a);
    int_invariant(a);
    int_print(a); printf("\n");
    assert(int_is_zero(a));

    int_set_i(123, a);
    int_set_i(4567, b);
    int_invariant(a);
    int_invariant(b);
    assert(int_is(a, b) == INT_LT);
    assert(int_is_abs(a, b) == INT_LT);

    int_set_i(123, l);
    int_invariant(l);
    assert(int_is(l, a) == INT_EQ);

    int_set_i(-123, l);
    int_invariant(l);
    l->neg_ = false;
    assert(int_is(l, a) == INT_EQ);

    int_set_u(4294967296, l);
    assert(l->overflow_);

    int_set_i(9223372036854775807L, a);
    int_set_i(9223372036854775806ll, b);
    int_invariant(a);
    int_invariant(b);
    assert(int_is(a, b) == INT_GT);
    assert(int_is_abs(a, b) == INT_GT);

    int_set_i(0ll, a);
    int_set_i(-1, b);
    int_invariant(a);
    int_invariant(b);
    assert(int_is(a, b) == INT_GT);
    assert(int_is_abs(a, b) == INT_LT);

    int_set_i(0ll, b); b->neg_ = true;
    int_invariant(b);
    assert(int_is(a, b) == INT_EQ);
    assert(int_is_abs(a, b) == INT_EQ);

    int_set_i(-2, a);
    int_set_i(-1, b);
    int_invariant(a);
    int_invariant(b);
    assert(int_is(a, b) == INT_LT);
    assert(int_is_abs(a, b) == INT_GT);

    int_set_i(2, b);
    int_invariant(b);
    assert(int_is(a, b) == INT_LT);
    assert(int_is_abs(a, b) == INT_EQ);

    int_set_i(123, a);
    int_set_i(4567, b);
    int_invariant(a);
    int_invariant(b);
    int_add(a, b, r);
    int_invariant(r);
    int_set_i(123+4567, t);
    int_invariant(t);
    assert(int_is(r, t) == INT_EQ);

    int_set_i(1230000001, a);
    int_set_i(45675555555, b);
    int_invariant(a);
    int_invariant(b);
    int_add(a, b, r); //buggy
    int_invariant(r);
    int_set_i(1230000001+45675555555, t);
    int_invariant(t);
    assert(int_is(r, t) == INT_EQ);

    int_set_i(9223372036854775807L, a);
    int_set_i(9223372036854775806ll, b);
    int_invariant(a);
    int_invariant(b);
    int_add(a, b, r);
    int_invariant(r);
    int_set_t(r, a);
    int_invariant(a);
    int_set_i(3, b);
    int_invariant(b);
    int_add(a, b, r);
    int_invariant(r);

    int_set_i(4567, a);
    int_set_i(123, b);
    int_invariant(a);
    int_invariant(b);
    int_sub(a, b, r);
    int_invariant(r);
    int_set_i(4567-123, t);
    int_invariant(t);
    assert(int_is(r, t) == INT_EQ);

    int_set_i(1230000001, a);
    int_set_i(45675555555, b);
    int_invariant(a);
    int_invariant(b);
    int_sub(a, b, r);
    int_invariant(r);
    int_set_i(45675555555-1230000001, t);
    int_invariant(t);
    int_set_i(-1, a);
    int_set_t(t, b);
    int_invariant(a);
    int_invariant(b);
    int_mul(a, b, t);
    int_invariant(t);
    assert(int_is(r, t) == INT_EQ);

    int_set_i(1, a);
    int_set_i(2147483649, b);
    int_invariant(a);
    int_invariant(b);
    int_mul(a, b, r);
    int_invariant(r);
    assert(int_is(r, b) == INT_EQ);

    int_set_i(4567, a);
    int_set_i(123, b);
    int_invariant(a);
    int_invariant(b);
    int_mul(a, b, r);
    int_invariant(r);
    int_set_i(4567*123, t);
    int_invariant(t);
    assert(int_is(r, t) == INT_EQ);

    int_set_i(-4567, a);
    int_set_i(-123, b);
    int_invariant(a);
    int_invariant(b);
    int_mul(a, b, r);
    int_invariant(r);
    int_set_i(4567*123, t);
    int_invariant(t);
    assert(int_is(r, t) == INT_EQ);

    int_set_i(-4567, a);
    int_set_i(123, b);
    int_invariant(a);
    int_invariant(b);
    int_mul(a, b, r);
    int_invariant(r);
    int_set_i(-4567*123, t);
    int_invariant(t);
    assert(int_is(r, t) == INT_EQ);

    int_set_i(1230000001, a);
    int_set_i(45675, b);
    int_invariant(a);
    int_invariant(b);
    int_mul(a, b, r);
    int_invariant(r);
    int_set_i(45675*1230000001ll, t);
    int_invariant(t);
    assert(int_is(r, t) == INT_EQ);
    char const *rr = int_to_s(r, ss, sizeof ss / sizeof ss[0]);
    assert(strcmp(rr, "56180250045675") == 0);
    assert(int_to_i64(r) == 56180250045675);

    int_set_i(10000u, d);
    int_div(t, d, q, r);
    int_invariant(q);
    int_invariant(r);
    int_set_i(5618025004, t);
    assert(int_is(q, t) == INT_EQ);
    int_set_i(5675, t);
    assert(int_is(r, t) == INT_EQ);

    int_div(q, d, q, r);
    int_invariant(q);
    int_invariant(r);
    int_set_i(561802, t);
    assert(int_is(q, t) == INT_EQ);
    int_set_i(5004, t);
    assert(int_is(r, t) == INT_EQ);

    int_div(q, d, q, r);
    int_invariant(q);
    int_invariant(r);
    int_set_i(56, t);
    assert(int_is(q, t) == INT_EQ);
    int_set_i(1802, t);
    assert(int_is(r, t) == INT_EQ);

    int_div(q, d, q, r);
    int_invariant(q);
    int_invariant(r);
    int_set_i(0, t);
    assert(int_is(q, t) == INT_EQ);
    int_set_i(56, t);
    assert(int_is(r, t) == INT_EQ);

    int_set_s("-12345", a);
    int_set_i(-12345, b);
    int_invariant(a);
    int_invariant(b);
    assert(int_is(a, b) == INT_EQ);

    int_set_i(13421772800000, a);
    int_set_i(13421772800000, b);
    int_invariant(a);
    int_invariant(b);
    int_div(a, b, q, 0);
    int_invariant(q);
    int_set_i(13421772800000/13421772800000, t);
    int_invariant(t);
    assert(int_is(q, t) == INT_EQ);

    int_set_i(13421772800700, a);
    int_set_i(444, b);
    int_invariant(a);
    int_invariant(b);
    int_div(a, b, q, 0);
    int_invariant(q);
    int_set_i(13421772800700/444, t);
    int_invariant(t);
    assert(int_is(q, t) == INT_EQ);

    int64_t tm[][2] =
    {
        {7473558006549, 1234134},
        {74986764527274600, 123},
        {933866438576, 9876543},
        {1234134, 7473558006549},
        {123, 74986764527274600},
        {9876543, 933866438576}
    };

    for (int x = 0; x < sizeof tm / sizeof tm[0]; x++)
    {
        int_set_i(tm[x][0], a);
        int_set_i(tm[x][1], b);
        int_invariant(a);
        int_invariant(b);
        int_mul(a, b, p);
        int_invariant(p);
        int_set_i(tm[x][0] * tm[x][1], t);
        int_invariant(t);
        assert(int_is(p, t) == INT_EQ);
    }

    char const *bb = "45004569476097716405600188052489096276589659783372385398756134755718969568599212452839780406252029736959022214045678577312955372997557808011166351368242024169330770738287024721942327258131593587116073102711494427349137261153163351227207";
    int_set_s(bb, a);
    int_print(a);printf("\n%s\n", bb);
    int_for_bc(a);printf("\n");

    int outit=0;
    while (!int_is_zero(a))
    {
        if (outit) {int_for_bc(a);printf("\n");}
        int_div(a, (Int *) &tenToTheFour, a, r);
        if (outit) {int_for_bc(a);printf("\n");int_for_bc(r);printf("\n");outit--;}
        if (int_to_u32(r) == 6933)
        {
            outit=3;
        }
    }

    char const *tss[][3] =
    {
        {"45004569476097716405600188052489096276589659783372385398756134755718969568599212452839780406252029736959022214045678577320970782103350621657798638622201854375249828618559078165391725554391637336051126938411312935632157993634139676471535", "8015409105792813646632287253959830205919057880272053443449398296260043748935053835699818508283020732480976325244328", "45004569476097716405600188052489096276589659783372385398756134755718969568599212452839780406252029736959022214045678577312955372997557808011166351368242024169330770738287024721942327258131593587116073102711494427349137261153163351227207"}
    };

    for (int x = 0; x < sizeof tss / sizeof tss[0]; x++)
    {
        int_set_s(tss[x][0], a);
        int_set_s(tss[x][1], b);
        int_set_s(tss[x][2], t);
        int_invariant(a);
        int_invariant(b);
        int_invariant(t);
        int_print(a);printf(" - ");int_print(b);printf(" => ");int_print(t);printf("\n");
        int_for_bc(t);printf("\n");
        int_sub(a, b, d);
        int_invariant(d);
        int_print(a);printf(" - ");int_print(b);printf(" => ");int_print(d);printf("\n");
        int_for_bc(d);printf("\n");
        assert(int_is(d, t) == INT_EQ);
    }

    div: ;
    char const *tds[][4] =
    {// todo check tds[0]
        {"86563698400295818556941199448342856196291390874257029273792164197338062186852288654487642865217811935325618416908915876918033088986025954151860537725611169266995543550767296630441222794629126373296710055138561939273380431459084392932980372126152312002135410572748911223141469923397490440838165", "-18070117593565567760862671033472963723720349409289083511961936073838222132616574596573447005412193114047731785144138437416182746256138625965817112866449101053048990151277922361862768377405760670987464162", "-4790433595801254714867564177737572902819347095069069988266970457550288026377822578316716665", "10164136718382858196019698312755863418683370176918249611820182655092817691253129467912534954339906708648920291884680103762877562724608016668599591528984883066544947276159939115847509871050635948245178435"},
        {"45004569476097716405600188052489096276589659783372385398756134755718969568599212452839780406252029736959022214045678577312955372997557808011166351368242", "281474976649780", "159888349620839703309792504026546654755626214020039863340514619510330396486743913688737005770575136765815764191961104637097362438018055441", "213649870915262"},
        {"45004569476097716405600188052489096276589659783372385398756134755718969568599212452839780406252029736959022214045678577312955372997557808011166351368242", "10000", "4500456947609771640560018805248909627658965978337238539875613475571896956859921245283978040625202973695902221404567857731295537299755780801116635136", "8242"}
    };

    for (int x = 0; x < sizeof tds / sizeof tds[0]; x++)
    {
        int_set_s(tds[x][0], a);
        int_set_s(tds[x][1], b);
        int_set_s(tds[x][2], t);
        int_set_s(tds[x][3], t2);
        //        printf("%s - ", tds[x][0]);int_for_bc(a);printf("\n");
        //        printf("%s - ", tds[x][1]);int_for_bc(b);printf("\n");
        //        printf("%s - ", tds[x][2]);int_for_bc(t);printf("\n");
        //        printf("%s - ", tds[x][3]);int_for_bc(t2);printf("\n");
        int_invariant(a);
        int_invariant(b);
        int_invariant(t);
        int_invariant(t2);
        int_div(a, b, q, r);
        int_invariant(d);
        int_invariant(r);

        printf("%s\n%s\n", tds[x][0], tds[x][1]);int_for_bc(q);printf("\n");int_for_bc(r);printf("\n");

        //        int_for_bc(q);printf("%s\n", int_is(q, t) == INT_EQ ? "÷✔️" : "÷⨯");
        assert(int_is(q, t) == INT_EQ);
        //        int_for_bc(r);printf("%s\n", int_is(q, t) == INT_EQ ? "%✔️" : "%⨯");
        assert(int_is(r, t2) == INT_EQ);
    }
//    goto rand;
    char const *tms[][3] =
    {
        {"3351951982485649274893506249551461531869841455148098344430890360930441007518386744200468574541725856922507964546621512713438470702986642486608412251521023", "3351951982485649274893506249551461531869841455148098344430890360930441007518386744200468574541725856922507964546621512713438470702986642486608412251521023", "11235582092889474423308157442431404585112356118389416079589380072358292237843810195794279832650471001320007117491962084853674360550901038905802964414967126069706528367755543042756389622154817142782907388308623998771662556764388893318631168471652618870995561901857550396971275994213576295767236553777010966529"},
        {"2397347246804399775846299341967274735921924813569660373241507470851337453571810991011870651746218077498232562138857178167039275006720515490364277003123459405004422981033297552634257771878766246736362189341419332733400905032674753650288808962155716002977274996173255530038506604174013456449782", "74986764527274600", "179769313486231590772930519078902473361797697894230657273430081157732675805500963132708477322407536021120113879871393357658789768814416622492847430639474124377767893424865485276302219601246094119453082952085005768838150682342462881473913110540827237163350510684586298239947245938479716304835356329624224137200"},
        {"145664339112472057955562782549465838686720970246529677712007027727728654915512386120719854831329123110715784412285370435997055237773545354469488265163648456632560073237481088177055505805079589509285930824436411093801929678902341951096001820337846001457986337532704145773430799198854999785141124326551431", "1234134", "179769313486231590772930519078902473361797697894230657273430081157732675805500963132708477322407536021120113879871393357658789768814416622492847430639474124377767893424865485276302219601246094119453082952085005768838150682342462881473913110540827237163350510684586298239947245938479716304835356329624223745754"},
        {"1461539134034403177015695277064247750908924373123826481897805537867745331752040350672426644897622244074147267316027588273648697307434281483681686427963204263233885312397280368100018045538586131052464089041341510315757322620670429930682220410901034448482524477110457709267863788117721270771019157151416456400", "123", "179769313486231590772930519078902473361797697894230657273430081157732675805500963132708477322407536021120113879871393357658789768814416622492847430639474124377767893424865485276302219601246094119453082952085005768838150682342462881473913110540827237163350510684586298239947245938479716304835356329624224137200"},
        {"74986764527274600", "2397347246804399775846299341967274735921924813569660373241507470851337453571810991011870651746218077498232562138857178167039275006720515490364277003123459405004422981033297552634257771878766246736362189341419332733400905032674753650288808962155716002977274996173255530038506604174013456449782", "179769313486231590772930519078902473361797697894230657273430081157732675805500963132708477322407536021120113879871393357658789768814416622492847430639474124377767893424865485276302219601246094119453082952085005768838150682342462881473913110540827237163350510684586298239947245938479716304835356329624224137200"},
        {"1234134", "145664339112472057955562782549465838686720970246529677712007027727728654915512386120719854831329123110715784412285370435997055237773545354469488265163648456632560073237481088177055505805079589509285930824436411093801929678902341951096001820337846001457986337532704145773430799198854999785141124326551431", "179769313486231590772930519078902473361797697894230657273430081157732675805500963132708477322407536021120113879871393357658789768814416622492847430639474124377767893424865485276302219601246094119453082952085005768838150682342462881473913110540827237163350510684586298239947245938479716304835356329624223745754"},
        {"123", "1461539134034403177015695277064247750908924373123826481897805537867745331752040350672426644897622244074147267316027588273648697307434281483681686427963204263233885312397280368100018045538586131052464089041341510315757322620670429930682220410901034448482524477110457709267863788117721270771019157151416456400", "179769313486231590772930519078902473361797697894230657273430081157732675805500963132708477322407536021120113879871393357658789768814416622492847430639474124377767893424865485276302219601246094119453082952085005768838150682342462881473913110540827237163350510684586298239947245938479716304835356329624224137200"},
        {"13407807929942597099574024998205846127479365820592393377723561443721764030073546976801874298166903427690031858186486050853753882811946569946433649006084095", "13407807929942597099574024998205846127479365820592393377723561443721764030073546976801874298166903427690031858186486050853753882811946569946433649006084095", "179769313486231590772930519078902473361797697894230657273430081157732675805500963132708477322407536021120113879871393357658789768814416622492847430639474097562152033539671286128252223189553839160721441767298250321715263238814402734379959506792230903356495130620869925267845538430714092411695463462326211969025"},
        {"13407807929942597099574024998205846127479365820592393377723561443721764030073546976801874298166903427690031858186486050853753882811946569946433649006084094", "13407807929942597099574024998205846127479365820592393377723561443721764030073546976801874298166903427690031858186486050853753882811946569946433649006084095", "179769313486231590772930519078902473361797697894230657273430081157732675805500963132708477322407536021120113879871393357658789768814416622492847430639474084154344103597074186554227224983707711681355621174904872598153819517050372660832982704917932736453067440589011738781794684676831280465125517028677205884930"}
    };

    for (int x = 0; x < sizeof tms / sizeof tms[0]; x++)
    {
        int_set_s(tms[x][0], a);
        int_set_s(tms[x][1], b);
        int_set_s(tms[x][2], t);
        int_invariant(a);
        int_invariant(b);
        int_invariant(t);
        int_print(a);printf(" * ");int_print(b);printf(" - ");int_print(t);printf("\n");
        int_mul(a, b, p);
        int_invariant(p);
        int_print(a);printf(" * ");int_print(b);printf(" - ");int_print(p);printf("\n");
        assert(int_is(p, t) == INT_EQ);
    }

    int64_t td[][2] =
    {
        {13421772800000, 13421772800000},
        {13421772800700, 444},
        {-13421772800700, 444},
        {13421772800700, -444},
        {-13421772800700, -444},
        {9223372036854775807, 9223372036854775807},
        {9223372036854775807, 9223372036854775806},
        {9223372036854775806, 9223372036854775807},
        {9223372036854775807, 1},
        {9223372036854775806, 1},
        {9223372036854775807, 11},
        {9223372036854775806, 16},
        {9223372036854775806, 21},
        {9223372036854775806, 65536},
        {9223372036854775806, 4294967296}
    };

    for (int x = 0; x < sizeof td / sizeof td[0]; x++)
    {
        int_set_i(td[x][0], a);
        int_set_i(td[x][1], b);
        int_invariant(a);
        int_invariant(b);
        int_div(a, b, q, r);
        int_invariant(q);
        int_invariant(r);
        int_set_i(td[x][0] / td[x][1], t);
        int_invariant(t);
        assert(int_is(q, t) == INT_EQ);
        int_set_i(td[x][0] % td[x][1], t);
        if (a->neg_) // put r back to normal c/bc modulus
        {
            int_sub(r, b, r);
        }
        int_invariant(t);
        int_print(r);printf(" ");int_print(t);printf("\n");
        assert(int_is(r, t) == INT_EQ);
    }

//    return;
rand: // pipe the output from here into `bc -lLS 0`
    for (int x = 0; x < 100000; x++)
    {
        s->m_ = (rand() % 127 + 1) * 2;
        d->m_ = (rand() % 127 + 1) * 2;
        q->m_ = (rand() % 127 + 1) * 2;
        p->m_ = (rand() % 127 + 1) * 2;

        printf("%d\n", x);

        int_make_random(a);int_invariant(a);
        int_make_random(b);int_invariant(b);

        int_add(a, b, s);
        if (!s->overflow_)
        {
            int_invariant(s);
            int_for_bc(a);printf(" + ");int_for_bc(b);printf(" - ");int_for_bc(s);printf("\n");
        }
        else
        {
            //            printf("%d\n", x);
            s->overflow_ = false;
        }

        int_sub(a, b, d);
        if (!d->overflow_)
        {
            int_invariant(d);
            int_for_bc(a);printf(" - ");int_for_bc(b);printf(" - ");int_for_bc(d);printf("\n");
        }
        else
        {
            //            printf("%d\n", x);
            d->overflow_ = false;
        }

        int_mul(a, b, p);
        if (!p->overflow_)
        {
            int_invariant(p);
            int_print(a);printf(" * ");int_print(b); printf(" - ");int_print(p);printf("\n");
        }
        else
        {
            //            printf("%d\n", x);
            p->overflow_ = false;
        }

        int_div(a, b, q, r);
        if (!q->overflow_ && !r->overflow_)
        {
            int_invariant(q); int_invariant(r);

            if (a->neg_) // put r back to normal c/bc modulus
            {
                int_sub(r, b, r);
            }

            int_for_bc(a);printf(" / ");int_for_bc(b);printf(" - ");int_for_bc(q);printf("\n");
            int_for_bc(a);printf(" %% ");int_for_bc(b);printf(" - ");int_for_bc(r);printf("\n");
        }
        else
        {
            //            printf("%d\n", x);
            q->overflow_ = false;
            r->overflow_ = false;
        }
    }
}
