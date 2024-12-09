PKGDIR   ?= ../../../..
L4DIR    ?= $(PKGDIR)/../..

TARGET    = libc32.a

SYSTEMS   = amd64-plain
VARIANTS  = nofpu

# Not needed. This is not for general consumption
NOTARGETSTOINSTALL = y

SRC_C = __assert_fail.c \
        glue/stdout_write.c \
        glue/printf.c \
        glue/puts.c \
        glue/vprintf.c \
        src/string/strlen.c \
        src/string/strnlen.c \
        src/string/memchr.c \
        src/string/memmove.c \
        src/string/memset.c \
        src/stdio/__towrite.c \
        src/stdio/fputs.c \
        src/stdio/fwrite.c \
        src/stdio/vfprintf.c \
        src/ctype/isdigit.c
SRC_S = src/string/i386/memcpy.S

include $(L4DIR)/mk/lib.mk

CFLAGS	 := $(filter-out $(CCXX_FLAGS) $(CARCHFLAGS), $(CFLAGS)) \
            -mno-sse -fno-builtin -ffreestanding
ASFLAGS  := $(call checkcc,-Wa$(BID_COMMA)--32)
CPPFLAGS := -DLIBCL4 \
            -m32 \
            -nostdinc \
            -I$(SRC_DIR)/src/internal \
            -I$(SRC_DIR)/src/include \
            -I$(SRC_DIR)/include
OPTS     := -Os
