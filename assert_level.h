#ifndef ASSERT_LEVEL_H
#define ASSERT_LEVEL_H

/* Defines 3 types of ASSERT_* () macros:
 * ASSERT_OPT (): should be kept even in optimized mode (negligible cost)
 * ASSERT_STD (): standard level of assert (small cost)
 * ASSERT_SAFE (): only present in debug mode (high cost)
 *
 * Every one of these macro is not guaranteed to be run ; the condition should not have any side effect.
 */

/* Macro enabling tests:
 * ASSERT_* expansion can be controled by setting ASSERT_LEVEL_* macros.
 * ASSERT_LEVEL_* can be SAFE, STD, OPT, DISABLED.
 * OPT only expands ASSERT_OPT ; STD expands OPT and STD ; SAFE expands the 3 ASSERT types.
 * DISABLED epxands nothing (no assert code at all).
 * If multiple ASSERT_LEVEL_* are set, the highest in the DISABLED > SAFE > STD > OPT wins.
 */
#if !(defined(ASSERT_LEVEL_SAFE) || defined(ASSERT_LEVEL_STD) || defined(ASSERT_LEVEL_OPT) ||                          \
      defined(ASSERT_LEVEL_DISABLED))
#define ASSERT_LEVEL_STD
#endif

#if defined(ASSERT_LEVEL_SAFE) && !defined(ASSERT_LEVEL_DISABLED)
#define ASSERT_SAFE_ENABLED
#endif

#if (defined(ASSERT_LEVEL_SAFE) || defined(ASSERT_LEVEL_STD)) && !defined(ASSERT_LEVEL_DISABLED)
#define ASSERT_STD_ENABLED
#endif

#if (defined(ASSERT_LEVEL_SAFE) || defined(ASSERT_LEVEL_STD) || defined(ASSERT_LEVEL_OPT)) &&                          \
    !defined(ASSERT_LEVEL_DISABLED)
#define ASSERT_OPT_ENABLED
#endif

/* Internal ASSERT macro, and conditionnal definition of ASSERT_* macros
 */
#define ASSERT_INTERNAL(cond)                                                                                          \
	do {                                                                                                                 \
		if (!(cond)) {                                                                                                     \
			Assert::invoke_handler ({#cond, __FILE__, __LINE__});                                                            \
		}                                                                                                                  \
	} while (false)

#ifdef ASSERT_SAFE_ENABLED
#define ASSERT_SAFE(cond) ASSERT_INTERNAL (cond)
#else
#define ASSERT_SAFE(cond)
#endif

#ifdef ASSERT_STD_ENABLED
#define ASSERT_STD(cond) ASSERT_INTERNAL (cond)
#else
#define ASSERT_STD(cond)
#endif

#ifdef ASSERT_OPT_ENABLED
#define ASSERT_OPT(cond) ASSERT_INTERNAL (cond)
#else
#define ASSERT_OPT(cond)
#endif

/* Helper functions (mostly error handler management).
 */
#include <stdexcept> // logic_error

namespace Givy {
namespace Assert {
	/* Structure with assert information
	 */
	struct Info {
		const char * text;
		const char * file;
		int line;
	};

	using HandlerType = void (*) (const Info &);

	/* Only one handler is alive at any point in time.
	 * Modifiying the handler is thread safe.
	 *
	 * set_handler returns the old handler.
	 */
	HandlerType get_handler (void);
	HandlerType set_handler (HandlerType new_handler);
	void invoke_handler (const Info & inf);

	/* Standard handlers.
	 * abort print a message and aborts the program.
	 * throw will trigger an AssertException.
	 */
	void abort_handler (const Info & inf);
	void throw_handler (const Info & inf);
	// Also sleep handler

	class AssertException : public std::logic_error {
	public:
		AssertException (const Info & inf);
	};
}
}

#endif
