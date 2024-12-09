#include "libc_stdio.h"
#include "libc_backend.h"
#include <string.h>

int puts(const char *s)
{
  int ret = __libc_backend_outs(s, strlen(s));
  if (ret >= 0)
    {
      char c = '\n';
      ret = __libc_backend_outs(&c, 1);
    }
  return ret;
}
