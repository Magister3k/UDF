/*
**
** File:        "typedef2.h"
**
*/
/*
	ITU-T G.723.1 Software Package Release 2 (June 2006)

    ITU-T G.723.1 Floating Point Speech Coder ANSI C Source Code.  Version 5.1F

    Original fixed-point code copyright (c) 1995,
    AudioCodes, DSP Group, France Telecom, Universite de Sherbrooke.
    All rights reserved.

    Floating-point code copyright (c) 1995,
    Intel Corporation and France Telecom (CNET).
    All rights reserved.
*/

/*
   Types definitions - Modernized with C11 stdint.h
*/

#ifndef TYPEDEF2_H
#define TYPEDEF2_H

#include <stdint.h>

/* Exact-width integer types (C99/C11) */
typedef int16_t  Word16;
typedef int32_t  Word32;
typedef int16_t  Flag;

/* Floating point type */
#if defined(_single) || defined(single)
typedef float FLOAT;
#else
typedef double FLOAT;
#endif

/* Boolean-like constants */
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

/* Static assertions for type sizes (C11) */
#if __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(Word16) == 2, "Word16 must be 16 bits");
_Static_assert(sizeof(Word32) == 4, "Word32 must be 32 bits");
_Static_assert(sizeof(Flag) == 2, "Flag must be 16 bits");
#endif

/* Restrict qualifier for C99+ */
#if __STDC_VERSION__ >= 199901L
#define RESTRICT restrict
#else
#define RESTRICT
#endif

/* Inline keyword for C99+ */
#if __STDC_VERSION__ >= 199901L
#define INLINE inline
#else
#define INLINE
#endif

/* Likely/unlikely branch hints */
#if defined(__GNUC__) || defined(__clang__)
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x)   (x)
#define UNLIKELY(x) (x)
#endif

#endif /* TYPEDEF2_H */