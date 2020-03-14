#include "task_builder.hpp"
#include "options.hpp"
#include "http_error.hpp"
#include "visitor.hpp"
#include <boost/assert.hpp>
#include <boost/concept_check.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <utility>

namespace
{
constexpr std::size_t MIN_BUF_SIZE = 512;
constexpr std::size_t OPTIMUM_BUF_SIZE = 4096;

static_assert(MIN_BUF_SIZE <= OPTIMUM_BUF_SIZE);
BOOST_CONCEPT_ASSERT((boost::InputIterator<task_builder::results::iterator>));
}

task_builder::results::results(task_builder &builder, const std::shared_ptr<client> &cl,
	incomplete_task it, string_view data, bool stop):
	data{data},
	builder{builder},
	cl{cl},
	it{std::move(it)},
	stop{stop}
{
}

auto task_builder::results::begin() -> iterator
{
	return ++iterator{ this };
}

auto task_builder::results::end() -> iterator
{
	return iterator{ this };
}

auto task_builder::results::next() -> std::optional<value>
{
	if (data.empty()) {
		if (stop)
			return std::nullopt;
		stop = true;
		return it;
	}
	
	auto result = builder.p.parse_chunk(data);
	return visit(visitor{
		[this](const http_error &error) -> value {
			data = {};
			stop = true;
			return make_error_task(it, error);
		},
		[this](parser::incomplete_request req) -> value {
			data = {};
			stop = true;
			return it;
		},
		[this](parser::complete_request req) -> value {
			data = req.rest;
			return make_ready_task(cl, it);
		},
	}, result);
}

auto task_builder::results::make_ready_task(const std::shared_ptr<client> &cl,
	incomplete_task &it) -> ready_task
{
	const auto has_more_bytes = !data.empty();
	const auto complete_task = move(it.t);

	if (complete_task->is_last()) {
		if (BOOST_UNLIKELY(has_more_bytes)) {
			complete_task->lg.warning("non-keep-alive request, bytes beyond: ", data.size());
			data = {};
		}
		stop = true;
	} else {
		if (!stop || has_more_bytes)
			it = builder.prepare_task(cl);
		if (BOOST_UNLIKELY(has_more_bytes))
			boost::copy(data, static_cast<char*>(builder.recv_buf.data()));
	}

	return { complete_task };
}

task_builder::task_builder(task::ident start_id, const options &opt):
	opt{opt},
	task_id{start_id}
{
	//TODO verify settings on load
	if (opt.headers_size < OPTIMUM_BUF_SIZE)
		throw std::runtime_error{ "headers_size too small: " + std::to_string(opt.headers_size) };
}

auto task_builder::prepare_task(const std::shared_ptr<client> &cl) -> incomplete_task
{
	auto t = task::make(task_id, cl);
	++task_id;
	auto size = opt.headers_size;
	recv_buf = { t->a.alloc(size, "request headers buffer"), size };
	p.reset(t->req);
	return { t };
}

auto task_builder::get_memory(const incomplete_task &it) -> boost::asio::mutable_buffer
{
	if (recv_buf.size() < MIN_BUF_SIZE)
		//TODO adaptive size
		recv_buf = { it.t->a.alloc(OPTIMUM_BUF_SIZE, "request buffer"), OPTIMUM_BUF_SIZE };
	return recv_buf;
}

auto task_builder::make_tasks(const std::shared_ptr<client> &cl,
	const incomplete_task &it, std::size_t bytes_recv, bool stop) -> results
{
	auto data = string_view{static_cast<char*>(recv_buf.data()), bytes_recv };
	recv_buf = recv_buf + bytes_recv;
	return results{ *this, cl, it, data, stop };
}

auto task_builder::make_error_task(incomplete_task it, const http_error &error) -> task::result
{
	it.lg().info("HTTP error ", error.code, " ", error.details);
	auto t = move(it.t);
	t->make_error(error.code);
	return { t };
}
