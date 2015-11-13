#include "assert_level.h"

#include <atomic>
#include <cstdio> // fprintf
#include <cstdlib> // abort

namespace Givy {
namespace Assert {

static std::atomic<HandlerType> current_handler (&abort_handler);

HandlerType get_handler (void) {
	return current_handler.load ();
}

HandlerType set_handler (HandlerType new_handler) {
	HandlerType expected = get_handler ();
	while (!current_handler.compare_exchange_weak (expected, new_handler));
	return expected;
}

void invoke_handler (const Info & inf) {
	get_handler () (inf);
}

static void print_error (const Info & inf) {
	const char * text = inf.text;
	if (text == nullptr) text = "<nullptr>";
	if (*text == '\0') text = "<empty string>";

	const char * file = inf.file;
	if (file == nullptr) file = "<nullptr>";
	if (*file == '\0') file = "<empty string>";

	std::fprintf (::stderr, "Assert '%s' failed, file '%s', at line %d\n", text, file, inf.line);
}

void abort_handler (const Info & inf) {
	print_error (inf);
	std::abort ();
}

void throw_handler (const Info & inf) {
	if (!std::uncaught_exception ()) {
		throw AssertException (inf);
	}

	/* If an exception is already in flight, print and abort */
	abort_handler (inf);
}

AssertException::AssertException (const Info & inf) : std::logic_error (inf.text) {
	// TODO formatting
}

}
}
