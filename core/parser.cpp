#include "parser.hpp"
#include "message.hpp"
#include "arena.hpp"

enum cb_result {
	OK = 0,
	ERR = 3 // on_headers_complete reserves 1 and 2
};

inline request::method_s method_from(http_method m)
{
	using M = request::method_s::type_e;
	static constexpr request::method_s
		m_get = { M::GET, "GET"_w },
		m_head = { M::HEAD, "HEAD"_w },
		m_post = { M::POST, "POST"_w };

	switch (m) {
	case HTTP_GET:  return m_get;
	case HTTP_HEAD: return m_head;
	case HTTP_POST: return m_post;
	default:        return { M::OTHER, http_method_str(m) };
	}

	BOOST_UNREACHABLE_RETURN({});
}

inline string_view url_field(const http_parser_url &url,
	http_parser_url_fields f, string_view s)
{
	return url.field_set & (1 << f)
		? s.substr(url.field_data[f].off, url.field_data[f].len)
		: ""_w;
}

struct parser_internal: parser
{
	template<typename cb>
	static int parser_cb(http_parser *p) noexcept
	{
		const http_parser *cp = p;
		context &ctx = *static_cast<context*>(cp->data);
		request &r = *ctx.r;
		static_assert(noexcept(cb::f(cp, ctx, r)),
			"parser callback should be noexcept");
		return cb::f(cp, ctx, r);
	}

	template<typename cb>
	static int data_cb(http_parser *p, const char *at, size_t len) noexcept
	{
		const http_parser *cp = p;
		context &ctx = *static_cast<context*>(cp->data);
		request &r = *ctx.r;
		string_view s{at, len};
		static_assert(noexcept(cb::f(cp, ctx, r, s)),
			"parser callback should be noexcept");
		return cb::f(cp, ctx, r, s);
	}

	static http_parser_settings make_settings();
};

http_parser_settings parser_internal::make_settings()
{
	http_parser_settings s;
	http_parser_settings_init(&s);

	struct on_url
	{
		static auto f(const http_parser *p, context &ctx,
		              request &r, string_view s) noexcept
		{
			auto method = static_cast<http_method>(p->method);
			r.method = method_from(method);

			r.url.all = s;
			http_parser_url url;
			http_parser_url_init(&url);
			auto err = http_parser_parse_url(s.data(), s.length(),
				method == HTTP_CONNECT, &url);
			if (BOOST_UNLIKELY(err)) {
				ctx.error.emplace(response_status::BAD_REQUEST, "bad URL"_w);
				return ERR;
			}
			r.url.path = url_field(url, UF_PATH, s);
			r.url.query = url_field(url, UF_QUERY, s);
			return OK;
		}
	};
	s.on_url = data_cb<on_url>;

	struct on_header_field
	{
		static auto f(const http_parser *, context&,
		              request &r, string_view s) noexcept
		{
			r.headers.emplace_back(s);
			return OK;
		}
	};
	s.on_header_field = data_cb<on_header_field>;

	struct on_header_value
	{
		static auto f(const http_parser *, context &,
		              request &r, string_view s) noexcept
		{
			r.headers.back().value = s;
			return OK;
		}
	};
	s.on_header_value = data_cb<on_header_value>;

	struct on_headers_complete
	{
		static auto f(const http_parser *p, context &ctx,
		              request &r) noexcept
		{
			if (BOOST_UNLIKELY(p->http_major != 1 || p->http_minor > 1)) {
				ctx.error.emplace(response_status::HTTP_VERSION_NOT_SUPPORTED);
				return ERR;
			}
			r.http_version = static_cast<message::http_version_type>(p->http_minor);
			return OK;
		}
	};
	s.on_headers_complete = parser_cb<on_headers_complete>;

	struct on_body
	{
		static auto f(const http_parser *, context &,
		              request &r, string_view s) noexcept
		{
			r.body.emplace_back(s);
			return OK;
		}
	};
	s.on_body = data_cb<on_body>;

	struct on_message_complete
	{
		static auto f(const http_parser *p, context &ctx,
		              request &r) noexcept
		{
			r.keep_alive = http_should_keep_alive(p) != 0;
			r.content_length = static_cast<size_t>(p->content_length);
			ctx.comp = true;
			return OK;
		}
	};
	s.on_message_complete = parser_cb<on_message_complete>;

	return s;
}

const http_parser_settings settings = parser_internal::make_settings();

void parser::reset(request &req, arena &a) noexcept
{
	http_parser_init(&p, HTTP_REQUEST);
	ctx.r = &req;
	ctx.a = &a;
	ctx.error = boost::none;
	ctx.comp = false;
	p.data = &ctx;
}

auto parser::parse_chunk(buffer buf) noexcept -> result
{
	using namespace boost::asio;

	auto data = buffer_cast<const char*>(buf);
	auto len = buffer_size(buf);
	auto nparsed = http_parser_execute(&p, &settings, data, len);

	auto err = HTTP_PARSER_ERRNO(&p);
	if (BOOST_UNLIKELY(err)) {
		return ctx.error
			? *ctx.error
			: http_error{response_status::BAD_REQUEST,
				http_errno_description(err)};
	}

	if (p.upgrade) {
		//TODO
	}

	return buf + nparsed;
}