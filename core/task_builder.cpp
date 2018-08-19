#include "task_builder.hpp"
#include "task.hpp"
#include "options.hpp"
#include "http_error.hpp"
#include <boost/assert.hpp>
#include <boost/static_assert.hpp>
#include <boost/variant/static_visitor.hpp>
#include <utility>

constexpr std::size_t MIN_BUF_SIZE = 512;
constexpr std::size_t OPTIMUM_BUF_SIZE = 4096;

BOOST_STATIC_ASSERT(MIN_BUF_SIZE <= OPTIMUM_BUF_SIZE);

struct parse_result_visitor : boost::static_visitor<task_builder::result>
{
	parse_result_visitor(task_builder &builder, incomplete_task &it,
		std::shared_ptr<client> cl):
		builder{builder}, it{it}, cl{move(cl)}
	{}
	auto operator()(const http_error &result) const
	{
		return builder.make_error(it, result);
	}
	auto operator()(parser::incomplete_request) const
	{
		builder.data_buf.clear();
		return builder.make_feed_again();
	}
	auto operator()(const parser::complete_request &result) const
	{
		builder.data_buf = result.rest;
		return builder.make_task(it, cl);
	}
private:
	task_builder &builder;
	incomplete_task &it;
	std::shared_ptr<client> cl;
};

task_builder::task_builder(task::ident start_id, const options &opt, logger &lg) noexcept:
	opt{opt},
	lg{lg},
	task_id{start_id}
{
	BOOST_ASSERT(OPTIMUM_BUF_SIZE <= opt.headers_size);
}

incomplete_task task_builder::prepare_task(std::shared_ptr<client> cl)
{
	lg.debug("prepare new task #", task_id);
	auto t = task::make(task_id, cl);
	++task_id;
	auto size = opt.headers_size;
	recv_buf = { t->a.alloc(size, "request headers buffer"), size };
	p.reset(t->req, t->a);
	return t;
}

boost::asio::mutable_buffer task_builder::get_memory(const incomplete_task &it)
{
	if (buffer_size(recv_buf) < MIN_BUF_SIZE)
		//TODO adaptive size
		recv_buf = { it.t->a.alloc(OPTIMUM_BUF_SIZE, "request buffer"), OPTIMUM_BUF_SIZE };
	return recv_buf;
}

void task_builder::feed(std::size_t bytes_recv, bool last) noexcept
{
	BOOST_ASSERT(data_buf.size() == 0);
	data_buf = { boost::asio::buffer_cast<char*>(recv_buf), bytes_recv };
	recv_buf = recv_buf + bytes_recv;
	last_buf = last;
	if (last)
		feed_again_or_stop = status::STOP;
}

ready_task task_builder::make_error_task(std::shared_ptr<client> cl) const
{
	auto t = task::make(task_id, cl);
	t->make_error(response_status::INTERNAL_SERVER_ERROR);
	return { t };
}

task_builder::result task_builder::make_error(incomplete_task& it, const http_error& error) const
{
	lg.info("HTTP error ", error.code, " ", error.details);
	auto error_task = move(it.t);
	error_task->make_error(error.code);
	return { ready_task{error_task}, status::STOP };
}

task_builder::result task_builder::make_task(incomplete_task& it, std::shared_ptr<client> cl)
{
	const auto bytes_rest = data_buf.size();
	const auto complete_task = move(it.t);
	complete_task->lg.access(
		complete_task->req.method.name,
		" ",
		complete_task->req.url.all);
	if (!complete_task->req.keep_alive) {
		if (BOOST_UNLIKELY(bytes_rest != 0))
			lg.warning("non-keep-alive request, ", bytes_rest, " bytes beyond");
		return { ready_task{complete_task}, status::STOP };
	}

	if (!last_buf)
		it = prepare_task(cl);
	if (BOOST_LIKELY(bytes_rest == 0))
		return { ready_task{complete_task}, feed_again_or_stop };

	std::copy_n(data_buf.data(), data_buf.size(),
		boost::asio::buffer_cast<char*>(recv_buf));
	return { ready_task{complete_task}, status::TRY_MAKE_AGAIN };
}

task_builder::result task_builder::make_feed_again() const
{
	return { boost::none, feed_again_or_stop };
}

auto task_builder::try_make_task(incomplete_task &it,
	std::shared_ptr<client> cl) -> result
{
	parse_result_visitor visitor{ *this, it, move(cl) };
	auto result = p.parse_chunk(data_buf);

	return apply_visitor(visitor, result);
}
