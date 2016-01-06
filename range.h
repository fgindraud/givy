#ifndef RANGE_H
#define RANGE_H

#include <iterator>

namespace Givy {

template <typename T> class Range {
private:
	const T start_;
	const T end_;

public:
	constexpr Range (T start, T end) : start_ (start), end_ (end) {}
	constexpr Range (T n) : Range (0, n) {}

	struct const_iterator : public std::iterator<std::bidirectional_iterator_tag, T> {
	private:
		T i_;
		constexpr const_iterator (T i) : i_ (i) {}
		friend class Range;

	public:
		constexpr const_iterator () : const_iterator (T ()) {}
		constexpr bool operator==(const_iterator other) const { return i_ == other.i_; }
		constexpr bool operator!=(const_iterator other) const { return i_ != other.i_; }
		constexpr const_iterator & operator++(void) {
			++i_;
			return *this;
		}
		constexpr const_iterator & operator--(void) {
			--i_;
			return *this;
		}
		constexpr const_iterator operator++(int) {
			auto cpy = *this;
			++*this;
			return cpy;
		}
		constexpr const_iterator operator--(int) {
			auto cpy = *this;
			--*this;
			return cpy;
		}
		constexpr const T & operator*(void) const { return i_; }
		constexpr const T * operator->(void) const { return &i_; }
	};

	constexpr const_iterator begin (void) const { return {start_}; }
	constexpr const_iterator end (void) const { return {end_}; }
};
template <typename T> static constexpr Range<T> range (T start, T end) {
	return {start, end};
}
template <typename T> static constexpr Range<T> range (T n) {
	return {n};
}
}

#endif
