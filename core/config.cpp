#include "config.hpp"
#include <boost/variant.hpp>
#include <boost/concept_check.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <utility>

BOOST_CONCEPT_ASSERT((boost::ForwardIterator<config::table::const_iterator>));

namespace config
{
namespace
{
struct empty_value_t {};
auto operator==(empty_value_t, empty_value_t) noexcept { return true; }
auto operator!=(empty_value_t, empty_value_t) noexcept { return false; }

struct value_type_visitor : boost::static_visitor<string>
{
	auto operator()(empty_value_t) const { return "empty"; }
	auto operator()(boolean) const { return "boolean"; }
	auto operator()(integer) const { return "integer"; }
	auto operator()(real) const { return "real"; }
	auto operator()(const string&) const { return "string"; }
	auto operator()(const table&) const { return "table"; }
};

using value_tuple = std::pair<property, bool>;

struct key_comparator
{
	const string_view key;

	auto operator()(const value_tuple &el) const
	{
		return el.first.key() == key;
	}
};
}

error::error(const std::string &msg): runtime_error{ msg }
{}

bad_key::bad_key(const std::string &key, const std::string &msg):
	error{ "key \"" + key + "\": " + msg },
	k{ key }
{
}

bad_value::bad_value(const std::string &key, const std::string &expected, const std::string &obtained,
	const std::string &msg):
	bad_key{ key, msg }
{
}

struct property::priv
{
	const string k;
	const boost::variant<empty_value_t, boolean, integer, real, string, table> val;
	const std::unique_ptr<const error_handler> eh;

	template <typename T>
	auto is() const noexcept
	{
		return boost::get<T>(&val) != nullptr;
	}

	template <typename T>
	auto get(string_view expected_type) const -> const T&
	{
		auto *v = boost::get<T>(&val);
		if (BOOST_UNLIKELY(!v)) {
			if (is<empty_value_t>())
				throw bad_key{ k, eh->key_error("not found") };
			auto expected = std::string{ expected_type };
			auto obtained = apply_visitor(value_type_visitor{}, val);
			auto msg = "expected: " + expected + ", but got: " + obtained;
			throw bad_value{ k, expected, obtained,
				eh->value_error(msg) };
		}

		return *v;
	}

	auto as_tuple() const
	{
		return tie(k, val);
	}
};

property::property(std::unique_ptr<const error_handler> eh,
	std::string key, empty_type) :
	p{ new priv{ move(key), {}, move(eh) } }
{
}

property::property(std::unique_ptr<const error_handler> eh,
	std::string key, boolean val) :
	p{ new priv{ move(key), val, move(eh) } }
{
}

property::property(std::unique_ptr<const error_handler> eh,
	std::string key, integer val) :
	p{ new priv{ move(key), val, move(eh) } }
{
}

property::property(std::unique_ptr<const error_handler> eh,
	std::string key, real val) :
	p{ new priv{ move(key), val, move(eh) } }
{
}

property::property(std::unique_ptr<const error_handler> eh,
	std::string key, string val) :
	p{ new priv{ move(key), std::move(val), move(eh) } }
{
}

property::property(std::unique_ptr<const error_handler> eh,
	std::string key, table val) :
	p{ new priv{ move(key), std::move(val), move(eh) } }
{
}

property::property(property &&rhs) noexcept :
	p{ std::exchange(rhs.p, nullptr) }
{
}

property::~property()
{
	delete p;
}

property::operator bool() const noexcept
{
	return !p->is<empty_value_t>();
}

auto property::operator!() const noexcept -> bool
{
	return p->is<empty_value_t>();
}

auto property::key() const noexcept -> const string&
{
	return p->k;
}

auto property::get_error_handler() const -> const error_handler&
{
	return *p->eh;
}

template <>
auto property::is<boolean>() const noexcept -> bool
{
	return p->is<boolean>();
}

template <>
auto property::is<integer>() const noexcept -> bool
{
	return p->is<integer>();
}

template <>
auto property::is<real>() const noexcept -> bool
{
	return p->is<real>();
}

template <>
auto property::is<string>() const noexcept -> bool
{
	return p->is<string>();
}

template <>
auto property::is<table>() const noexcept -> bool
{
	return p->is<table>();
}

template <>
auto property::as<boolean>() const -> const boolean&
{
	return p->get<boolean>("boolean"_w);
}

template <>
auto property::as<integer>() const -> const integer&
{
	return p->get<integer>("integer"_w);
}

template <>
auto property::as<real>() const -> const real&
{
	return p->get<real>("real"_w);
}

template <>
auto property::as<string>() const -> const string&
{
	return p->get<string>("string"_w);
}

template <>
auto property::as<table>() const -> const table&
{
	return p->get<table>("table"_w);
}

struct table::priv
{
	const std::unique_ptr<error_handler> eh;
	std::vector<value_tuple> map;
	std::vector<property> empty_vals;

	explicit priv(std::unique_ptr<error_handler> eh): eh(move(eh)) {}

	auto empty_value(string_view key) -> const property&
	{
		empty_vals.emplace_back(eh->make_error_handler(), string{ key }, property::empty);
		return empty_vals.back();
	}
};

table::table(std::unique_ptr<error_handler> eh):
	p{ new priv{ move(eh) } }
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

auto table::add(property val) -> table&
{
	p->map.emplace_back(std::move(val), false);
	return *this;
}

auto table::get_unique(string_view name) const -> const property&
{
	const key_comparator pred{ name };
	const auto end = p->map.end();

	auto it = boost::find_if(p->map, pred);
	if (it == end)
		return p->empty_value(name);

	auto it2 = find_if(next(it), end, pred);
	if (it2 != end)
		throw bad_key{ it2->first.key(), it2->first.get_error_handler().key_error("non-unique") };

	it->second = true;
	return it->first;
}

auto table::get_last(string_view name) const -> const property&
{
	const property *result = nullptr;
	for (auto &e : p->map | boost::adaptors::filtered(key_comparator{ name })) {
		result = &e.first;
		e.second = true;
	}

	return result ? *result : p->empty_value(name);
}

auto table::get_all(string_view name) const -> std::vector<const property*>
{
	std::vector<const property*> res;
	for (auto &e : p->map | boost::adaptors::filtered(key_comparator{ name })) {
		res.push_back(&e.first);
		e.second = true;
	}

	return res;
}

auto table::operator[](string_view name) const -> const property&
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
	for (auto &v : p->map) {
		if (!v.second)
			throw bad_key{ v.first.key(), v.first.get_error_handler().key_error("unknown key") };
		if (v.first.is<table>())
			v.first.as<table>().throw_on_unknown_key();
	}
}

struct table::const_iterator::priv
{
	decltype(table::priv::map)::const_iterator it;
};

table::const_iterator::const_iterator() :
	p{ new priv{} }
{
}

table::const_iterator::const_iterator(const table *tbl, begin_tag) :
	p{ new priv{ tbl->p->map.begin() } }
{
}

table::const_iterator::const_iterator(const table* tbl, end_tag) :
	p{ new priv{ tbl->p->map.end() } }
{
}

table::const_iterator::const_iterator(const const_iterator &rhs) :
	p{ new priv{ *rhs.p } }
{
}

table::const_iterator::const_iterator(const_iterator &&rhs) noexcept :
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

auto operator==(const property &lhs, const property &rhs) noexcept -> bool
{
	return lhs.p->as_tuple() == rhs.p->as_tuple();
}

auto operator!=(const property &lhs, const property &rhs) noexcept -> bool
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
}