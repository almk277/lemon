#include "task_builder.hpp"
#include "task.hpp"
#include "options.hpp"
#include "http_error.hpp"
#include <boost/assert.hpp>
#include <boost/static_assert.hpp>
#include <boost/variant/get.hpp>
#include <utility>

constexpr std::size_t MIN_BUF_SIZE = 512;
constexpr std::size_t OPTIMUM_BUF_SIZE = 4096;

BOOST_STATIC_ASSERT(MIN_BUF_SIZE <= OPTIMUM_BUF_SIZE);

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
	BOOST_ASSERT(buffer_size(data_buf) == 0);
	data_buf = buffer(recv_buf, bytes_recv);
	recv_buf = recv_buf + bytes_recv;
	last_buf = last;
	if (last)
		feed_again_or_stop = status::STOP;
}

auto task_builder::try_make_task(incomplete_task &it,
	std::shared_ptr<client> cl) -> result
{
	auto parse_result = p.parse_chunk(data_buf);
	if (auto err = boost::get<http_error>(&parse_result)) {
		lg.error("HTTP error ", err->code, " ", err->details);
		auto t = move(it.t);
		t->make_error(err->code);
		return{ {t}, status::STOP };
	}

	BOOST_ASSERT(boost::get<parser::buffer>(&parse_result));
	data_buf = boost::get<parser::buffer>(parse_result);

	const auto bytes_rest = buffer_size(data_buf);
	if (p.complete()) {
		const auto complete_task = move(it.t);
		complete_task->lg.access(
			complete_task->req.method.name,
			" ",
			complete_task->req.url.all);
		if (!complete_task->req.keep_alive) {
			if (BOOST_UNLIKELY(bytes_rest != 0))
				lg.warning("non-keep-alive request, ", bytes_rest, " bytes beyond");
			return { {complete_task}, status::STOP };
		}

		if (!last_buf)
			it = prepare_task(cl);
		if (BOOST_LIKELY(bytes_rest == 0))
			return { {complete_task}, feed_again_or_stop };

		buffer_copy(recv_buf, data_buf);
		return { {complete_task}, status::TRY_MAKE_AGAIN };
	}

	if (BOOST_UNLIKELY(bytes_rest != 0))
		throw std::logic_error("HTTP parser did ignore some input");
	return { boost::none, feed_again_or_stop };
}

ready_task task_builder::make_error_task(std::shared_ptr<client> cl) const
{
	auto t = task::make(task_id, cl);
	t->make_error(response_status::INTERNAL_SERVER_ERROR);
	return { t };
}
