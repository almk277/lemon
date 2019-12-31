#pragma once
#include "config.hpp"
#include <boost/filesystem/path.hpp>
#include <memory>
#include <vector>

namespace config
{
class syntax_error : public error
{
public:
	using position = std::size_t;

	syntax_error(position where, const std::string &what):
		error{ what },
		pos{ where }
	{}

	auto where() const noexcept -> position { return pos; }

private:
	const position pos;
};

class text_view
{
public:
	explicit text_view(string_view data, const std::string &filename = {});

	struct priv;
	const std::shared_ptr<priv> p;
};

class text: public text_view
{
public:
	explicit text(std::vector<char> data, const std::string& filename = {});
private:
	const std::vector<char> data; //TODO c++17 string
};

class file : public text
{
public:
	explicit file(const boost::filesystem::path &path);
};

auto parse(std::shared_ptr<text_view> text) -> table;
}
