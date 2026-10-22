/*
 * Workaround for the C99 math declarations missing from MeeGo Harmattan's
 * eglibc 2.10 headers.
 *
 * ARM has no distinct `long double' type, so glibc's <bits/mathdef.h> defines
 * __NO_LONG_DOUBLE_MATH and <math.h> then skips the whole block of long double
 * variants of the C99 math functions. libstdc++ 14, when configured against
 * those headers, concludes that the C99 math functions are not available and
 * leaves every `using ::foo;' out of <cmath>. glm and the emulator then fail
 * with errors such as "'log2' is not a member of 'std'".
 *
 * The functions still exist in the device's libm (they are aliases of the
 * double versions), so declaring them here and telling libstdc++ that the C99
 * math functions are present (see _GLIBCXX_USE_C99_MATH_FUNCS in the toolchain
 * file) is enough to get a complete <cmath>.
 *
 * The toolchain file force-includes this header before any standard library
 * header, so only declarations may be added here.
 */

#ifndef EKA2L1_HARMATTAN_MATH_COMPAT_H
#define EKA2L1_HARMATTAN_MATH_COMPAT_H

#if defined(__HARMATTAN__) && defined(__cplusplus)

extern "C" {

/* The C99 long double variants whose return type is not `long double'. */
int ilogbl(long double __x);
long int lrintl(long double __x);
long long int llrintl(long double __x);
long int lroundl(long double __x);
long long int llroundl(long double __x);

/* The C99 long double variants returning `long double'. */
long double acoshl(long double __x);
long double asinhl(long double __x);
long double atanhl(long double __x);
long double cbrtl(long double __x);
long double copysignl(long double __x, long double __y);
long double erfl(long double __x);
long double erfcl(long double __x);
long double exp2l(long double __x);
long double expm1l(long double __x);
long double fdiml(long double __x, long double __y);
long double fmal(long double __x, long double __y, long double __z);
long double fmaxl(long double __x, long double __y);
long double fminl(long double __x, long double __y);
long double hypotl(long double __x, long double __y);
long double lgammal(long double __x);
long double log1pl(long double __x);
long double log2l(long double __x);
long double logbl(long double __x);
long double nanl(const char *__tagb);
long double nearbyintl(long double __x);
long double nextafterl(long double __x, long double __y);
long double nexttowardl(long double __x, long double __y);
long double remainderl(long double __x, long double __y);
long double remquol(long double __x, long double __y, int *__quo);
long double rintl(long double __x);
long double roundl(long double __x);
long double scalblnl(long double __x, long int __n);
long double scalbnl(long double __x, int __n);
long double tgammal(long double __x);
long double truncl(long double __x);

} // extern "C"

#endif // __HARMATTAN__ && __cplusplus

#endif // EKA2L1_HARMATTAN_MATH_COMPAT_H
