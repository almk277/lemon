#define BOOST_SPIRIT_X3_NO_FILESYSTEM
#include "config_parser.hpp"
#include "config.hpp"
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>
#include <boost/fusion/adapted/struct/adapt_struct.hpp>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

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
struct Table;

struct Key : x3::position_tagged
{
	std::string name;
};

struct Value : x3::variant<
		config::Boolean,
		config::Real,
		config::Integer,
		config::String,
		x3::forward_ast<Table>
	>,
	x3::position_tagged
{
	using base_type::operator=;
};

struct Stmt : x3::position_tagged
{
	Key key;
	Value val;
};

struct Table : x3::position_tagged
{
	std::vector<Stmt> entries;
};
}

namespace grammar
{
using namespace boost::spirit::x3;

struct KeyId : annotate_on_success {};
struct ValueId : annotate_on_success {};
struct StmtId : annotate_on_success {};
struct TableId : annotate_on_success {};

const rule<KeyId, ast::Key> key = "key";
const rule<ValueId, ast::Value> value = "value";
const rule<StmtId, ast::Stmt>  stmt = "statement";
const rule<TableId, ast::Table> table = "table";

const auto bool_kw = lexeme[bool_ >> !(graph - '=')];
const auto single_quoted_string = lexeme['\'' >> *~char_('\'') >> '\''];
const auto unquoted_string = lexeme[+graph] - bool_kw - '{' - '}';
const auto string = single_quoted_string | unquoted_string;

template <typename T>
struct RealPolicy : strict_real_policies<T>
{
	template <typename I, typename A>
	static auto parse_nan(I&, const I&, A&) { return false; }
	template <typename I, typename A>
	static auto parse_inf(I&, const I&, A&) { return false; }
};
const auto real = real_parser<double, RealPolicy<double>>{};

static_assert(std::is_same_v<decltype(real)::attribute_type, config::Real>);
static_assert(std::is_same_v<decltype(int32)::attribute_type, config::Integer>);

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

using Iterator = string_view::const_iterator;

auto read(const std::filesystem::path& path)
{
	std::ifstream f{ path,  std::ios::in | std::ios::binary | std::ios::ate };
	if (!f.is_open())
		throw std::runtime_error{ "can't load config: " + path.string() };

	auto size = f.tellg();
	f.seekg(0, std::ios::beg);
	std::string data(size, 0);
	if (!f.read(data.data(), size))
		throw std::runtime_error{ "can't read config: " + path.string() };

	return data;
}
}

BOOST_FUSION_ADAPT_STRUCT(ast::Key, name);
BOOST_FUSION_ADAPT_STRUCT(ast::Stmt, key, val);
BOOST_FUSION_ADAPT_STRUCT(ast::Table, entries);

namespace config
{
//TODO proper tab handling
struct TextView::Priv
{
	Priv(string_view data, const std::string& filename):
		data{ data },
		error_handler{ data.begin(), data.end(), error_stream, filename }
	{
	}

	auto make_error_string(const grammar::position_tagged& where, const std::string& what) const
	{
		error_handler(where, what);
		return error_stream.str();
	}

	auto make_error_string(Iterator where, const std::string& what) const
	{
		error_handler(where, what);
		return error_stream.str();
	}

	auto make_error(Iterator where, const std::string& msg) const
	{
		return SyntaxError{ static_cast<SyntaxError::Position>(where - data.begin()),
			make_error_string(where, msg) };
	}

	const string_view data;
	std::stringstream error_stream{ std::ios::out };
	grammar::error_handler<Iterator> error_handler;
	ast::Table ast;
};

TextView::TextView(string_view data, const std::string& filename):
	p{ std::make_shared<Priv>(data, filename) }
{
}

Text::Text(std::string data, const std::string& filename):
	TextView{ string_view{ data.data(), data.size() }, filename },
	data{ move(data) }
{
}

File::File(const std::filesystem::path& path):
	Text{ read(path), path.string() }
{
}

namespace
{
class PropertyErrorHandler : public Property::ErrorHandler
{
public:
	PropertyErrorHandler(std::shared_ptr<const TextView> text,
		const grammar::position_tagged& key, const grammar::position_tagged& value) :
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
	const grammar::position_tagged& key;
	const grammar::position_tagged& value;
	const std::shared_ptr<const TextView> text;
};

class EmptyPropertyErrorHandler : public Property::ErrorHandler
{
public:
	EmptyPropertyErrorHandler(std::shared_ptr<const TextView> text,
		const grammar::position_tagged& table) :
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
	const grammar::position_tagged& table;
	const std::shared_ptr<const TextView> text;
};

class TableErrorHandler : public Table::ErrorHandler
{
public:
	TableErrorHandler(std::shared_ptr<const TextView> text,
		const grammar::position_tagged& table):
		table{ table },
		text{ move(text) }
	{}

	auto make_error_handler() -> std::unique_ptr<Property::ErrorHandler> override
	{
		return std::make_unique<EmptyPropertyErrorHandler>(text, table);
	}

private:
	const grammar::position_tagged& table;
	const std::shared_ptr<const TextView> text;
};

auto make_table(const ast::Table& t, const std::shared_ptr<const TextView>& text) -> Table;

struct PropertyVisitor : boost::static_visitor<Property>
{
	PropertyVisitor(const ast::Stmt& stmt, std::shared_ptr<const TextView> text):
		eh{ std::make_unique<PropertyErrorHandler>(text, stmt.key, stmt.val) },
		key{ stmt.key.name },
		text{ text }
	{}

	auto operator()(bool v) { return Property{ move(eh), move(key), v }; }
	auto operator()(Real v) { return Property{ move(eh), move(key), v }; }
	auto operator()(Integer v) { return Property{ move(eh), move(key), v }; }
	auto operator()(const ast::Table& t)
	{
		return Property{ move(eh), move(key), make_table(t, move(text)) };
	}
	auto operator()(const std::string& v)
	{
		return Property{ move(eh), move(key), v };
	}
private:
	std::unique_ptr<PropertyErrorHandler> eh;
	std::string key;
	std::shared_ptr<const TextView> text;
};

auto make_table(const ast::Table& t, const std::shared_ptr<const TextView>& text) -> Table
{
	Table res{ std::make_unique<TableErrorHandler>(text, t) };
	for (const auto& stmt : t.entries) {
		PropertyVisitor visitor{ stmt, text };
		res.add(apply_visitor(visitor, stmt.val));
	}

	return res;
}

}
}

auto config::parse(std::shared_ptr<TextView> text) -> Table
{
	auto& txt = *text->p;
	auto begin = txt.data.begin();
	auto end = txt.data.end();

	auto parser = grammar::with<grammar::error_handler_tag>(std::ref(txt.error_handler))
	[
		grammar::table
	];

	try {
		auto parsed = phrase_parse(begin, end, parser, grammar::space, txt.ast);
		BOOST_ASSERT(parsed);
	} catch (grammar::expectation_failure<Iterator>& e) {
		throw txt.make_error(e.where(), "Error! Expecting " + e.which() + " here:");
	}

	auto consumed = begin == end;
	if (!consumed)
		throw txt.make_error(begin, "can't parse:");

	return make_table(txt.ast, move(text));
}