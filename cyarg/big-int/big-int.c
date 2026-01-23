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
#include <assert.h> // will cause runtime error in target

union TwoDigits
{
    struct
    {
        uint16_t l16_;
        uint16_t h16_;
    };
    uint32_t u32_;
    int32_t i32_;
};
typedef union TwoDigits TwoDigits;

union FourDigits
{
    struct
    {
        uint16_t a16_;
        uint16_t b16_;
        uint16_t c16_;
        uint16_t d16_;
    };
    struct
    {
        uint32_t x16_;
        uint32_t y16_;
    };
    int64_t ll64_;
    uint64_t ull64_;
};
typedef union FourDigits FourDigits;

#ifdef CYARG_FEATURE_TEST_INT
void main(void)
{
    int_run_tests();
}
#endif

void int_run_tests(void)
{
    Int a;
    int_set_i(0, &a);
    int_invariant(&a);
    assert(int_is_zero(&a));

    int_set_i(0, &a);
    a.neg_ = true;
    int_invariant(&a);
    assert(int_is_zero(&a));

    int_set_i(-2147483648, &a);
    int_invariant(&a);
    assert(int_to_i32(&a) == -0x80000000);

    int_set_i(-2130706432, &a);
    int_invariant(&a);
    assert(int_to_i32(&a) == -0x7f000000);

    int_set_i(0x7fffffff, &a);
    int_invariant(&a);
    assert(int_to_i32(&a) == 0x7fffffff);

    int_set_i(0x7effffff, &a);
    int_invariant(&a);
    assert(int_to_i32(&a) == 0x7effffff);

    int_set_i(-9223372036854775808, &a);
    int_invariant(&a);
    assert(int_to_i64(&a) == -0x8000000000000000);

    int_set_i(-9151314442816847872, &a);
    int_invariant(&a);
    assert(int_to_i64(&a) == -0x7f00000000000000);

    int_set_i(0x7fffffffffffffff, &a);
    int_invariant(&a);
    assert(int_to_i64(&a) == 0x7fffffffffffffff);

    int_set_i(0x7effffffffffffff, &a);
    int_invariant(&a);
    assert(int_to_i64(&a) == 0x7effffffffffffff);

    int_set_i(0x180000000, &a);
    int_invariant(&a);
    assert(int_to_i32(&a) != -0x80000000);
    assert(int_to_u32(&a) == 0x80000000);

    Int b;
    int_set_i(123, &a);
    int_set_i(4567, &b);
    int_invariant(&a);
    int_invariant(&b);
    assert(int_is(&a, &b) == INT_LT);
    assert(int_is_abs(&a, &b) == INT_LT);

    int_set_i(9223372036854775807L, &a);
    int_set_i(9223372036854775806ll, &b);
    int_invariant(&a);
    int_invariant(&b);
    assert(int_is(&a, &b) == INT_GT);
    assert(int_is_abs(&a, &b) == INT_GT);

    int_set_i(0ll, &a);
    int_set_i(-1, &b);
    int_invariant(&a);
    int_invariant(&b);
    assert(int_is(&a, &b) == INT_GT);
    assert(int_is_abs(&a, &b) == INT_LT);

    int_set_i(0ll, &b); b.neg_ = true;
    int_invariant(&b);
    assert(int_is(&a, &b) == INT_EQ);
    assert(int_is_abs(&a, &b) == INT_EQ);

    int_set_i(-2, &a);
    int_set_i(-1, &b);
    int_invariant(&a);
    int_invariant(&b);
    assert(int_is(&a, &b) == INT_LT);
    assert(int_is_abs(&a, &b) == INT_GT);

    int_set_i(2, &b);
    int_invariant(&b);
    assert(int_is(&a, &b) == INT_LT);
    assert(int_is_abs(&a, &b) == INT_EQ);

    Int r, t;
    int_init(&r);
    int_set_i(123, &a);
    int_set_i(4567, &b);
    int_invariant(&a);
    int_invariant(&b);
    int_add(&a, &b, &r);
    int_invariant(&r);
    int_set_i(123+4567, &t);
    int_invariant(&t);
    assert(int_is(&r, &t) == INT_EQ);
    assert(int_to_i32(&r) == 4690);
    assert(int_to_u32(&t) == 4690);

    int_set_i(1230000001, &a);
    int_set_i(45675555555, &b);
    int_invariant(&a);
    int_invariant(&b);
    int_add(&a, &b, &r); //buggy
    int_invariant(&r);
    int_set_i(1230000001+45675555555, &t);
    int_invariant(&t);
    assert(int_is(&r, &t) == INT_EQ);
    assert(int_to_i64(&r) == 0xAEBC9FA64);
    assert(int_to_u64(&t) == 0xAEBC9FA64);

    int_set_i(9223372036854775807L, &a);
    int_set_i(9223372036854775806ll, &b);
    int_invariant(&a);
    int_invariant(&b);
    int_add(&a, &b, &r);
    int_invariant(&r);
    int_set_t(&r, &a);
    int_invariant(&a);
    int_set_i(3, &b);
    int_invariant(&b);
    int_add(&a, &b, &r);
    int_invariant(&r);
    assert(int_to_u64(&a) == 0xFFFFFFFFFFFFFFFDu);
    assert(int_to_u64(&r) == 0u);

    int_set_i(4567, &a);
    int_set_i(123, &b);
    int_invariant(&a);
    int_invariant(&b);
    int_sub(&a, &b, &r);
    int_invariant(&r);
    int_set_i(4567-123, &t);
    int_invariant(&t);
    assert(int_is(&r, &t) == INT_EQ);

    int_set_i(1230000001, &a);
    int_set_i(45675555555, &b);
    int_invariant(&a);
    int_invariant(&b);
    int_sub(&a, &b, &r);
    int_invariant(&r);
    int_set_i(45675555555-1230000001, &t);
    int_invariant(&t);
    int_set_i(-1, &a);
    int_set_t(&t, &b);
    int_invariant(&a);
    int_invariant(&b);
    int_mul(&a, &b, &t);
    int_invariant(&t);
    assert(int_is(&r, &t) == INT_EQ);

    int_set_i(1, &a);
    int_set_i(2147483649, &b);
    int_invariant(&a);
    int_invariant(&b);
    int_mul(&a, &b, &r);
    int_invariant(&r);
    assert(int_is(&r, &b) == INT_EQ);

    int_set_i(4567, &a);
    int_set_i(123, &b);
    int_invariant(&a);
    int_invariant(&b);
    int_mul(&a, &b, &r);
    int_invariant(&r);
    int_set_i(4567*123, &t);
    int_invariant(&t);
    assert(int_is(&r, &t) == INT_EQ);

    int_set_i(-4567, &a);
    int_set_i(-123, &b);
    int_invariant(&a);
    int_invariant(&b);
    int_mul(&a, &b, &r);
    int_invariant(&r);
    int_set_i(4567*123, &t);
    int_invariant(&t);
    assert(int_is(&r, &t) == INT_EQ);

    int_set_i(-4567, &a);
    int_set_i(123, &b);
    int_invariant(&a);
    int_invariant(&b);
    int_mul(&a, &b, &r);
    int_invariant(&r);
    int_set_i(-4567*123, &t);
    int_invariant(&t);
    assert(int_is(&r, &t) == INT_EQ);

    int_set_i(1230000001, &a);
    int_set_i(45675, &b);
    int_invariant(&a);
    int_invariant(&b);
    int_mul(&a, &b, &r);
    int_invariant(&r);
    int_set_i(45675*1230000001ll, &t);
    int_invariant(&t);
    assert(int_is(&r, &t) == INT_EQ);
    int_print(&r);printf("\n");

    Int d, q;
    int_init(&q);
    int_set_i(10000u, &d);
    int_div(&t, &d, &q, &r);
    int_invariant(&q);
    int_invariant(&r);
    int_set_i(5618025004, &t);
    assert(int_is(&q, &t) == INT_EQ);
    int_set_i(5675, &t);
    assert(int_is(&r, &t) == INT_EQ);

    int_div(&q, &d, &q, &r);
    int_invariant(&q);
    int_invariant(&r);
    int_set_i(561802, &t);
    assert(int_is(&q, &t) == INT_EQ);
    int_set_i(5004, &t);
    assert(int_is(&r, &t) == INT_EQ);

    int_div(&q, &d, &q, &r);
    int_invariant(&q);
    int_invariant(&r);
    int_set_i(56, &t);
    assert(int_is(&q, &t) == INT_EQ);
    int_set_i(1802, &t);
    assert(int_is(&r, &t) == INT_EQ);

    int_div(&q, &d, &q, &r);
    int_invariant(&q);
    int_invariant(&r);
    int_set_i(0, &t);
    assert(int_is(&q, &t) == INT_EQ);
    int_set_i(56, &t);
    assert(int_is(&r, &t) == INT_EQ);

    int_set_s("-12345", &a);
    int_set_i(-12345, &b);
    int_invariant(&a);
    int_invariant(&b);
    assert(int_is(&a, &b) == INT_EQ);

    int_set_i(13421772800000, &a);
    int_set_i(13421772800000, &b);
    int_invariant(&a);
    int_invariant(&b);
    int_div(&a, &b, &q, 0);
    int_invariant(&q);
    int_set_i(13421772800000/13421772800000, &t);
    int_invariant(&t);
    assert(int_is(&q, &t) == INT_EQ);

    int_set_i(13421772800700, &a);
    int_set_i(444, &b);
    int_invariant(&a);
    int_invariant(&b);
    int_div(&a, &b, &q, 0);
    int_invariant(&q);
    int_set_i(13421772800700/444, &t);
    int_invariant(&t);
    assert(int_is(&q, &t) == INT_EQ);

    Int p;
    int_init(&p);

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
        int_set_i(tm[x][0], &a);
        int_set_i(tm[x][1], &b);
        int_invariant(&a);
        int_invariant(&b);
        int_mul(&a, &b, &p);
        int_invariant(&p);
        int_set_i(tm[x][0] * tm[x][1], &t);
        int_invariant(&t);
        assert(int_is(&p, &t) == INT_EQ);
    }

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
        int_set_s(tms[x][0], &a);
        int_set_s(tms[x][1], &b);
        int_set_s(tms[x][2], &t);
        int_invariant(&a);
        int_invariant(&b);
        int_invariant(&t);
        int_print(&a);printf(" * ");int_print(&b);printf(" - ");int_print(&t);printf("\n");
        int_mul(&a, &b, &p);
        int_invariant(&p);
        int_print(&a);printf(" * ");int_print(&b);printf(" - ");int_print(&p);printf("\n");
        assert(int_is(&p, &t) == INT_EQ);
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
        int_set_i(td[x][0], &a);
        int_set_i(td[x][1], &b);
        int_invariant(&a);
        int_invariant(&b);
        int_div(&a, &b, &q, &r);
        int_invariant(&q);
        int_invariant(&r);
        int_set_i(td[x][0] / td[x][1], &t);
        int_invariant(&t);
        assert(int_is(&q, &t) == INT_EQ);
        int_set_i(td[x][0] % td[x][1], &t);
        int_invariant(&t);
        assert(int_is(&r, &t) == INT_EQ);
    }

    int_set_s("135358547299737016026944784076399913892329979959413632311040351208192617688605777748629200218979012809309902876532501243923094056842496073878356580559806024248007176850258062322719137263368448493733726486018103641504762647590705290961752726334259860457999896434747976461450755454734072964127870749101348010716", &a);
    int_print(&a);printf("\n");

    Int s;
    int_init(&s);

    for (int x = 0; x < 100000; x++)
    {
        int_make_random(&a);int_invariant(&a);
        int_make_random(&b);int_invariant(&b);
        int_add(&a, &b, &s);int_invariant(&s);
        int_print(&a);printf(" + ");int_print(&b);printf(" - ");int_print(&s);printf("\n");

        int_sub(&a, &b, &d);int_invariant(&d);
        int_print(&a);printf(" - ");int_print(&b);printf(" - ");int_print(&d);printf("\n");

        if (a.d_ + b.d_ <= a.m_)
        {
            int_mul(&a, &b, &p);int_invariant(&p);
            int_print(&a);printf(" * ");int_print(&b); printf(" - ");int_print(&p);printf("\n");
        }

        int_div(&a, &b, &q, &r);int_invariant(&q);int_invariant(&r);
        int_print(&a);printf(" / ");int_print(&b);printf(" - ");int_print(&q);printf("\n");
        int_print(&a);printf(" %% ");int_print(&b);printf(" - ");int_print(&r);printf("\n");

        int_div(&a, &b, &q, 0);int_invariant(&q);
        int_print(&a);printf(" / ");int_print(&b);printf(" - ");int_print(&q);printf("\n");
    }
}



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

void addPos(Int const *, Int const *, Int *);
void int_sub_abs(Int const *, Int const *, Int *);
static void subAGtB(Int const *, Int const *, Int *);

void int_invariant(Int const *i)
{
    assert(i->neg_ == 1 || i->neg_ == 0);
    assert(i->m_ == INT_MAX_DIGITS); // todo - this will change when arbitrary length
    assert(i->d_ >= 1 && i->d_ <= i->m_);
    // check top half-word is zero if length is odd
    if (i->d_ % 2 == 1)
    {
        assert(i->h_[i->d_] == 0);
    }
    // check no leading zeros
    if (i->d_ > 1)
    {
        assert(i->h_[i->d_ - 1] != 0);
    }
}

void int_init(Int *i)
{
    i->neg_ = false;
    i->d_ = 1;
    i->m_ = INT_MAX_DIGITS;
    i->w_[0] = 0;
}

void int_make_random(Int *i)
{
    i->neg_ = rand() > RAND_MAX / 2;
    i->d_ = 1 + rand() / (RAND_MAX / 64);
    assert(i->d_ <= INT_MAX_DIGITS);
    i->m_ = INT_MAX_DIGITS;
    for (int x = 0; x < i->d_; x++)
    {
        i->h_[x] = (uint16_t) rand();
    }
    while (i->h_[i->d_ - 1] == 0)
    {
        i->h_[i->d_ - 1] = (uint16_t) rand();
    }
    if (i->d_ < 64)
    {
        i->h_[i->d_] = 0u;
    }
}
#include <limits.h>
void int_set_i(int64_t to, Int *i)
{
    i->m_ = INT_MAX_DIGITS;

    uint64_t v;
    i->neg_ = to < 0;
    if (i->neg_)
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
    uint32_t *ip = i->w_;
    for (; ip < &i->w_[i->m_ / 2] && v > 0;)
    {
        *ip++ = (uint32_t) v;
        v /= 4294967296u;
    }
    i->d_ = (uint8_t)((ip - i->w_) * 2);
    if (i->d_ == 0)
    {
        i->w_[0] = 0u;
        i->d_ = 1;
    }
    else
    {
        if (i->h_[i->d_ - 1] == 0u) // overshot
        {
            i->d_--;
        }
    }
}

void int_set_s(char const *s, Int *i)
{
    int_init(i);
    
    Int ten, acc;
    int_set_i(10, &ten);
    int_init(&acc);

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
        int_mul(&ten, i, &acc);
        int d = *s++ - '0';
        Int digit;
        int_set_i(d, &digit);
        int_add(&acc, &digit, i);
    }
    i->neg_ = neg;
}

void int_set_t(Int const *v, Int *i)
{
    i->neg_ = v->neg_;
    i->d_ = v->d_;
    i->m_ = INT_MAX_DIGITS;
    assert(i->m_ == v->m_);
    memcpy(i->h_, v->h_, (v->d_ + 1) / 2 * sizeof v->w_[0]);
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
    uint16_t *rp = r->h_;
    uint16_t const *bp = b->h_;
    for (uint16_t const *ap = a->h_; ap < &a->h_[a->d_] || bp < &b->h_[b->d_]; ap++, bp++)
    {
        TwoDigits ps;
        if (ap >= &a->h_[a->d_])
        {
            ps.u32_ = (uint32_t) *bp + (uint32_t) carry;
        }
        else if (bp >= &b->h_[b->d_])
        {
            ps.u32_ = (uint32_t) *ap + (uint32_t) carry;
        }
        else
        {
            ps.u32_ = (uint32_t) *ap + (uint32_t) *bp + (uint32_t) carry;
        }
        *rp++ = ps.l16_;
        carry = ps.h16_;
    }
    if (rp < &r->h_[r->m_])
    {
        *rp = carry; // only needs to be done (if carry is zero) of odd number of digits but saves a test
        if (carry > 0)
        {
            rp++;
            if (rp < &r->h_[r->m_]) // only needs to be done of odd number of digits but saves a test
            {
                *rp = 0;
            }
        }
    }
    r->d_ = (uint8_t)(rp - r->h_);
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
    if (shift >= 0)
    {
        assert(i->d_ + shift <= i->m_);

        memmove(&i->h_[shift], &i->h_[0], i->d_ * sizeof (uint16_t));
        memset(&i->h_[0], 0, shift * sizeof (uint16_t));
        i->d_ += shift;
        if (i->d_ < i->m_)
        {
            i->h_[i->d_] = 0; // only need to do if rd.d_ + shift is odd - but why not
        }
    }
    else
    {
        assert(i->d_ + shift >= 0);

        memmove(&i->h_[0], &i->h_[-shift], ((int)i->d_ + shift) * sizeof (uint16_t));
        memset(&i->h_[i->d_ + shift], 0, -shift * sizeof (uint16_t));
        i->d_ += shift;
        if (i->d_ == 0)
        {
            i->d_ = 1;
            assert(i->h_[0] == 0u);
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
    uint16_t *rp = r->h_;
    uint16_t const *bp = b->h_;
    for (uint16_t const *ap = a->h_; ap < &a->h_[a->d_]; ap++, bp++)
    {
        uint16_t ad = *ap, bd;
        if (bp < &b->h_[b->d_])
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
            pd.u32_ = 0x10000u + ad - pd.u32_;
        }
        else
        {
            borrow = 0;
            pd.u32_ = ad - pd.u32_;
        }
        *rp++ = pd.l16_;
    }
    r->d_ = (uint8_t)(rp - r->h_);
    if (r->d_ % 2 == 1)
    {
        r->h_[r->d_] = 0; // todo optimize - don’t need to do this if the following is true
    }
    while (r->d_ > 1 && r->h_[r->d_ - 1] == 0)
    {
        r->d_--;
    }
}

void int_mul(Int const *a, Int const *b, Int *r)
{
    int_set_i(0, r);
    if (a->d_ == 1 && a->h_[0] == 0 || b->d_ == 1 && b->h_[0] == 0)
    {
        return;
    }

    uint16_t *rpn = r->h_;
    for (uint16_t const *ap = a->h_; ap < &a->h_[a->d_]; ap++)
    {
        uint16_t *rp = rpn;
        rpn = rp + 1;
//        printf("%ld->%ld", ap - a->h_, rp - r->h_);
        for (uint16_t const *bp = b->h_; bp < &b->h_[b->d_]; bp++, rp++)
        {
//            printf(".%ld/%ld", ap - a->h_, bp - b->h_);
            Int pp;
            int_set_i((uint32_t) *ap * (uint32_t) *bp, &pp);
            if (rp - r->h_ + pp.d_ > r->m_)
            {
                goto overflow;
            }
            int_shift((int)(rp - r->h_), &pp);
//            printf("%d:%d := %d %d\n", (int)pp.h16_, (int)pp.l16_, (int)*ap, (int)*bp);
            int_add(r, &pp, r);
        }
    }
overflow:
    r->neg_ = a->neg_ ^ b->neg_;
}

/*
        1774_______
 473046)839264859 => /5 as first attempt
        473046 *1
        ------
        366218859 >D Continue
         3311322 *7
         ------
         35086659 >D Continue
          3311322 *7
          ------
          1973439 >D Continue
          1419138 *3
          =======
           554301 >D Continue
           473046 *1
          =======
            81255 <D Terminate
 */
void int_div(Int const *n, Int const *d, Int *i, Int *r)
{
    bool nSign = n->neg_;
    bool dSign = d->neg_;
    
    if (int_is_abs(n, d) == INT_LT)
    {
        if (r != 0)
        {
            int_set_t(n, r);
        }
        int_set_i(0ll, i);
        i->neg_ = nSign ^ dSign;
        return;
    }

    Int qAccumulator;
    int_init(&qAccumulator);
    Int qDigit;
    int_init(&qDigit);
    Int reduceBy;
    int_init(&reduceBy);

    uint16_t testDenominator = d->h_[d->d_ - 1]; // todo what to do if this is 65535+1 (make it 1, 65535? or use uint32?)
    assert(testDenominator != 0 && testDenominator != 65535); // divide by zero
    testDenominator++;

    Int reducingNumerator;
    int_set_t(n, &reducingNumerator);
    reducingNumerator.neg_ = false;
    Int shiftedDenominator;
    int_set_t(d, &shiftedDenominator);
    int shift = n->d_ - d->d_;
    int_shift(shift, &shiftedDenominator);
    shiftedDenominator.neg_ = false;

    int nDigit = reducingNumerator.d_ - 1;

    while (1)
    {
        if (int_is_abs(&reducingNumerator, d) == INT_LT)
        {
            break;
        }
        while (int_is(&shiftedDenominator, &reducingNumerator) == INT_GT)
        {
            nDigit--;
            int_shift(-1, &shiftedDenominator);
        }
        TwoDigits testNumerator;
        testNumerator.l16_ = reducingNumerator.h_[nDigit];
        testNumerator.h16_ = nDigit == reducingNumerator.d_ - 1 ? 0u : reducingNumerator.h_[nDigit + 1];
        uint32_t testMultiplier = testNumerator.u32_ / testDenominator;
        if (testMultiplier == 0u)
        {
            testMultiplier = 1u;
        }

        int_set_i(testMultiplier, &qDigit);
        int_mul(&shiftedDenominator, &qDigit, &reduceBy);
        int_shift(nDigit, &qDigit);
        int_add(&qAccumulator, &qDigit, &qAccumulator);
        int_sub(&reducingNumerator, &reduceBy, &reducingNumerator);
        assert(!reducingNumerator.neg_);
    }
    qAccumulator.neg_ = nSign ^ dSign;
    int_set_t(&qAccumulator, i); // todo optimise - no need for qAccumulator really
    int_shift(-nDigit, i);
    if (r != 0)
    {
        int_set_t(&reducingNumerator, r);
        r->neg_ = nSign;
    }
}

void int_neg(Int *i)
{
    i->neg_ = !i->neg_;
}

bool int_is_zero(Int const *i)
{
    for (uint32_t const *ip = &i->w_[0]; ip < &i->w_[(i->d_ + 1) / 2]; ip++)
    {
        if (*ip != 0u)
        {
            return false;
        }
    }
    return true; // is +/- zero
}

IntComp int_is(Int const *a, Int const *b)
{
    IntComp r;
    if (a->neg_ != b->neg_) // check for zero and -ve zero
    {
        if (a->d_ == 1 && b->d_ == 1 && a->h_[0] == 0 && b->h_[0]  == 0)
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

char const *int_to_s(Int const *i, char *s, int n)
{
    char *out = &s[n - 1];
    *out = '\0';

    Int v;
    int_set_t(i, &v);
    Int tenToTheFour;
    int_set_i(10000u, &tenToTheFour);

    while (!int_is_zero(&v))
    {
        Int r;
        int_div(&v, &tenToTheFour, &v, &r);

        assert(r.d_ == 1);
        u_int16_t rem = r.h_[0];
        bool leading = int_is_zero(&v);
        for (int c = 0; c < 4; c++)
        {
            char ch = (char) (rem % 10u + '0');
            if (!leading || rem != 0)
            {
                *--out = ch;
            }
            rem /= 10u;
        }
    }
    if (out == &s[310]) // todo optimise - break to for loop above if rem and v are zero
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
    uint16_t const *s = i->h_;
    uint16_t const *e = min(&i->h_[4], &i->h_[i->d_]);
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
    uint16_t const *s = i->h_;
    uint16_t const *e = min(&i->h_[4], &i->h_[i->d_]);
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
    uint16_t const *s = i->h_;
    uint16_t const *e = min(&i->h_[2], &i->h_[i->d_]);
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
    uint16_t const *s = i->h_;
    uint16_t const *e = min(&i->h_[2], &i->h_[i->d_]);
    uint16_t *d = &r.l16_;
    while (s < e)
    {
        *d++ = *s++;
    }
    return r.u32_;
}

void int_print(Int const *i)
{
    char s[311];
    const char *out = int_to_s(i, s, 311);
    printf("%s", out);
}

void int_for_bc(Int const *a)
{
    for (int i = 0; i < a->d_; i++)
    {
        printf("%d", a->h_[i]);
        for (int j = 0; j < i; j++)
            printf("*65536");
        if (i < a->d_ - 1)
            printf("+");
    }
    printf("\n");
}
