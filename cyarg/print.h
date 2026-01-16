#ifndef cyarg_print_h
#define cyarg_print_h

#include <stdio.h>

#ifdef CYARG_PICO_STDLIB

#define PRINTERR(...) printf(__VA_ARGS__)
#define FPRINTMSG(STREAM_, ...) printf(__VA_ARGS__)

#else


#define PRINTERR(...) fprintf(stderr, __VA_ARGS__)
#define FPRINTMSG(STREAM_, ...) fprintf(STREAM_, __VA_ARGS__)

#endif

#endif
