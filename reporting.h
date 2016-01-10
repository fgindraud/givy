#ifndef REPORTING_H
#define REPORTING_H

#include <cstdio>
#include <cstdlib>

/* This file defines error and debug reporting primitives
 */

/* --------------------------------- Assertions ------------------------------------
 *
 * Defines 3 types of ASSERT_* () macros:
 * ASSERT_OPT (): should be kept even in optimized mode (negligible cost)
 * ASSERT_STD (): standard level of assert (small cost)
 * ASSERT_SAFE (): only present in debug mode (high cost)
 *
 * Similarly, ASSERT_*_FAIL(text) can be used to fail unconditionally.
 *
 * Every one of these macro is not guaranteed to be run ; the condition should not have any side
 * effect. They are only here to detect *some* cases of undefined behavior.
 */

/* Macro enabling tests:
 * ASSERT_* expansion can be controled by setting ASSERT_LEVEL_* macros.
 * ASSERT_LEVEL_* can be SAFE, STD, OPT, DISABLED.
 * OPT only expands ASSERT_OPT ; STD expands OPT and STD ; SAFE expands the 3 ASSERT types.
 * DISABLED epxands nothing (no assert code at all).
 * If multiple ASSERT_LEVEL_* are set, the highest in the DISABLED > SAFE > STD > OPT wins.
 */
#if !(defined(ASSERT_LEVEL_SAFE) || defined(ASSERT_LEVEL_STD) || defined(ASSERT_LEVEL_OPT) ||      \
      defined(ASSERT_LEVEL_DISABLED))
#define ASSERT_LEVEL_STD
#endif

#if defined(ASSERT_LEVEL_SAFE) && !defined(ASSERT_LEVEL_DISABLED)
#define ASSERT_SAFE_ENABLED
#endif

#if (defined(ASSERT_LEVEL_SAFE) || defined(ASSERT_LEVEL_STD)) && !defined(ASSERT_LEVEL_DISABLED)
#define ASSERT_STD_ENABLED
#endif

#if (defined(ASSERT_LEVEL_SAFE) || defined(ASSERT_LEVEL_STD) || defined(ASSERT_LEVEL_OPT)) &&      \
    !defined(ASSERT_LEVEL_DISABLED)
#define ASSERT_OPT_ENABLED
#endif

/* Internal ASSERT macro, and conditionnal definition of ASSERT_* macros
 *
 * ASSERT_INTERNAL is valid in constexpr context (in g++/clang++ at least).
 * clang and gcc allow calls to some non-constexpr functions (like printf/abort) in constexpr
 * context, as long at they are not in the control path taken during constexpr evaluation.
 * In ASSERT_INTERNAL, printf/abort are only called if the assert fail, which means that on
 * constexpr context assert failure, the error will be some kind of "you should not call this non
 * constexpr function in constexpr evaluation", instead of a proper assert failure message.
 * ASSERT_FAIL is a macro and uses printf because g++ disallow custom non-constexpr functions and
 * stderr.
 */
#define ASSERT_FAIL(text)                                                                          \
	do {                                                                                             \
		std::printf ("[file=%s][line=%d] Assert '%s' failed\n", __FILE__, __LINE__, text);             \
		std::abort ();                                                                                 \
	} while (false)

#define ASSERT_INTERNAL(cond)                                                                      \
	do {                                                                                             \
		if (!(cond)) {                                                                                 \
			ASSERT_FAIL (#cond);                                                                         \
		}                                                                                              \
	} while (false)

#ifdef ASSERT_SAFE_ENABLED
#define ASSERT_SAFE(cond) ASSERT_INTERNAL (cond)
#define ASSERT_SAFE_FAIL(text) ASSERT_FAIL (text)
#else
#define ASSERT_SAFE(cond)
#define ASSERT_SAFE_FAIL(text)
#endif

#ifdef ASSERT_STD_ENABLED
#define ASSERT_STD(cond) ASSERT_INTERNAL (cond)
#define ASSERT_STD_FAIL(text) ASSERT_FAIL (text)
#else
#define ASSERT_STD(cond)
#define ASSERT_STD_FAIL(text)
#endif

#ifdef ASSERT_OPT_ENABLED
#define ASSERT_OPT(cond) ASSERT_INTERNAL (cond)
#define ASSERT_OPT_FAIL(text) ASSERT_FAIL (text)
#else
#define ASSERT_OPT(cond)
#define ASSERT_OPT_FAIL(text)
#endif

/* ------------------------ Debug messages ----------------------------
 *
 * Only enable debug messages if we are in debug (safe) build.
 */
#ifdef ASSERT_SAFE_ENABLED
#define DEBUG_TEXT(...) std::fprintf (::stderr, __VA_ARGS__)
#else
#define DEBUG_TEXT(...)
#endif

#ifdef ASSERT_STD_ENABLED
#define INFO_TEXT(...) std::fprintf (::stderr, __VA_ARGS__)
#else
#define INFO_TEXT(...)
#endif

/* ------------------------ 'nice' failure ----------------------------
 *
 * Abort nicely (using exit()).
 */
#ifdef ASSERT_STD_ENABLED
#define FAILURE(str, ...)                                                                          \
	do {                                                                                             \
		std::fprintf (::stderr, "[file=%s][line=%d] " str "\n", __FILE__, __LINE__, ##__VA_ARGS__);    \
		std::exit (EXIT_FAILURE);                                                                      \
	} while (false)
#else
#define FAILURE(str, ...) std::exit (EXIT_FAILURE)
#endif

#endif
