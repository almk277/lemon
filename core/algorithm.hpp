#pragma once
#include <algorithm>

template <typename Container, typename Elem>
bool contains(Container &&where, Elem &&what)
{
	using namespace std;

	auto first = begin(where);
	auto last = end(where);
	return find(first, last, what) != last;
}