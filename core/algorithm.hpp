#pragma once
#include <algorithm>

template <typename Container, typename Elem>
bool contains(Container &&where, Elem &&what)
{
	auto first = begin(where);
	auto last = end(where);
	return std::find(first, last, what) != last;
}