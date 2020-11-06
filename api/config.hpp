#pragma once
#include "string_view.hpp"
#include <string>
#include <vector>
#include <iterator>
#include <stdexcept>
#include <memory>
#include <type_traits>
#include <cstdint>

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
};

using Boolean = bool;
using Integer = std::int32_t;
using Real = double;
using String = std::string;
class Table;
class File;

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
	Property(const Property& rhs) = delete;
	Property(Property&& rhs) noexcept;
	~Property();

	auto operator=(const Property& rhs) -> Property& = delete;
	auto operator=(Property&& rhs) -> Property& = delete;

	[[nodiscard]] auto has_value() const noexcept -> bool;
	explicit operator bool() const noexcept;
	auto operator!() const noexcept -> bool;

	[[nodiscard]] auto key() const noexcept -> const std::string&;

	template <typename T>
	[[nodiscard]] auto is() const noexcept -> bool;

	template <typename T>
	std::enable_if_t<
		   std::is_same_v<T, Boolean>
		|| std::is_same_v<T, Integer>
		|| std::is_same_v<T, Real>
		|| std::is_same_v<T, String>
		|| std::is_same_v<T, Table>
		, const T&> as() const;

	auto get_error_handler() const -> const ErrorHandler&;

	friend auto operator==(const Property& lhs, const Property& rhs) noexcept -> bool;
	friend auto operator!=(const Property& lhs, const Property& rhs) noexcept -> bool;

private:
	struct Priv;
	Priv* p;
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
	auto operator=(Table&&) -> Table& = delete;

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
	Priv* p;
	friend class const_iterator;
};

template <>
auto Property::is<Boolean>() const noexcept -> bool;

template <>
auto Property::is<Integer>() const noexcept -> bool;

template <>
auto Property::is<Real>() const noexcept -> bool;

template <>
auto Property::is<String>() const noexcept -> bool;

template <>
auto Property::is<Table>() const noexcept -> bool;

template <typename T>
auto Property::is() const noexcept -> bool
{
	return false;
}

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
	Priv* p;
};
}