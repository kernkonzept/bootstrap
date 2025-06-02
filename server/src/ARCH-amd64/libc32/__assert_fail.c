#include "assert.h"
#include "stdio.h"
#include "stdlib.h"

void
__assert(const char *__assertion, const char *__file,
         unsigned int __line, const char *__func)
{
  printf("ASSERTION_FAILED (%s)\n"
	 "  in file %s:%d(%s)\n", __assertion, __file, __line, __func);
  _Exit (EXIT_FAILURE);
}
