#pragma once
#include "utility.hpp"
#include <string>
#include <vector>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <cstdint>

class table
{
public:
	class error : public std::runtime_error
	{
	public:
		error(const std::string &key, const std::string &msg);
		const std::string &get_key() const { return key; }
	private:
		const std::string key;
	};

	class value
	{
	public:
		struct empty {};
		using int_ = std::int32_t;
		using real = double;
		using string = std::string;

		explicit value(std::string key, empty = empty{});
		value(std::string key, bool val);
		value(std::string key, int_ val);
		value(std::string key, real val);
		value(std::string key, const char *val);
		value(std::string key, string val);
		value(std::string key, table val);
		value(const value &rhs);
		value(value &&rhs) noexcept;
		~value();

		auto operator=(const value& rhs) -> value& = delete;
		auto operator=(value&& rhs) -> value& = delete;

		explicit operator bool() const noexcept;
		auto has_value() const noexcept -> bool;
		auto operator!() const noexcept -> bool;

		auto key() const noexcept -> const std::string&;

		template <typename T> 
		auto is() const noexcept -> bool;

		template <typename T> 
		std::enable_if_t<
			   std::is_same_v<T, bool>
			|| std::is_same_v<T, int_>
			|| std::is_same_v<T, real>
			|| std::is_same_v<T, string>
			|| std::is_same_v<T, table>
		, const T&> as() const;

		friend auto operator==(const value &lhs, const value &rhs) noexcept -> bool;
		friend auto operator!=(const value &lhs, const value &rhs) noexcept -> bool;

	private:
		struct priv;
		priv *p;
	};

	class const_iterator;

	table();
	table(const table &rhs);
	table(table &&rhs) noexcept;
	~table();

	auto operator=(const table&) -> table& = delete;
	auto operator=(table&&) -> table& = delete;

	auto add(value val) -> table&;

	auto get_unique(string_view name) const -> const value&;
	auto get_last(string_view name) const -> const value&;
	auto operator[](string_view name) const -> const value&;
	auto get_all(string_view name) const -> std::vector<const value*>;

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
auto table::value::is<bool>() const noexcept -> bool;

template <>
auto table::value::is<table::value::int_>() const noexcept -> bool;

template <>
auto table::value::is<table::value::real>() const noexcept -> bool;

template <>
auto table::value::is <table::value::string> () const noexcept -> bool;

template <>
auto table::value::is<table>() const noexcept -> bool;

template <typename T>
auto table::value::is() const noexcept -> bool
{
	return false;
}

inline table::value::value(std::string key, const char* val):
	value{ move(key), string{ val } }
{
}

inline auto table::value::has_value() const noexcept -> bool
{
	return static_cast<bool>(*this);
}

class table::const_iterator
{
public:
	using value_type = value;
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

	auto operator=(const const_iterator &rhs) -> const_iterator&;
	auto operator=(const_iterator &&rhs) noexcept -> const_iterator&;

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
