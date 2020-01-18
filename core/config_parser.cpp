#define BOOST_SPIRIT_X3_NO_FILESYSTEM
#include "config_parser.hpp"
#include "config.hpp"
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#ifdef _MSC_VER
#pragma warning(push)
// seems to be fixed in boost 1.70
#pragma warning(disable: 4521)
#endif
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>
#include <boost/fusion/adapted/struct/adapt_struct.hpp>
#include <utility>
#include <vector>
#include <sstream>
#include <fstream>

/*
	value ::= bool | int | real | string | { table }
	stmt  ::= string = value
	table ::= stmt*
 */

namespace
{
namespace ast
{
namespace x3 = boost::spirit::x3;
struct table;

struct key : x3::position_tagged
{
	std::string name;
};

struct value : x3::variant<
		config::boolean,
		config::real,
		config::integer,
		config::string,
		x3::forward_ast<table>
	>,
	x3::position_tagged
{
	using base_type::operator=;
};

struct stmt : x3::position_tagged
{
	ast::key key;
	ast::value val;
};

struct table : x3::position_tagged
{
	std::vector<stmt> entries;
};
}

namespace grammar
{
using namespace boost::spirit::x3;

struct key_id : annotate_on_success {};
struct value_id : annotate_on_success {};
struct stmt_id : annotate_on_success {};
struct table_id : annotate_on_success {};

const rule<key_id, ast::key> key = "key";
const rule<value_id, ast::value> value = "value";
const rule<stmt_id, ast::stmt>  stmt = "statement";
const rule<table_id, ast::table> table = "table";

const auto bool_kw = lexeme[bool_ >> !(graph - '=')];
const auto single_quoted_string = lexeme['\'' >> *~char_('\'') >> '\''];
const auto unquoted_string = lexeme[+graph] - bool_kw - '{' - '}';
const auto string = single_quoted_string | unquoted_string;

template <typename T>
struct real_policy : strict_real_policies<T>
{
	template <typename I, typename A>
	static auto parse_nan(I&, const I&, A&) { return false; }
	template <typename I, typename A>
	static auto parse_inf(I&, const I&, A&) { return false; }
};
const auto real = real_parser<double, real_policy<double>>{};

static_assert(std::is_same_v<decltype(real)::attribute_type, config::real>);
static_assert(std::is_same_v<decltype(int32)::attribute_type, config::integer>);

const auto key_def = string;
const auto value_def =
	  bool_kw
	| real
	| int32
	| '{' > table > '}'
	| string
	;
const auto stmt_def = key > '=' > value;
const auto table_def = *stmt;

BOOST_SPIRIT_DEFINE(key, value, stmt, table);
}

using iterator = string_view::const_iterator;

auto read(const boost::filesystem::path &path)
{
	auto name = path.string();
	std::ifstream f{ name,  std::ios::in | std::ios::binary | std::ios::ate };
	if (!f.is_open())
		throw std::runtime_error{ "can't load config: " + name };

	auto size = f.tellg();
	f.seekg(0, std::ios::beg);
	std::string data(size, 0);
	if (!f.read(data.data(), size))
		throw std::runtime_error{ "can't read config: " + name };

	return data;
}
}

BOOST_FUSION_ADAPT_STRUCT(ast::key, name);
BOOST_FUSION_ADAPT_STRUCT(ast::stmt, key, val);
BOOST_FUSION_ADAPT_STRUCT(ast::table, entries);

//TODO proper tab handling
struct config::text_view::priv
{
	priv(string_view data, const std::string &filename):
		data{ data },
		error_handler{ data.begin(), data.end(), error_stream, filename }
	{
	}

	auto make_error_string(const grammar::position_tagged &where, const std::string& what) const
	{
		error_handler(where, what);
		return error_stream.str();
	}

	auto make_error_string(iterator where, const std::string& what) const
	{
		error_handler(where, what);
		return error_stream.str();
	}

	auto make_error(iterator where, const std::string &msg) const
	{
		return syntax_error{ static_cast<syntax_error::position>(where - data.begin()),
			make_error_string(where, msg) };
	}

	const string_view data;
	std::stringstream error_stream{ std::ios::out };
	grammar::error_handler<iterator> error_handler;
	ast::table ast;
};

config::text_view::text_view(string_view data, const std::string &filename):
	p{ std::make_shared<priv>(data, filename) }
{
}

config::text::text(std::string data, const std::string& filename):
	text_view{ string_view{ data.data(), data.size() }, filename },
	data{ move(data) }
{
}

config::file::file(const boost::filesystem::path &path):
	text{ read(path), path.string() }
{
}

namespace config
{
namespace
{
class property_error_handler : public property::error_handler
{
public:
	property_error_handler(std::shared_ptr<const text_view> text,
		const grammar::position_tagged &key, const grammar::position_tagged &value) :
		key{ key },
		value{ value },
		text{ move(text) }
	{
	}

	auto key_error(const std::string& msg) const -> std::string override
	{
		return text->p->make_error_string(key, msg);
	}

	auto value_error(const std::string& msg) const -> std::string override
	{
		return text->p->make_error_string(value, msg);
	}

private:
	const grammar::position_tagged &key;
	const grammar::position_tagged &value;
	const std::shared_ptr<const text_view> text;
};

class empty_property_error_handler : public property::error_handler
{
public:
	empty_property_error_handler(std::shared_ptr<const text_view> text,
		const grammar::position_tagged &table) :
		table{ table },
		text{ move(text) }
	{
	}

	auto key_error(const std::string& msg) const -> std::string override
	{
		return text->p->make_error_string(table, msg);
	}

	auto value_error(const std::string& msg) const -> std::string override
	{
		return text->p->make_error_string(table, msg);
	}

private:
	const grammar::position_tagged &table;
	const std::shared_ptr<const text_view> text;
};

class table_error_handler : public table::error_handler
{
public:
	table_error_handler(std::shared_ptr<const text_view> text,
		const grammar::position_tagged &table):
		table{ table },
		text{ move(text) }
	{}

	auto make_error_handler() -> std::unique_ptr<property::error_handler> override
	{
		return std::make_unique<empty_property_error_handler>(text, table);
	}

private:
	const grammar::position_tagged &table;
	const std::shared_ptr<const text_view> text;
};

auto make_table(const ast::table& t, std::shared_ptr<const text_view> text) -> table;

struct property_visitor : boost::static_visitor<property>
{
	property_visitor(const ast::stmt &stmt, std::shared_ptr<const text_view> text):
		eh{ std::make_unique<property_error_handler>(text, stmt.key, stmt.val) },
		key{ stmt.key.name },
		text{ text }
	{}

	auto operator()(bool v) { return property{ move(eh), move(key), v }; }
	auto operator()(real v) { return property{ move(eh), move(key), v }; }
	auto operator()(integer v) { return property{ move(eh), move(key), v }; }
	auto operator()(const ast::table &t)
	{
		return property{ move(eh), move(key), make_table(t, move(text)) };
	}
	auto operator()(const std::string &v)
	{
		return property{ move(eh), move(key), v };
	}
private:
	std::unique_ptr<property_error_handler> eh;
	std::string key;
	std::shared_ptr<const text_view> text;
};

auto make_table(const ast::table& t, std::shared_ptr<const text_view> text) -> table
{
	table res{ std::make_unique<table_error_handler>(text, t) };
	for (const auto &stmt : t.entries) {
		property_visitor visitor{ stmt, text };
		res.add(apply_visitor(visitor, stmt.val));
	}

	return res;
}

}
}

auto config::parse(std::shared_ptr<text_view> text) -> table
{
	auto &txt = *text->p;
	auto begin = txt.data.begin();
	auto end = txt.data.end();

	auto parser = grammar::with<grammar::error_handler_tag>(std::ref(txt.error_handler))
	[
		grammar::table
	];

	try {
		auto parsed = phrase_parse(begin, end, parser, grammar::space, txt.ast);
		BOOST_ASSERT(parsed);
	} catch (grammar::expectation_failure<iterator> &e) {
		throw txt.make_error(e.where(), "Error! Expecting " + e.which() + " here:");
	}

	auto consumed = begin == end;
	if (!consumed)
		throw txt.make_error(begin, "can't parse:");

	return make_table(txt.ast, move(text));
}
