#define BOOST_SPIRIT_X3_NO_FILESYSTEM
#include "config_parser.hpp"
#include "options.hpp"
#include "config.hpp"
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>
#include <boost/fusion/adapted/struct/adapt_struct.hpp>
#include <boost/static_assert.hpp>
#include <vector>
#include <utility>
#include <sstream>
#include <fstream>

/*
	value ::= bool | int | real | string | { table }
	stmt  ::= string = value
	table ::= stmt*
 */

using int_ = table::value::int_;
using real = table::value::real;

config::error::error(std::size_t position, const std::string &msg):
	runtime_error{ msg },
	position{ position }
{
}

namespace
{
namespace ast
{
namespace x3 = boost::spirit::x3;
struct table;

struct value : x3::variant<
		bool,
		real,
		int_,
		x3::forward_ast<table>,
		std::string
	>,
	x3::position_tagged
{
	using base_type::operator=;
};

struct stmt : x3::position_tagged
{
	std::string key;
	value val;
};

struct table : x3::position_tagged
{
	std::vector<stmt> entries;
};
}

namespace grammar
{
using namespace boost::spirit::x3;

struct value_id : annotate_on_success {};
struct stmt_id : annotate_on_success {};
struct table_id : annotate_on_success {};

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

BOOST_STATIC_ASSERT(std::is_same_v<decltype(real)::attribute_type, table::value::real>);
BOOST_STATIC_ASSERT(std::is_same_v<decltype(int32)::attribute_type, table::value::int_>);

const auto value_def =
	  bool_kw
	| real
	| int32
	| '{' > table > '}'
	| string
	;
const auto stmt_def = string > '=' > value;
const auto table_def = *stmt;

BOOST_SPIRIT_DEFINE(value, stmt, table);
}

using iterator = string_view::const_iterator;
using positions_type = grammar::position_cache<std::vector<iterator>>;
using error_handler_type = grammar::error_handler<iterator>;

table make_table(const ast::table &t, const positions_type &positions);

struct value_visitor: boost::static_visitor<table::value>
{
	value_visitor(std::string key, const positions_type &positions):
		key{ move(key) },
		positions{ positions }
	{}

	auto operator()(bool v) { return table::value{ move(key), v }; }
	auto operator()(real v) { return table::value{ move(key), v }; }
	auto operator()(int_ v) { return table::value{ move(key), v }; }
	auto operator()(const ast::table &t) { return table::value{ move(key), make_table(t, positions) }; }
	auto operator()(const std::string &v) { return table::value{ move(key), v }; }
private:
	std::string key;
	const positions_type &positions;
};

table make_table(const ast::table &t, const positions_type &positions)
{
	table res;
	for (auto &stmt: t.entries) {
		value_visitor visitor{ stmt.key, positions };
		auto pos = positions.position_of(stmt.val);
		res.add(apply_visitor(visitor, stmt.val));
	}

	return res;
}

auto make_error(string_view text, iterator where, const error_handler_type &error_handler,
	const std::stringstream &stream, const std::string &msg)
{
	error_handler(where, msg);
	return config::error{ static_cast<std::size_t>(where - text.begin()), stream.str() };
}
}

BOOST_FUSION_ADAPT_STRUCT(ast::stmt, key, val);
BOOST_FUSION_ADAPT_STRUCT(ast::table, entries);

table config::parse(string_view text)
{
	ast::table data;
	auto begin = text.begin();
	auto end = text.end();

	std::stringstream error_stream{ std::ios::out };
	error_handler_type error_handler{ begin, end, error_stream };
	auto parser = grammar::with<grammar::error_handler_tag>(std::ref(error_handler))
	[
		grammar::table
	];

	try {
		auto parsed = phrase_parse(begin, end, parser, grammar::space, data);
		BOOST_ASSERT(parsed);
	} catch (grammar::expectation_failure<iterator> &e) {
		throw make_error(text, e.where(), error_handler, error_stream, 
			"Error! Expecting " + e.which() + " here:");
	}

	auto consumed = begin == end;
	if (!consumed)
		throw make_error(text, begin, error_handler, error_stream, "can't parse:");

	return make_table(data, error_handler.get_position_cache());
}

table config::parse_file(const std::string &filename)
{
	std::ifstream f{filename,  std::ios::in | std::ios::binary | std::ios::ate };
	if (!f.is_open())
		throw std::runtime_error{ "can't load config" };

	auto filesize = f.tellg();
	std::vector<char> filedata(filesize);
	f.seekg(0, std::ios::beg);
	if (!f.read(filedata.data(), filesize))
		throw std::runtime_error{ "can't read config" };

	string_view text{ &filedata.front(), filedata.size() };
	return parse(text);
}
