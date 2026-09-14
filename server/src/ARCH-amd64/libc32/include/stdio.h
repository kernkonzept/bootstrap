#ifndef _STDIO_H
#define _STDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <features.h>

#define __NEED_FILE
#define __NEED___isoc_va_list
#define __NEED_size_t

#define __NEED_ssize_t
#define __NEED_off_t
#define __NEED_va_list

#include <bits/alltypes.h>

#if defined(__cplusplus)
#define NULL nullptr
#else
#define NULL ((void*)0)
#endif

#define EOF (-1)

int getchar(void);

int putchar(int);
int puts(const char *);
int printf(const char *__restrict, ...);
int sprintf(char *__restrict, const char *__restrict, ...);
int snprintf(char *__restrict, size_t, const char *__restrict, ...);

int vprintf(const char *__restrict, __isoc_va_list);
int vsprintf(char *__restrict, const char *__restrict, __isoc_va_list);
int vsnprintf(char *__restrict, size_t, const char *__restrict, __isoc_va_list);

#ifdef __cplusplus
}
#endif

#endif
