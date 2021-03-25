#include "http_parser_.hpp"
#include "http_message.hpp"
#include <boost/algorithm/string/case_conv.hpp>

namespace http
{
namespace
{
enum CallbackResult
{
	ok = 0,
	error = 3 // on_headers_complete reserves 1 and 2
};

const auto& header_locale = std::locale::classic();

auto method_from(http_method m) -> Request::Method
{
	using M = Request::Method::Type;
	static constexpr Request::Method
		m_get = { M::get, "GET"sv },
		m_head = { M::head, "HEAD"sv },
		m_post = { M::post, "POST"sv };

	switch (m) {
	case HTTP_GET:  return m_get;
	case HTTP_HEAD: return m_head;
	case HTTP_POST: return m_post;
	default:        return { M::other, http_method_str(m) };
	}

	BOOST_UNREACHABLE_RETURN({});
}

auto prolong(string_view& original, string_view added)
{
	BOOST_ASSERT(original.data() + original.size() == added.data());
	original = { original.data(), original.size() + added.size() };
}

auto url_field(const http_parser_url& url,
               http_parser_url_fields f, string_view s)
{
	return url.field_set & (1 << f)
		? s.substr(url.field_data[f].off, url.field_data[f].len)
		: ""sv;
}

struct ParserInternal: Parser
{
	template<typename cb>
	static auto parser_cb(http_parser* p) noexcept -> int
	{
		Context& ctx = *static_cast<Context*>(p->data);
		Request& r = *ctx.r;
		static_assert(noexcept(cb::f(p, ctx, r)),
			"parser callback should be noexcept");
		return cb::f(p, ctx, r);
	}

	template<typename cb>
	static auto data_cb(http_parser* p, const char* at, size_t len) noexcept -> int
	{
		Context& ctx = *static_cast<Context*>(p->data);
		Request& r = *ctx.r;
		string_view s{at, len};
		static_assert(noexcept(cb::f(p, ctx, r, s)),
			"parser callback should be noexcept");
		return cb::f(p, ctx, r, s);
	}

	static auto make_work_settings() -> http_parser_settings;
	static auto make_drop_settings();
};

auto ParserInternal::make_work_settings() -> http_parser_settings
{
	http_parser_settings s;
	http_parser_settings_init(&s);

	struct OnUrl
	{
		static auto f(http_parser* p, Context& ctx,
		              Request& r, string_view s) noexcept
		{
			if (r.url.all.empty())
				r.url.all = s;
			else
				prolong(r.url.all, s);
			
			auto method = static_cast<http_method>(p->method);
			r.method = method_from(method);

			http_parser_url url;
			http_parser_url_init(&url);
			auto err = http_parser_parse_url(r.url.all.data(), r.url.all.length(),
				method == HTTP_CONNECT, &url);
			if (BOOST_UNLIKELY(err)) {
				ctx.error.emplace(Response::Status::bad_request, "bad URL"sv);
				return error;
			}
			r.url.path = url_field(url, UF_PATH, r.url.all);
			r.url.query = url_field(url, UF_QUERY, r.url.all);

			http_parser_pause(p, 1);
			ctx.state = State::request_line;
			
			return ok;
		}
	};
	s.on_url = data_cb<OnUrl>;

	struct OnHeaderField
	{
		static auto f(const http_parser*, Context& ctx,
		              Request& r, string_view s) noexcept
		{
			if (BOOST_LIKELY(ctx.hdr_state == HeaderState::value)) {
				r.headers.emplace_back(s, string_view{});
				ctx.hdr_state = HeaderState::key;
			} else {
				prolong(r.headers.back().name, s);
			}
			return ok;
		}
	};
	s.on_header_field = data_cb<OnHeaderField>;

	struct OnHeaderValue
	{
		static auto f(const http_parser*, Context& ctx,
		              Request& r, string_view s) noexcept
		{
			if (BOOST_LIKELY(ctx.hdr_state == HeaderState::key)) {
				r.headers.back().value = s;
				ctx.hdr_state = HeaderState::value;
			} else {
				prolong(r.headers.back().value, s);
			}
			return ok;
		}
	};
	s.on_header_value = data_cb<OnHeaderValue>;

	struct OnHeadersComplete
	{
		static auto f(const http_parser* p, Context& ctx, Request& r) noexcept
		{
			if (BOOST_UNLIKELY(p->http_major != 1 || p->http_minor > 1)) {
				ctx.error.emplace(Response::Status::http_version_not_supported);
				return error;
			}
			r.http_version = static_cast<Message::ProtocolVersion>(p->http_minor);

			return ok;
		}
	};
	s.on_headers_complete = parser_cb<OnHeadersComplete>;

	struct OnBody
	{
		static auto f(const http_parser*, Context&,
		              Request& r, string_view s) noexcept
		{
			r.body.emplace_back(s);
			return ok;
		}
	};
	s.on_body = data_cb<OnBody>;

	struct OnMessageComplete
	{
		static auto f(http_parser* p, Context& ctx,
		              Request& r) noexcept
		{
			r.keep_alive = http_should_keep_alive(p) != 0;
			r.content_length = static_cast<size_t>(p->content_length);
			ctx.state = State::body;
			http_parser_pause(p, 1);
			return ok;
		}
	};
	s.on_message_complete = parser_cb<OnMessageComplete>;

	return s;
}

auto ParserInternal::make_drop_settings()
{
	http_parser_settings s;
	http_parser_settings_init(&s);

	struct OnMessageComplete
	{
		static auto f(http_parser* p, Context& ctx,
			Request& r) noexcept
		{
			ctx.state = State::body;
			http_parser_pause(p, 1);
			return ok;
		}
	};
	s.on_message_complete = parser_cb<OnMessageComplete>;

	return s;
}

const auto work_settings = ParserInternal::make_work_settings();
const auto drop_settings = ParserInternal::make_drop_settings();
}

auto Parser::reset(Request& req, bool& drop_mode) noexcept -> void
{
	http_parser_init(&p, HTTP_REQUEST); // TODO check out if we need to do this every time
	ctx.r = &req;
	ctx.drop_mode = &drop_mode;
	ctx.state = State::start;
	ctx.hdr_state = HeaderState::value;
	ctx.error = boost::none;
	p.data = &ctx;
}

auto Parser::parse_chunk(string_view chunk) noexcept -> Result
{
	auto settings = *ctx.drop_mode ? &drop_settings : &work_settings;
	auto nparsed = http_parser_execute(&p, settings, chunk.data(), chunk.size());

	auto err = HTTP_PARSER_ERRNO(&p);
	switch (err)
	{
	case HPE_OK:
		break;
	case HPE_PAUSED:
		http_parser_pause(&p, 0);
		break;
	default:
		return { ctx.error.value_or_eval([err] {
			return Error{ Response::Status::bad_request, http_errno_description(err) };
		}), string_view{} };
	}

	if (p.upgrade) {
		//TODO
	}

	if (ctx.state == State::request_line)
		return { RequestLine{}, chunk.substr(nparsed) };
	if (ctx.state == State::body)
		return { CompleteRequest{}, chunk.substr(nparsed) };

	BOOST_ASSERT(nparsed == chunk.size());
	return { IncompleteRequest{}, string_view{} };
}

auto Parser::finalize(Request& req) const -> void
{
	if (*ctx.drop_mode)
		return;
	
	auto allocator = req.a.make_allocator<char>("lowercase header");
	for (auto& hdr : req.headers) {
		auto lc_length = hdr.name.length();
		auto lc_begin = allocator.allocate(lc_length);
		boost::to_lower_copy(lc_begin, hdr.name, header_locale);
		hdr.lowercase_name = { lc_begin, lc_length };
	}
}
}