#pragma once
#include "config.hpp"
#include <ostream>

namespace config
{
auto operator<<(std::ostream& stream, const Property& p) -> std::ostream&;
auto operator<<(std::ostream& stream, const Property::EmptyType& e) -> std::ostream&;
auto operator<<(std::ostream& stream, const Table& t) -> std::ostream&;

namespace test
{
class PropertyErrorHandler : public Property::ErrorHandler
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

class TableErrorHandler : public Table::ErrorHandler
{
	struct empty_property_error_handler : Property::ErrorHandler
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
	auto make_error_handler() -> std::unique_ptr<Property::ErrorHandler> override
	{
		return std::make_unique<empty_property_error_handler>();
	}
};

template <typename T>
auto prop(std::string key, std::shared_ptr<T>&& value)
{
	return Property{ std::make_unique<PropertyErrorHandler>(), move(key), std::move(*value) };
}

template <typename T>
auto prop(std::string key, T value)
{
	return Property{ std::make_unique<PropertyErrorHandler>(), move(key), std::move(value) };
}

inline auto operator<<(std::shared_ptr<Table> t, Property&& p)
{
	t->add(std::move(p));
	return t;
}

inline auto operator<<(Table t, Property&& p)
{
	t.add(std::move(p));
	return t;
}
}
}