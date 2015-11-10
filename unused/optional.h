#ifndef OPTIONAL_H
#define OPTIONAL_H

#include <experimental/optional>

using std::experimental::nullopt;
using std::experimental::bad_optional_access;

template <typename T> class Optional : public std::experimental::optional<T> {};

/* Reference specialisation
 * - does not support in_place stuff
 * - no move semantics, as they do nothing different from copy
 * - memory of T is not owned, so different const qualifiers as the standard
 * - can be initialised with pointers of references
 * - is exactly the size of a pointer (no redundant boolean)
 */
template <typename T> class Optional<T &> {
	/* nullptr is reported as a non valid value */
public:
	using value_type = T &;

	constexpr Optional () noexcept = default;
	constexpr Optional (std::experimental::nullopt_t) noexcept : Optional () {}
	Optional (const Optional & other) noexcept = default;
	constexpr Optional (T & value) noexcept : val (&value) {}
	constexpr Optional (T * value) noexcept : val (value) {}
	constexpr Optional (std::nullptr_t) noexcept : Optional () {}

	~Optional () = default;

	Optional & operator=(const Optional & other) = default;
	Optional & operator=(Optional && other) = default;

	constexpr T * operator->() const noexcept { return val; }
	constexpr T & operator*() const noexcept { return *val; }

	constexpr explicit operator bool() const noexcept { return val; }

	constexpr T & value (void) const {
		if (val == nullptr) throw bad_optional_access ("invalid value");
		return *val;
	}

	constexpr T & value_or (T & default_value) const noexcept { return val ? *val : default_value; }

private:
	T * val;
};

#endif
