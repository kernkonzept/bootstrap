
#include <h/types.h>
#include <h/os.h>
#include <h/hypervisor.h>
#include <public/xen.h>

asm(".section __xen_guest                                \n"
    ".ascii \"GUEST_OS=DD-L4\"                           \n"
    ".ascii \",GUEST_VER=0.1\"                           \n"
    ".ascii \",XEN_VER=2.0\"                             \n"
    ".ascii \",VIRT_BASE=0xf0000000\"                    \n"
    ".ascii \",LOADER=generic\"                          \n"
    ".ascii \",PT_MODE_WRITABLE\"                        \n"
    ".previous                                           \n");

int
putchar(int c)
{
  HYPERVISOR_console_io(CONSOLEIO_write, 1, (char *)&c);
  return c;
}
