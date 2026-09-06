/*
 * Fixed-point math utilities for G.723.1
 * Based on FFmpeg's optimizations (libavcodec/celp_math.h)
 * Provides efficient integer arithmetic alternatives to floating-point
 */

#ifndef G723_MATH_H
#define G723_MATH_H

#include "typedef2.h"
#include <stdint.h>

/* Fixed-point format: Q15 (1.15) for coefficients, Q31 for accumulation */

/* Saturating arithmetic helpers */
static INLINE int16_t sat_add16(int16_t a, int16_t b)
{
    int32_t sum = (int32_t)a + (int32_t)b;
    if (sum > 32767) return 32767;
    if (sum < -32768) return -32768;
    return (int16_t)sum;
}

static INLINE int16_t sat_sub16(int16_t a, int16_t b)
{
    int32_t diff = (int32_t)a - (int32_t)b;
    if (diff > 32767) return 32767;
    if (diff < -32768) return -32768;
    return (int16_t)diff;
}

static INLINE int16_t sat_mul16(int16_t a, int16_t b)
{
    int32_t prod = (int32_t)a * (int32_t)b;
    if (prod > 32767) return 32767;
    if (prod < -32768) return -32768;
    return (int16_t)prod;
}

/* Q15 multiplication with rounding: (a * b) >> 15 */
static INLINE int16_t mul_q15(int16_t a, int16_t b)
{
    int32_t prod = (int32_t)a * (int32_t)b;
    return (int16_t)((prod + 16384) >> 15);
}

/* Q15 multiplication with 32-bit accumulator: (a * b) << 1 */
static INLINE int32_t mul_q15_32(int16_t a, int16_t b)
{
    return (int32_t)a * (int32_t)b << 1;
}

/* Saturating 32-bit to 16-bit */
static INLINE int16_t sat_32_to_16(int32_t x)
{
    if (x > 32767) return 32767;
    if (x < -32768) return -32768;
    return (int16_t)x;
}

/* Normalize 16-bit value: return number of leading zeros (0-15) */
static INLINE int16_t normalize16(int16_t x)
{
    if (x == 0) return 15;
    if (x < 0) x = (int16_t)~x;
    int16_t n = 0;
    while ((x & 0x4000) == 0) {
        x <<= 1;
        n++;
    }
    return n;
}

/* Normalize 32-bit value: return number of leading zeros (0-31) */
static INLINE int16_t normalize32(int32_t x)
{
    if (x == 0) return 31;
    if (x < 0) x = ~x;
    int16_t n = 0;
    while ((x & 0x40000000) == 0) {
        x <<= 1;
        n++;
    }
    return n;
}

/* Fast integer square root (approximation) */
static INLINE int16_t isqrt16(uint16_t x)
{
    int16_t res = 0;
    int16_t bit = 0x4000;
    while (bit != 0) {
        int16_t trial = res + bit;
        if ((uint32_t)trial * (uint32_t)trial <= x)
            res = trial;
        bit >>= 1;
    }
    return res;
}

/* Fast integer square root for 32-bit */
static INLINE int16_t isqrt32(uint32_t x)
{
    int32_t res = 0;
    int32_t bit = 0x40000000;
    while (bit != 0) {
        int32_t trial = res + bit;
        if ((uint64_t)trial * (uint64_t)trial <= x)
            res = trial;
        bit >>= 1;
    }
    return (int16_t)res;
}

/* Dot product with 32-bit accumulation (Q15 * Q15 -> Q31) */
static INLINE int32_t dot_product_q15(const int16_t *a, const int16_t *b, int len)
{
    int32_t sum = 0;
    for (int i = 0; i < len; i++)
        sum += (int32_t)a[i] * (int32_t)b[i];
    return sum;
}

/* Scale vector to prevent overflow: find max, compute scale, apply */
static INLINE void scale_vector_q15(int16_t *vec, int len)
{
    int16_t max_val = 0;
    for (int i = 0; i < len; i++) {
        int16_t abs_val = vec[i] >= 0 ? vec[i] : (int16_t)-vec[i];
        if (abs_val > max_val) max_val = abs_val;
    }
    
    if (max_val == 0) return;
    
    int16_t shifts = normalize16(max_val);
    if (shifts == 0) return;
    
    for (int i = 0; i < len; i++) {
        int32_t val = (int32_t)vec[i] << shifts;
        vec[i] = sat_32_to_16(val);
    }
}

/* LPC synthesis filter (Q12 coefficients) */
static INLINE void lpc_synthesis_q12(const int16_t *lpc, const int16_t *exc, int16_t *out, int len, int order)
{
    for (int i = 0; i < len; i++) {
        int32_t acc = (int32_t)exc[i] << 12;
        for (int j = 1; j <= order; j++) {
            if (i >= j)
                acc -= mul_q15_32(lpc[j-1], out[i-j]);
        }
        out[i] = sat_32_to_16(acc);
    }
}

/* Postfilter: formant (Q15 coefficients) */
static INLINE void formant_postfilter_q15(const int16_t *zero, const int16_t *pole,
                                          int16_t *buf, int len, int order)
{
    for (int i = 0; i < len; i++) {
        int32_t num = (int32_t)buf[i] << 15;
        int32_t den = (int32_t)buf[i] << 15;
        
        for (int j = 1; j <= order; j++) {
            if (i >= j) {
                num -= mul_q15_32(zero[j-1], buf[i-j]);
                den -= mul_q15_32(pole[j-1], buf[i-j]);
            }
        }
        if (den != 0)
            buf[i] = sat_32_to_16((num << 1) / den);
    }
}

/* Gain scaling for postfilter output */
static INLINE void gain_scale_q15(int16_t *buf, int len, int16_t target_energy)
{
    int32_t energy = 0;
    for (int i = 0; i < len; i++)
        energy += (int32_t)buf[i] * (int32_t)buf[i];
    
    if (energy == 0) return;
    
    /* Compute scaling factor: sqrt(target_energy / energy) */
    int32_t scale_num = (int32_t)target_energy * len;
    int16_t scale = (int16_t)((int64_t)scale_num * 32768 / isqrt32(energy));
    
    for (int i = 0; i < len; i++) {
        int32_t val = (int32_t)buf[i] * scale;
        buf[i] = sat_32_to_16(val >> 15);
    }
}

#endif /* G723_MATH_H */