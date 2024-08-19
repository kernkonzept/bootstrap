/* This file defines the backend interface */
/* of the kernel c-library. */
#pragma once
#include <stddef.h>
#include <cdefs.h>

__BEGIN_DECLS

/**
 * The text output backend.
 *
 * This function must be provided to the c-library for
 * text output. It must simply send len characters of s
 * to an output device.
 *
 * @param s the string to send (not zero terminated).
 * @param len the number of characters.
 * @return 1 on success, 0 else.
 */
int __libc_backend_outs(const char *s, size_t len);

__END_DECLS
