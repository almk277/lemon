#include "parser.hpp"
#include "message.hpp"
#include <boost/algorithm/string/case_conv.hpp>

namespace
{
enum cb_result
{
	OK = 0,
	ERR = 3 // on_headers_complete reserves 1 and 2
};

const auto &header_locale = std::locale::classic();

request::method_s method_from(http_method m)
{
	using M = request::method_s::type_e;
	static constexpr request::method_s
		m_get = { M::GET, "GET"sv },
		m_head = { M::HEAD, "HEAD"sv },
		m_post = { M::POST, "POST"sv };

	switch (m) {
	case HTTP_GET:  return m_get;
	case HTTP_HEAD: return m_head;
	case HTTP_POST: return m_post;
	default:        return { M::OTHER, http_method_str(m) };
	}

	BOOST_UNREACHABLE_RETURN({});
}

void prolong(string_view &original, string_view added)
{
	BOOST_ASSERT(original.data() + original.size() == added.data());
	original = { original.data(), original.size() + added.size() };
}

string_view url_field(const http_parser_url &url,
	http_parser_url_fields f, string_view s)
{
	return url.field_set & (1 << f)
		? s.substr(url.field_data[f].off, url.field_data[f].len)
		: ""sv;
}

struct parser_internal: parser
{
	template<typename cb>
	static int parser_cb(http_parser *p) noexcept
	{
		context &ctx = *static_cast<context*>(p->data);
		request &r = *ctx.r;
		static_assert(noexcept(cb::f(p, ctx, r)),
			"parser callback should be noexcept");
		return cb::f(p, ctx, r);
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
			if (r.url.all.empty())
				r.url.all = s;
			else
				prolong(r.url.all, s);
			return OK;
		}
	};
	s.on_url = data_cb<on_url>;

	struct on_header_field
	{
		static auto f(const http_parser *, context &ctx,
		              request &r, string_view s) noexcept
		{
			if (BOOST_LIKELY(ctx.hdr_state == context::hdr::VAL)) {
				r.headers.emplace_back(s, string_view{});
				ctx.hdr_state = context::hdr::KEY;
			} else {
				prolong(r.headers.back().name, s);
			}
			return OK;
		}
	};
	s.on_header_field = data_cb<on_header_field>;

	struct on_header_value
	{
		static auto f(const http_parser *, context &ctx,
		              request &r, string_view s) noexcept
		{
			if (BOOST_LIKELY(ctx.hdr_state == context::hdr::KEY)) {
				r.headers.back().value = s;
				ctx.hdr_state = context::hdr::VAL;
			} else {
				prolong(r.headers.back().value, s);
			}
			return OK;
		}
	};
	s.on_header_value = data_cb<on_header_value>;

	struct on_headers_complete
	{
		static auto f(const http_parser *p, context &ctx, request &r) noexcept
		{
			if (BOOST_UNLIKELY(p->http_major != 1 || p->http_minor > 1)) {
				ctx.error.emplace(response::status::HTTP_VERSION_NOT_SUPPORTED);
				return ERR;
			}
			r.http_version = static_cast<message::http_version_type>(p->http_minor);

			auto method = static_cast<http_method>(p->method);
			r.method = method_from(method);

			http_parser_url url;
			http_parser_url_init(&url);
			auto err = http_parser_parse_url(r.url.all.data(), r.url.all.length(),
				method == HTTP_CONNECT, &url);
			if (BOOST_UNLIKELY(err)) {
				ctx.error.emplace(response::status::BAD_REQUEST, "bad URL"sv);
				return ERR;
			}
			r.url.path = url_field(url, UF_PATH, r.url.all);
			r.url.query = url_field(url, UF_QUERY, r.url.all);

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
		static auto f(http_parser *p, context &ctx,
		              request &r) noexcept
		{
			r.keep_alive = http_should_keep_alive(p) != 0;
			r.content_length = static_cast<size_t>(p->content_length);
			ctx.complete = true;
			http_parser_pause(p, 1);
			return OK;
		}
	};
	s.on_message_complete = parser_cb<on_message_complete>;

	return s;
}

const http_parser_settings settings = parser_internal::make_settings();
}

void parser::reset(request &req) noexcept
{
	http_parser_init(&p, HTTP_REQUEST);
	ctx.r = &req;
	ctx.hdr_state = context::hdr::VAL;
	ctx.error = boost::none;
	ctx.complete = false;
	p.data = &ctx;
}

auto parser::parse_chunk(string_view chunk) noexcept -> result
{
	auto nparsed = http_parser_execute(&p, &settings, chunk.data(), chunk.size());

	auto err = HTTP_PARSER_ERRNO(&p);
	switch (err)
	{
	case HPE_OK:
		break;
	case HPE_PAUSED:
		http_parser_pause(&p, 0);
		break;
	default:
		return ctx.error.value_or_eval([err] {
			return http_error{ response::status::BAD_REQUEST, http_errno_description(err) };
		});
	}

	if (p.upgrade) {
		//TODO
	}

	if (ctx.complete)
		return complete_request{ chunk.substr(nparsed) };

	BOOST_ASSERT(nparsed == chunk.size());
	return incomplete_request{};
}

void parser::finalize(request &req)
{
	auto allocator = req.a.make_allocator<char>("lowercase header");
	for (auto &hdr : req.headers) {
		auto lc_length = hdr.name.length();
		auto lc_begin = allocator.allocate(lc_length);
		boost::to_lower_copy(lc_begin, hdr.name, header_locale);
		hdr.lowercase_name = {lc_begin, lc_length};
	}
}
