#include "config.hpp"
#include <boost/variant.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <utility>

namespace
{
using int_ = table::value::int_;
using real = table::value::real;
using std::string;

struct empty_value_t {};
auto operator==(empty_value_t, empty_value_t) noexcept { return true; }
auto operator!=(empty_value_t, empty_value_t) noexcept { return false; }

struct value_type_visitor: boost::static_visitor<string>
{
	auto operator()(empty_value_t) { return "empty"; }
	auto operator()(bool) { return "bool"; }
	auto operator()(int_) { return "int"; }
	auto operator()(double) { return "real"; }
	auto operator()(const string&) { return "string"; }
	auto operator()(const table&) { return "table"; }
};

using value_tuple = std::pair<table::value, bool>;

struct key_comparator
{
	const string_view key;

	auto operator()(const value_tuple &el) const
	{
		return el.first.key() == key;
	}
};

}

struct table::value::priv
{
	string k;
	boost::variant<empty_value_t, bool, int_, real, string, table> val{};

	template <typename T>
	auto is() const noexcept
	{
		return boost::get<T>(&val) != nullptr;
	}

	template <typename T>
	auto get(string_view expected_type) -> const T&
	{
		auto *v = boost::get<T>(&val);
		if (BOOST_UNLIKELY(!v)) {
			if (is<empty_value_t>())
				throw error{ k, "not found" };
			auto visitor = value_type_visitor{};
			throw error{ k,
				"expected type: " + string{ expected_type }
				+ ", but got: " + apply_visitor(visitor, val)};
		}

		return *v;
	}

	auto as_tuple() const
	{
		return tie(k, val);
	}
};

table::value::value(string key, empty):
	p{ new priv{ move(key) } }
{
}

table::value::value(string key, bool val):
	p{ new priv{ move(key), val } }
{
}

table::value::value(string key, int_ val):
	p{ new priv{ move(key), val } }
{
}

table::value::value(string key, real val):
	p{ new priv{ move(key), val } }
{
}

table::value::value(string key, string val):
	p{ new priv{ move(key), move(val) } }
{
}

table::value::value(string key, table val):
	p{ new priv{ move(key), std::move(val) } }
{
}

table::value::value(const value& rhs):
	p{ new priv{ *rhs.p } }
{
}

table::value::value(value&& rhs) noexcept:
	p{ std::exchange(rhs.p, nullptr) }
{
}

table::value::~value()
{
	delete p;
}

table::value::operator bool() const noexcept
{
	return !p->is<empty_value_t>();
}

auto table::value::operator!() const noexcept -> bool
{
	return p->is<empty_value_t>();
}

auto table::value::key() const noexcept -> const string&
{
	return p->k;
}

template <>
auto table::value::is<bool>() const noexcept -> bool
{
	return p->is<bool>();
}

template <>
auto table::value::is<int_>() const noexcept -> bool
{
	return p->is<int_>();
}

template <>
auto table::value::is<double>() const noexcept -> bool
{
	return p->is<double>();
}

template <>
auto table::value::is<string>() const noexcept -> bool
{
	return p->is<string>();
}

template <>
auto table::value::is<table>() const noexcept -> bool
{
	return p->is<table>();
}

template <>
auto table::value::as<bool>() const -> const bool&
{
	return p->get<bool>("bool"_w);
}

template <>
auto table::value::as<int_>() const -> const int_&
{
	return p->get<int_>("int"_w);
}

template <>
auto table::value::as<double>() const -> const double&
{
	return p->get<double>("real"_w);
}

template <>
auto table::value::as<string>() const -> const string&
{
	return p->get<string>("string"_w);
}

template <>
auto table::value::as<table>() const -> const table&
{
	return p->get<table>("table"_w);
}

table::error::error(const string &key, const string &msg) :
	runtime_error{ "property \"" + key + "\": " + msg },
	key{ key }
{
}

struct table::priv
{
	std::vector<value_tuple> map;
	std::vector<value> empty_vals;

	auto empty_value(string_view key) -> const value&
	{
		empty_vals.emplace_back(string{ key }, value::empty{});
		return empty_vals.back();
	}
};

table::table():
	p{ new priv{} }
{
}

table::table(const table &rhs):
	p{ new priv{ *rhs.p } }
{
}

table::table(table &&rhs) noexcept:
	p{ std::exchange(rhs.p, nullptr) }
{
}

table::~table()
{
	delete p;
}

auto table::add(value val) -> table&
{
	p->map.emplace_back(std::move(val), false);
	return *this;
}

auto table::get_unique(string_view name) const -> const value&
{
	const key_comparator pred{ name };
	const auto end = p->map.end();

	auto it = boost::find_if(p->map, pred);
	if (it == end)
		return p->empty_value(name);

	auto it2 = find_if(next(it), end, pred);
	if (it2 != end)
		throw error{ it2->first.key(), string{ "non-unique" } };

	it->second = true;
	return it->first;
}

auto table::get_last(string_view name) const -> const value&
{
	const value *result = nullptr;
	for (auto &e : p->map | boost::adaptors::filtered(key_comparator{ name })) {
		result = &e.first;
		e.second = true;
	}

	return result ? *result : p->empty_value(name);
}

auto table::get_all(string_view name) const -> std::vector<const value*>
{
	std::vector<const value*> res;
	for (auto &e: p->map | boost::adaptors::filtered(key_comparator{ name })) {
		res.push_back(&e.first);
		e.second = true;
	}

	return res;
}

auto table::operator[](string_view name) const -> const value&
{
	return get_unique(name);
}

auto table::begin() const -> const_iterator
{
	return { this, const_iterator::begin_tag{} };
}

auto table::cbegin() const -> const_iterator
{
	return begin();
}

auto table::end() const -> const_iterator
{
	return { this, const_iterator::end_tag{} };
}

auto table::cend() const -> const_iterator
{
	return end();
}

auto table::throw_on_unknown_key() const -> void
{
	for (auto &v: p->map) {
		if (!v.second)
			throw error{ v.first.key(), "unknown key" };
		if (v.first.is<table>())
			v.first.as<table>().throw_on_unknown_key();
	}
}

struct table::const_iterator::priv
{
	decltype(table::priv::map)::const_iterator it;
};

table::const_iterator::const_iterator():
	p{ new priv{} }
{
}

table::const_iterator::const_iterator(const table *tbl, begin_tag):
	p{ new priv{ tbl->p->map.begin() } }
{
}

table::const_iterator::const_iterator(const table* tbl, end_tag):
	p{ new priv{ tbl->p->map.end() } }
{
}

table::const_iterator::const_iterator(const const_iterator &rhs):
	p{ new priv{ *rhs.p } }
{
}

table::const_iterator::const_iterator(const_iterator &&rhs) noexcept:
	p{ std::exchange(rhs.p, nullptr) }
{
}

table::const_iterator::~const_iterator()
{
	delete p;
}

auto table::const_iterator::operator=(const const_iterator& rhs) -> const_iterator&
{
	delete p;
	p = new priv{ *rhs.p };
	return *this;
}

auto table::const_iterator::operator=(const_iterator&& rhs) noexcept -> const_iterator&
{
	delete p;
	p = std::exchange(rhs.p, nullptr);
	return *this;
}

auto table::const_iterator::operator*() const -> reference
{
	return p->it->first;
}

auto table::const_iterator::operator->() const -> pointer
{
	return &**this;
}

auto table::const_iterator::operator++() -> const_iterator&
{
	++p->it;
	return *this;
}

auto table::const_iterator::operator++(int) -> const_iterator
{
	auto tmp(*this);
	++p->it;
	return tmp;
}

auto operator==(const table::value &lhs, const table::value &rhs) noexcept -> bool
{
	return lhs.p->as_tuple() == rhs.p->as_tuple();
}

auto operator!=(const table::value &lhs, const table::value &rhs) noexcept -> bool
{
	return lhs.p->as_tuple() != rhs.p->as_tuple();
}

auto operator==(const table& lhs, const table& rhs) noexcept -> bool
{
	return lhs.p->map == rhs.p->map;
}

auto operator!=(const table& lhs, const table& rhs) noexcept -> bool
{
	return lhs.p->map != rhs.p->map;
}

auto operator==(const table::const_iterator& it1, const table::const_iterator& it2) -> bool
{
	return it1.p->it == it2.p->it;
}

auto operator!=(const table::const_iterator& it1, const table::const_iterator& it2) -> bool
{
	return !(it1 == it2);
}
