#pragma once
#include "config.hpp"
#include <boost/filesystem/path.hpp>
#include <memory>
#include <string>

namespace config
{
class SyntaxError : public Error
{
public:
	using Position = std::size_t;

	SyntaxError(Position where, const std::string &what):
		Error{ what },
		pos{ where }
	{}

	auto where() const noexcept -> Position { return pos; }

private:
	const Position pos;
};

class TextView
{
public:
	explicit TextView(string_view data, const std::string &filename = {});

	struct Priv;
	const std::shared_ptr<Priv> p;
};

class Text: public TextView
{
public:
	explicit Text(std::string data, const std::string& filename = {});
private:
	const std::string data;
};

class File : public Text
{
public:
	explicit File(const boost::filesystem::path &path);
};

auto parse(std::shared_ptr<TextView> text) -> Table;
}