#ifndef _FEATURES_H
#define _FEATURES_H

#if !defined(__cplusplus)
#define __restrict restrict
#endif // c++

#define __inline inline

#if defined(__cplusplus)
#define _Noreturn [[noreturn]]
#endif

#endif
