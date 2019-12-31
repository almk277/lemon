#pragma once
#include "utility.hpp"
#include <string>
#include <vector>
#include <iterator>
#include <stdexcept>
#include <memory>
#include <type_traits>
#include <cstdint>

namespace config
{
class error : public std::runtime_error
{
public:
	explicit error(const std::string &msg);
};

class bad_key : public error
{
public:
	bad_key(const std::string &key, const std::string &msg);
	auto key() const noexcept -> const std::string& { return k; }
private:
	const std::string k;
};

class bad_value : public bad_key
{
public:
	bad_value(const std::string &key, const std::string &expected, const std::string &obtained,
		const std::string &msg);
};

using boolean = bool;
using integer = std::int32_t;
using real = double;
using string = std::string;
class table;
class file;

class property
{
public:
	struct error_handler
	{
		virtual ~error_handler() = default;
		virtual auto key_error(const std::string &msg) const -> std::string = 0;
		virtual auto value_error(const std::string &msg) const -> std::string = 0;
	};

	struct empty_type {};
	static constexpr empty_type empty{};

	property(std::unique_ptr<const error_handler> eh,
		std::string key, empty_type = empty);
	property(std::unique_ptr<const error_handler> eh,
		std::string key, boolean val);
	property(std::unique_ptr<const error_handler> eh,
		std::string key, integer val);
	property(std::unique_ptr<const error_handler> eh,
		std::string key, real val);
	property(std::unique_ptr<const error_handler> eh,
		std::string key, const char *val);
	property(std::unique_ptr<const error_handler> eh,
		std::string key, string val);
	property(std::unique_ptr<const error_handler> eh,
		std::string key, table val);
	property(const property &rhs) = delete;
	property(property &&rhs) noexcept;
	~property();

	auto operator=(const property& rhs)->property& = delete;
	auto operator=(property&& rhs)->property& = delete;

	explicit operator bool() const noexcept;
	auto has_value() const noexcept -> bool;
	auto operator!() const noexcept -> bool;

	auto key() const noexcept -> const std::string&;

	template <typename T>
	auto is() const noexcept -> bool;

	template <typename T>
	std::enable_if_t<
		   std::is_same<T, boolean>::value
		|| std::is_same<T, integer>::value
		|| std::is_same<T, real>::value
		|| std::is_same<T, string>::value
		|| std::is_same<T, table>::value
		, const T&> as() const;

	auto get_error_handler() const -> const error_handler&;

	friend auto operator==(const property &lhs, const property &rhs) noexcept -> bool;
	friend auto operator!=(const property &lhs, const property &rhs) noexcept -> bool;

private:
	struct priv;
	priv *p;
};

class table
{
public:
	struct error_handler
	{
		virtual ~error_handler() = default;
		virtual auto make_error_handler() -> std::unique_ptr<property::error_handler> = 0;
	};
	
	class const_iterator;
	using value_type = property;

	explicit table(std::unique_ptr<error_handler> eh);
	table(const table &rhs) = delete;
	table(table &&rhs) noexcept;
	~table();

	auto operator=(const table&) -> table& = delete;
	auto operator=(table&&) -> table& = delete;

	auto add(property val) -> table&;

	auto get_unique(string_view name) const -> const property&;
	auto get_last(string_view name) const -> const property&;
	auto operator[](string_view name) const -> const property&;
	auto get_all(string_view name) const -> std::vector<const property*>;

	auto size() const -> std::size_t;

	auto begin() const -> const_iterator;
	auto cbegin() const -> const_iterator;
	auto end() const -> const_iterator;
	auto cend() const -> const_iterator;

	auto throw_on_unknown_key() const -> void;

	friend auto operator==(const table &lhs, const table &rhs) noexcept -> bool;
	friend auto operator!=(const table &lhs, const table &rhs) noexcept -> bool;

private:
	struct priv;
	priv *p;
	friend class const_iterator;
};

template <>
auto property::is<boolean>() const noexcept -> bool;

template <>
auto property::is<integer>() const noexcept -> bool;

template <>
auto property::is<real>() const noexcept -> bool;

template <>
auto property::is<string>() const noexcept -> bool;

template <>
auto property::is<table>() const noexcept -> bool;

template <typename T>
auto property::is() const noexcept -> bool
{
	return false;
}

inline property::property(std::unique_ptr<const error_handler> eh,
	std::string key, const char* val) :
	property{ move(eh), move(key), string{ val } }
{
}

inline auto property::has_value() const noexcept -> bool
{
	return static_cast<bool>(*this);
}

class table::const_iterator
{
public:
	using value_type = property;
	using reference = const value_type&;
	using pointer = const value_type*;
	using difference_type = int;
	using iterator_category = std::forward_iterator_tag;

	struct begin_tag {};
	struct end_tag {};

	const_iterator();
	const_iterator(const table *tbl, begin_tag);
	const_iterator(const table *tbl, end_tag);
	const_iterator(const const_iterator &rhs);
	const_iterator(const_iterator &&rhs) noexcept;
	~const_iterator();

	auto operator=(const const_iterator &rhs)->const_iterator&;
	auto operator=(const_iterator &&rhs) noexcept->const_iterator&;

	auto operator*() const->reference;
	auto operator->() const->pointer;
	auto operator++()->const_iterator&;
	auto operator++(int)->const_iterator;

	friend auto operator==(const const_iterator& it1,
		const const_iterator& it2) -> bool;
	friend auto operator!=(const const_iterator& it1,
		const const_iterator& it2) -> bool;

private:
	struct priv;
	priv *p;
};
}