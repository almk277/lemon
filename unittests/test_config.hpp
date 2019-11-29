#pragma once
#include "config.hpp"
#include <ostream>

namespace config
{
auto operator<<(std::ostream& stream, const property& p) -> std::ostream&;
auto operator<<(std::ostream& stream, const property::empty_type& e) -> std::ostream&;
auto operator<<(std::ostream& stream, const table& t) -> std::ostream&;

namespace test
{
class property_error_handler : public property::error_handler
{
public:
	auto key_error(const std::string& msg) const -> std::string override
	{
		return "";
	}

	auto value_error(const std::string& msg) const->std::string override
	{
		return "";
	}
};

class table_error_handler : public table::error_handler
{
	struct empty_property_error_handler : property::error_handler
	{
		auto key_error(const std::string& msg) const -> std::string override
		{
			return "";
		}

		auto value_error(const std::string& msg) const->std::string override
		{
			return "";
		}
	};

public:
	auto make_error_handler() -> std::unique_ptr<property::error_handler> override
	{
		return std::make_unique<empty_property_error_handler>();
	}
};

template <typename T>
auto prop(std::string key, std::shared_ptr<T> &&value)
{
	return property{ std::make_unique<property_error_handler>(), move(key), std::move(*value) };
}

template <typename T>
auto prop(std::string key, T value)
{
	return property{ std::make_unique<property_error_handler>(), move(key), std::move(value) };
}

inline auto operator<<(std::shared_ptr<table> t, property &&p)
{
	t->add(std::move(p));
	return t;
}

inline auto operator<<(table t, property &&p)
{
	t.add(std::move(p));
	return t;
}
}
}