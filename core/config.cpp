#include "config.hpp"
#include "visitor.hpp"
#include <boost/concept_check.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <cstdlib>
#include <utility>
#include <variant>

namespace config
{
namespace
{
BOOST_CONCEPT_ASSERT((boost::ForwardIterator<Table::const_iterator>));

constexpr auto REAL_EPS = 0.001;

struct EmptyValue {};
auto operator==(EmptyValue, EmptyValue) noexcept { return true; }
[[maybe_unused]] auto operator!=(EmptyValue, EmptyValue) noexcept { return false; }

struct RoughReal
{
	Real val;
};

auto operator==(RoughReal lhs, RoughReal rhs) noexcept
{
	return std::abs(lhs.val - rhs.val) < REAL_EPS;
}

[[maybe_unused]] auto operator!=(RoughReal lhs, RoughReal rhs) noexcept
{
	return std::abs(lhs.val - rhs.val) > REAL_EPS;
}
}

template <> inline auto Property::type_to_string<RoughReal>() -> std::string { return "real"; }

namespace
{
using ValueTuple = std::pair<Property, bool>;

struct KeyComparator
{
	const string_view key;

	auto operator()(const ValueTuple& el) const
	{
		return el.first.key() == key;
	}
};
}

Error::Error(const std::string& msg): runtime_error{ msg }
{}

BadKey::BadKey(const std::string& key, const std::string& msg):
	Error{ "key \"" + key + "\": " + msg },
	k{ key }
{
}

BadValue::BadValue(const std::string& key, const std::string& expected, const std::string& obtained,
	const std::string& msg):
	BadKey{ key, msg },
	exp{ expected },
	obt{ obtained }
{
}

struct Property::Priv
{
	const String k;
	const std::variant<EmptyValue, Boolean, Integer, RoughReal, String, Table> val;
	const std::unique_ptr<const ErrorHandler> eh;

	template <typename T>
	auto is() const noexcept
	{
		return std::holds_alternative<T>(val);
	}

	auto type_string() const
	{
		return visit(Visitor{
			[](EmptyValue) { return std::string{"empty"}; },
			[](Boolean) { return type_to_string<Boolean>(); },
			[](Integer) { return type_to_string<Integer>(); },
			[](RoughReal) { return type_to_string<Real>(); },
			[](const String&) { return type_to_string<String>(); },
			[](const Table&) { return type_to_string<Table>(); },
			}, val);
	}

	[[noreturn]] auto raise_type_error(const std::string& expected) const
	{
		auto obtained = type_string();
		auto msg = "expected: " + expected + ", but got: " + obtained;
		throw BadValue{ k, expected, obtained, eh->value_error(msg) };
	}
	
	template <typename T>
	auto get() const -> std::conditional_t<light_type<T>, T, const T&>
	{
		auto* v = std::get_if<T>(&val);
		if (BOOST_UNLIKELY(!v)) {
			if (is<EmptyValue>())
				throw BadKey{ k, eh->key_error("not found") };
			auto expected = type_to_string<T>();
			raise_type_error(expected);
		}

		return *v;
	}

	auto as_tuple() const
	{
		return tie(k, val);
	}
};

Property::Property(std::unique_ptr<const ErrorHandler> eh,
	std::string key, EmptyType) :
	p{ new Priv{ move(key), {}, move(eh) } }
{
}

Property::Property(std::unique_ptr<const ErrorHandler> eh,
	std::string key, Boolean val) :
	p{ new Priv{ move(key), val, move(eh) } }
{
}

Property::Property(std::unique_ptr<const ErrorHandler> eh,
	std::string key, Integer val) :
	p{ new Priv{ move(key), val, move(eh) } }
{
}

Property::Property(std::unique_ptr<const ErrorHandler> eh,
	std::string key, Real val) :
	p{ new Priv{ move(key), RoughReal{val}, move(eh) } }
{
}

Property::Property(std::unique_ptr<const ErrorHandler> eh,
	std::string key, String val) :
	p{ new Priv{ move(key), std::move(val), move(eh) } }
{
}

Property::Property(std::unique_ptr<const ErrorHandler> eh,
	std::string key, Table val) :
	p{ new Priv{ move(key), std::move(val), move(eh) } }
{
}

Property::Property(Property&& rhs) noexcept :
	p{ std::exchange(rhs.p, nullptr) }
{
}

Property::~Property()
{
	delete p;
}

Property::operator bool() const noexcept
{
	return !p->is<EmptyValue>();
}

auto Property::operator!() const noexcept -> bool
{
	return p->is<EmptyValue>();
}

auto Property::key() const noexcept -> const String&
{
	return p->k;
}

auto Property::get_error_handler() const noexcept -> const ErrorHandler&
{
	return *p->eh;
}

auto Property::never_called() -> void
{
	std::abort();
}

auto Property::raise_type_error(const std::string& expected) const -> void
{
	p->raise_type_error(expected);
}

template <>
auto Property::is<Boolean>() const noexcept -> bool
{
	return p->is<Boolean>();
}

template <>
auto Property::is<Integer>() const noexcept -> bool
{
	return p->is<Integer>();
}

template <>
auto Property::is<Real>() const noexcept -> bool
{
	return p->is<RoughReal>();
}

template <>
auto Property::is<String>() const noexcept -> bool
{
	return p->is<String>();
}

template <>
auto Property::is<Table>() const noexcept -> bool
{
	return p->is<Table>();
}

template <>
auto Property::as<Boolean>() const -> Boolean
{
	return p->get<Boolean>();
}

template <>
auto Property::as<Integer>() const -> Integer
{
	return p->get<Integer>();
}

template <>
auto Property::as<Real>() const -> Real
{
	return p->get<RoughReal>().val;
}

template <>
auto Property::as<String>() const -> const String&
{
	return p->get<String>();
}

template <>
auto Property::as<Table>() const -> const Table&
{
	return p->get<Table>();
}

struct Table::Priv
{
	const std::unique_ptr<ErrorHandler> eh;
	std::vector<ValueTuple> map;
	std::vector<Property> empty_vals;

	explicit Priv(std::unique_ptr<ErrorHandler> eh): eh(move(eh)) {}

	auto empty_value(string_view key) -> const Property&
	{
		empty_vals.emplace_back(eh->make_error_handler(), String{ key }, Property::empty);
		return empty_vals.back();
	}
};

Table::Table(std::unique_ptr<ErrorHandler> eh):
	p{ new Priv{ move(eh) } }
{
}

Table::Table(Table&& rhs) noexcept:
	p{ std::exchange(rhs.p, nullptr) }
{
}

Table::~Table()
{
	delete p;
}

auto Table::add(Property val) -> Table&
{
	p->map.emplace_back(std::move(val), false);
	return *this;
}

auto Table::get_unique(string_view name) const -> const Property&
{
	const KeyComparator pred{ name };
	const auto end = p->map.end();

	auto it = boost::find_if(p->map, pred);
	if (it == end)
		return p->empty_value(name);

	auto it2 = find_if(next(it), end, pred);
	if (it2 != end)
		throw BadKey{ it2->first.key(), it2->first.get_error_handler().key_error("non-unique") };

	auto& [prop, used] = *it;
	used = true;
	return prop;
}

auto Table::get_last(string_view name) const -> const Property&
{
	const Property* result = nullptr;
	for (auto& [prop, used] : p->map | boost::adaptors::filtered(KeyComparator{ name })) {
		result = &prop;
		used = true;
	}

	return result ? *result : p->empty_value(name);
}

auto Table::get_all(string_view name) const -> std::vector<const Property*>
{
	std::vector<const Property*> res;
	for (auto& [prop, used] : p->map | boost::adaptors::filtered(KeyComparator{ name })) {
		res.push_back(&prop);
		used = true;
	}

	return res;
}

auto Table::size() const -> std::size_t
{
	return p->map.size();
}

auto Table::operator[](string_view name) const -> const Property&
{
	return get_unique(name);
}

auto Table::begin() const -> const_iterator
{
	return { this, const_iterator::begin_tag{} };
}

auto Table::cbegin() const -> const_iterator
{
	return begin();
}

auto Table::end() const -> const_iterator
{
	return { this, const_iterator::end_tag{} };
}

auto Table::cend() const -> const_iterator
{
	return end();
}

auto Table::throw_on_unknown_key() const -> void
{
	for (auto& [prop, used] : p->map) {
		if (!used)
			throw BadKey{ prop.key(), prop.get_error_handler().key_error("unknown key") };
		if (prop.is<Table>())
			prop.as<Table>().throw_on_unknown_key();
	}
}

struct Table::const_iterator::Priv
{
	decltype(Table::Priv::map)::const_iterator it;
};

Table::const_iterator::const_iterator() :
	p{ new Priv{} }
{
}

Table::const_iterator::const_iterator(const Table* tbl, begin_tag) :
	p{ new Priv{ tbl->p->map.begin() } }
{
}

Table::const_iterator::const_iterator(const Table* tbl, end_tag) :
	p{ new Priv{ tbl->p->map.end() } }
{
}

Table::const_iterator::const_iterator(const const_iterator& rhs) :
	p{ new Priv{ *rhs.p } }
{
}

Table::const_iterator::const_iterator(const_iterator&& rhs) noexcept :
	p{ std::exchange(rhs.p, nullptr) }
{
}

Table::const_iterator::~const_iterator()
{
	delete p;
}

auto Table::const_iterator::operator=(const const_iterator& rhs) -> const_iterator&
{
	delete p;
	p = new Priv{ *rhs.p };
	return *this;
}

auto Table::const_iterator::operator=(const_iterator&& rhs) noexcept -> const_iterator&
{
	delete p;
	p = std::exchange(rhs.p, nullptr);
	return *this;
}

auto Table::const_iterator::operator*() const -> reference
{
	return p->it->first;
}

auto Table::const_iterator::operator->() const -> pointer
{
	return &**this;
}

auto Table::const_iterator::operator++() -> const_iterator&
{
	++p->it;
	return *this;
}

auto Table::const_iterator::operator++(int) -> const_iterator
{
	auto tmp(*this);
	++p->it;
	return tmp;
}

auto operator==(const Property& lhs, const Property& rhs) noexcept -> bool
{
	return lhs.p->as_tuple() == rhs.p->as_tuple();
}

auto operator!=(const Property& lhs, const Property& rhs) noexcept -> bool
{
	return lhs.p->as_tuple() != rhs.p->as_tuple();
}

auto operator==(const Table& lhs, const Table& rhs) noexcept -> bool
{
	return lhs.p->map == rhs.p->map;
}

auto operator!=(const Table& lhs, const Table& rhs) noexcept -> bool
{
	return lhs.p->map != rhs.p->map;
}

auto operator==(const Table::const_iterator& it1, const Table::const_iterator& it2) -> bool
{
	return it1.p->it == it2.p->it;
}

auto operator!=(const Table::const_iterator& it1, const Table::const_iterator& it2) -> bool
{
	return !(it1 == it2);
}
}