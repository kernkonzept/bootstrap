#pragma once

#include <features.h>
#include <cdefs.h>

__BEGIN_DECLS

_Noreturn
void __assert(const char *__assertion, const char *__file,
              unsigned int __line, const char *func)
     __attribute__ ((__noreturn__));

__END_DECLS

#define ASSERT_EXPECT_FALSE(exp)  __builtin_expect((exp), 0)

#undef assert
#ifdef NDEBUG
#define assert(expr) do {} while (0)
#define check(expr) (void)(expr)
#else
# define assert(expr)						\
  ((void)((ASSERT_EXPECT_FALSE(!(expr)))			\
	  ? (__assert(#expr, __FILE__, __LINE__, __func__), 0)	\
	  : 0))
# define check(expr) assert(expr)
#endif
