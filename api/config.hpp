#pragma once
#include "string_view.hpp"
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

//TODO merge tables

namespace config
{
class Error : public std::runtime_error
{
public:
	explicit Error(const std::string& msg);
};

class BadKey : public Error
{
public:
	BadKey(const std::string& key, const std::string& msg);
	auto key() const noexcept -> const std::string& { return k; }
private:
	const std::string k;
};

class BadValue : public BadKey
{
public:
	BadValue(const std::string& key, const std::string& expected, const std::string& obtained,
		const std::string& msg);
	auto expected() const noexcept -> const std::string& { return exp; }
	auto obtained() const noexcept -> const std::string& { return obt; }
private:
	const std::string exp;
	const std::string obt;
};

using Boolean = bool;
using Integer = std::int32_t;
using Real = double;
using String = std::string;
class Table;

class Property
{
public:
	struct ErrorHandler
	{
		virtual ~ErrorHandler() = default;
		virtual auto key_error(const std::string& msg) const -> std::string = 0;
		virtual auto value_error(const std::string& msg) const -> std::string = 0;
	};

	struct EmptyType {};
	static constexpr EmptyType empty{};

	template <typename T, typename... Ts>
	static constexpr bool one_of = std::disjunction_v<std::is_same<T, Ts>...>;
	
	template <typename T>
	static constexpr bool supported_type = one_of<T, Boolean, Integer, Real, String, Table>;
	
	template <typename T>
	static constexpr bool light_type = one_of<T, Boolean, Integer, Real>;
	
	template <typename T>
	static constexpr bool heavy_type = one_of<T, String, Table>;
	
	template <typename T>
	static constexpr bool scalar_type = one_of<T, Boolean, Integer, Real, String>;
	
	template <typename T>
	static constexpr bool complex_type = one_of<T, Table>;
	
	Property(std::unique_ptr<const ErrorHandler> eh,
		std::string key, EmptyType = empty);
	Property(std::unique_ptr<const ErrorHandler> eh,
		std::string key, Boolean val);
	Property(std::unique_ptr<const ErrorHandler> eh,
		std::string key, Integer val);
	Property(std::unique_ptr<const ErrorHandler> eh,
		std::string key, Real val);
	Property(std::unique_ptr<const ErrorHandler> eh,
		std::string key, const char *val);
	Property(std::unique_ptr<const ErrorHandler> eh,
		std::string key, String val);
	Property(std::unique_ptr<const ErrorHandler> eh,
		std::string key, Table val);
	Property(const Property& rhs) = default;
	Property(Property&& rhs) = default;
	~Property() = default;

	auto operator=(const Property& rhs) -> Property& = default;
	auto operator=(Property&& rhs) -> Property& = default;

	[[nodiscard]] auto has_value() const noexcept -> bool;
	explicit operator bool() const noexcept;
	auto operator!() const noexcept -> bool;

	[[nodiscard]] auto key() const noexcept -> const std::string&;

	template <typename T>
	[[nodiscard]] auto is() const noexcept -> bool;
	
	template <typename T>
	auto as() const -> std::enable_if_t<light_type<T>, T>;
	template <typename T>
	auto as() const -> std::enable_if_t<heavy_type<T>, const T&>;

	template <typename T>
	auto as_opt() const -> std::enable_if_t<scalar_type<T>, std::optional<T>>;

	template <typename T>
	auto get_or(T default_value) const -> std::enable_if_t<scalar_type<T>, T>;
	
	auto get_error_handler() const noexcept -> const ErrorHandler&;

	friend auto operator==(const Property& lhs, const Property& rhs) noexcept -> bool;
	friend auto operator!=(const Property& lhs, const Property& rhs) noexcept -> bool;

private:
	[[noreturn]] static auto never_called() -> void;
	[[noreturn]] auto raise_type_error(const std::string& expected) const -> void;
	
	template <typename T>
	static auto type_to_string() noexcept -> std::string;

	template <typename T, typename... Ts>
	struct TypeList
	{
		auto to_string() const
		{
			return "[" + (type_to_string<T>() + ... + (", " + type_to_string<Ts>())) + "]";
		}
	};
	
	template <typename Visitor, typename AllTypes, typename T, typename... Ts>
	auto visit_impl(Visitor&& visitor) const
	{
		if (is<T>())
			return visitor(as<T>());

		if constexpr (sizeof...(Ts) != 0)
			return visit_impl<Visitor, AllTypes, Ts...>(std::forward<Visitor>(visitor));
		else
			raise_type_error(AllTypes{}.to_string());
	}
	
	template <typename... Ts> friend struct Visit;

	struct Priv;
	std::shared_ptr<Priv> p;
};

class Table
{
public:
	struct ErrorHandler
	{
		virtual ~ErrorHandler() = default;
		virtual auto make_error_handler() -> std::unique_ptr<Property::ErrorHandler> = 0;
	};
	
	class const_iterator;
	using value_type = Property;

	explicit Table(std::unique_ptr<ErrorHandler> eh);
	Table(const Table& rhs) = delete;
	Table(Table&& rhs) noexcept;
	~Table();

	auto operator=(const Table&) -> Table& = delete;
	auto operator=(Table&&) noexcept -> Table&;

	auto add(Property val) -> Table&;

	[[nodiscard]] auto get_unique(string_view name) const -> const Property&;
	[[nodiscard]] auto get_last(string_view name) const -> const Property&;
	[[nodiscard]] auto operator[](string_view name) const -> const Property&;
	[[nodiscard]] auto get_all(string_view name) const -> std::vector<const Property*>;

	auto size() const -> std::size_t;

	auto begin() const -> const_iterator;
	auto cbegin() const -> const_iterator;
	auto end() const -> const_iterator;
	auto cend() const -> const_iterator;

	auto throw_on_unknown_key() const -> void;

	friend auto operator==(const Table& lhs, const Table& rhs) noexcept -> bool;
	friend auto operator!=(const Table& lhs, const Table& rhs) noexcept -> bool;

private:
	struct Priv;
	std::unique_ptr<Priv> p;
	friend class const_iterator;
};

template <> auto Property::is<Boolean>() const noexcept -> bool;
template <> auto Property::is<Integer>() const noexcept -> bool;
template <> auto Property::is<Real>() const noexcept -> bool;
template <> auto Property::is<String>() const noexcept -> bool;
template <> auto Property::is<Table>() const noexcept -> bool;

template <typename T>
auto Property::is() const noexcept -> bool
{
	return false;
}

template <typename T>
auto Property::as() const -> std::enable_if_t<light_type<T>, T>
{
	never_called();
}

template <typename T>
auto Property::as() const -> std::enable_if_t<heavy_type<T>, const T&>
{
	never_called();
}

template <> auto Property::as<Boolean>() const -> Boolean;
template <> auto Property::as<Integer>() const -> Integer;
template <> auto Property::as<Real>() const -> Real;
template <> auto Property::as<String>() const -> const String&;
template <> auto Property::as<Table>() const -> const Table&;

template <typename T>
auto Property::as_opt() const -> std::enable_if_t<scalar_type<T>, std::optional<T>>
{
	return has_value() ? std::optional<T>{ as<T>() } : std::optional<T>{};
}

template <typename T>
auto Property::get_or(T default_value) const -> std::enable_if_t<scalar_type<T>, T>
{
	return has_value() ? as<T>(): default_value;
}

template <> inline auto Property::type_to_string<Boolean>() noexcept -> std::string { return "boolean"; }
template <> inline auto Property::type_to_string<Integer>() noexcept -> std::string { return "integer"; }
template <> inline auto Property::type_to_string<Real>() noexcept -> std::string { return "real"; }
template <> inline auto Property::type_to_string<String>() noexcept -> std::string { return "string"; }
template <> inline auto Property::type_to_string<Table>() noexcept -> std::string { return "table"; }

inline Property::Property(std::unique_ptr<const ErrorHandler> eh,
                          std::string key, const char* val) :
	Property{ move(eh), move(key), String{ val } }
{
}

inline auto Property::has_value() const noexcept -> bool
{
	return static_cast<bool>(*this);
}

class Table::const_iterator
{
public:
	using value_type = Property;
	using reference = const value_type&;
	using pointer = const value_type*;
	using difference_type = int;
	using iterator_category = std::forward_iterator_tag;

	struct begin_tag {};
	struct end_tag {};

	const_iterator();
	const_iterator(const Table* tbl, begin_tag);
	const_iterator(const Table* tbl, end_tag);
	const_iterator(const const_iterator& rhs);
	const_iterator(const_iterator&& rhs) noexcept;
	~const_iterator();

	auto operator=(const const_iterator& rhs) -> const_iterator&;
	auto operator=(const_iterator&& rhs) noexcept -> const_iterator&;

	auto operator*() const -> reference;
	auto operator->() const -> pointer;
	auto operator++() -> const_iterator&;
	auto operator++(int) -> const_iterator;

	friend auto operator==(const const_iterator& it1,
		const const_iterator& it2) -> bool;
	friend auto operator!=(const const_iterator& it1,
		const const_iterator& it2) -> bool;

private:
	struct Priv;
	std::unique_ptr<Priv> p;
};

template <typename... Ts>
struct Visit
{
	static_assert((Property::supported_type<Ts> && ...),
		"config::Visit: unsupported Property value type provided");
	
	template <typename Visitor>
	auto operator()(const Property& prop, Visitor&& visitor) const
	{
		static_assert((std::is_invocable_v<Visitor, Ts> && ...),
			"config::Visit::operator(): provided type not handled by visitor");

		return prop.visit_impl<Visitor, Property::TypeList<Ts...>, Ts...>(std::forward<Visitor>(visitor));
	}
};
}