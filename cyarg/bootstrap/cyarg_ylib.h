#ifndef cyarg_bootstrap_cyarg_ylib_h
#define cyarg_bootstrap_cyarg_ylib_h

#ifdef CYARG_OS_HOSTED
extern const unsigned char cyarg_hosted_ylib[];
extern const unsigned int cyarg_hosted_ylib_len;
#else
extern const unsigned char cyarg_ylib[];
extern const unsigned int cyarg_ylib_len;
#endif

#endif
