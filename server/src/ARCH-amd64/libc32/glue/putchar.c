#include <stdio.h>
#include "libc_backend.h"

int putchar (int c) 
{
  char _c = c;
  int ret = __libc_backend_outs(&_c,1) ? c : 0;
  return ret;
}
