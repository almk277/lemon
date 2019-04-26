#include "task_builder.hpp"
#include "options.hpp"
#include "http_error.hpp"
#include <boost/assert.hpp>
#include <boost/concept_check.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/static_assert.hpp>
#include <boost/variant/static_visitor.hpp>
#include <utility>

constexpr std::size_t MIN_BUF_SIZE = 512;
constexpr std::size_t OPTIMUM_BUF_SIZE = 4096;

BOOST_STATIC_ASSERT(MIN_BUF_SIZE <= OPTIMUM_BUF_SIZE);
BOOST_CONCEPT_ASSERT((boost::InputIterator<task_builder::results::iterator>));

struct task_builder::results::parse_result_visitor : boost::static_visitor<value>
{
	explicit parse_result_visitor(results &r): r{ r }, it { r.it } {}

	auto operator()(const http_error &error) const
	{
		it.lg().info("HTTP error ", error.code, " ", error.details);
		r.data.clear();
		r.stop = true;
		return value{ make_error_task(it, error.code) };
	}
	auto operator()(parser::incomplete_request) const
	{
		r.data.clear();
		r.stop = true;
		return value{ it };
	}
	auto operator()(const parser::complete_request &result) const
	{
		r.data = result.rest;
		return value{ r.make_ready_task(r.cl, it) };
	}
private:
	results &r;
	incomplete_task &it;
};

auto task_builder::results::iterator::increment() -> void
{
	if (r->data.empty()) {
		if (r->stop) {
			current = boost::none;
		} else {
			r->stop = true;
			current.emplace(r->it);
		}
	} else {
		auto result = r->builder.p.parse_chunk(r->data);
		current = apply_visitor(parse_result_visitor{ *r }, result);
	}
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

auto task_builder::results::make_ready_task(const std::shared_ptr<client> &cl,
	incomplete_task &it) -> ready_task
{
	const auto bytes_rest = data.size();
	const auto complete_task = move(it.t);

	complete_task->lg.access(
		complete_task->req.method.name, " ", complete_task->req.url.all);

	if (complete_task->req.keep_alive) {
		if (!stop || bytes_rest != 0)
			it = builder.prepare_task(cl);
		if (BOOST_UNLIKELY(bytes_rest != 0))
			boost::copy(data, boost::asio::buffer_cast<char*>(builder.recv_buf));
	}
	else {
		if (BOOST_UNLIKELY(bytes_rest != 0)) {
			complete_task->lg.warning("non-keep-alive request, bytes beyond: ", bytes_rest);
			data.clear();
		}
		stop = true;
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
	p.reset(t->req, t->a);
	return { t };
}

auto task_builder::get_memory(const incomplete_task &it) -> boost::asio::mutable_buffer
{
	if (buffer_size(recv_buf) < MIN_BUF_SIZE)
		//TODO adaptive size
		recv_buf = { it.t->a.alloc(OPTIMUM_BUF_SIZE, "request buffer"), OPTIMUM_BUF_SIZE };
	return recv_buf;
}

auto task_builder::make_tasks(const std::shared_ptr<client> &cl,
	const incomplete_task &it, std::size_t bytes_recv, bool stop) -> results
{
	auto data = string_view{ boost::asio::buffer_cast<char*>(recv_buf), bytes_recv };
	recv_buf = recv_buf + bytes_recv;
	return results{ *this, cl, it, data, stop };
}

auto task_builder::make_error_task(incomplete_task it,
	response::status error) -> task::result
{
	auto t = move(it.t);
	t->make_error(error);
	return { t };
}
