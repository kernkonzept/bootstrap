#ifndef _STDLIB_H
#define _STDLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <features.h>

#if defined(__cplusplus)
#define NULL nullptr
#else
#define NULL ((void*)0)
#endif

#define __NEED_size_t

#include <bits/alltypes.h>

long strtol (const char *__restrict, char **__restrict, int);
unsigned long strtoul (const char *__restrict, char **__restrict, int);
long long strtoll (const char *__restrict, char **__restrict, int);
unsigned long long strtoull (const char *__restrict, char **__restrict, int);

_Noreturn void _Exit (int);

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

#ifdef __cplusplus
}
#endif

#endif
