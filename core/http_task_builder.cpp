#include "http_task_builder.hpp"
#include "http_error.hpp"
#include "options.hpp"
#include "visitor.hpp"
#include <boost/assert.hpp>
#include <boost/concept_check.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <utility>

namespace http
{
namespace
{
constexpr std::size_t min_buf_size = 512;
constexpr std::size_t optimum_buf_size = 4096;

static_assert(min_buf_size <= optimum_buf_size);
BOOST_CONCEPT_ASSERT((boost::InputIterator<TaskBuilder::Results::iterator>));
}

TaskBuilder::Results::Results(TaskBuilder& builder, const std::shared_ptr<tcp::Session>& session,
	IncompleteTask it, string_view data, bool stop):
	data{data},
	builder{builder},
	session{session},
	it{std::move(it)},
	stop{stop}
{
}

auto TaskBuilder::Results::begin() -> iterator
{
	return ++iterator{ this };
}

auto TaskBuilder::Results::end() -> iterator
{
	return iterator{ this };
}

auto TaskBuilder::Results::next() -> std::optional<value>
{
	using Result = std::optional<value>;

	if (data.empty()) {
		if (stop)
			return std::nullopt;
		stop = true;
		return it;
	}

	bool repeat;
	Result result;
	do {
		repeat = false;
		auto [parse_result, rest_data] = builder.parser.parse_chunk(data);
		BOOST_ASSERT(rest_data.size() < data.size());
		data = rest_data;
		result = visit(Visitor{
			               [this](const Error& error) -> Result
			               {
				               stop = true;
				               return make_error_task(it, error);
			               },
			               [this, &repeat](Parser::RequestLine) -> Result
			               {
				               repeat = true;
							   it.resolve();
				               return it;
			               },
			               [this](Parser::IncompleteRequest) -> Result
			               {
				               stop = true;
				               return it;
			               },
			               [this](Parser::CompleteRequest) -> Result
			               {
				               return make_ready_task(session, it);
			               },
		               }, parse_result);
	} while (repeat);
	return result;
}

auto TaskBuilder::Results::make_ready_task(const std::shared_ptr<tcp::Session>& session,
	IncompleteTask& it) -> ReadyTask
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
			it = builder.prepare_task(session);
		if (BOOST_UNLIKELY(has_more_bytes))
			boost::copy(data, static_cast<char*>(builder.recv_buf.data()));
	}

	builder.parser.finalize(complete_task->req);
	
	return { complete_task };
}

TaskBuilder::TaskBuilder(Task::Ident start_id, const Options& opt):
	opt{opt},
	task_id{start_id}
{
	//TODO verify settings on load
	if (opt.headers_size < optimum_buf_size)
		throw std::runtime_error{ "headers_size too small: " + std::to_string(opt.headers_size) };
}

auto TaskBuilder::prepare_task(const std::shared_ptr<tcp::Session>& session) -> IncompleteTask
{
	auto t = Task::make(task_id, session);
	++task_id;
	auto size = opt.headers_size;
	head_buf = { t->a.alloc(size, "request headers buffer"), size };
	recv_buf = head_buf;
	parser.reset(t->req, t->drop_mode);
	return { t };
}

auto TaskBuilder::get_memory(const IncompleteTask& it) -> boost::asio::mutable_buffer
{
	if (it.t->drop_mode)
		// reuse old buffer
		recv_buf = head_buf;
	else if (recv_buf.size() < min_buf_size)
		//TODO adaptive size
		recv_buf = { it.t->a.alloc(optimum_buf_size, "request buffer"), optimum_buf_size };
	return recv_buf;
}

auto TaskBuilder::make_tasks(const std::shared_ptr<tcp::Session>& session,
	const IncompleteTask& it, std::size_t bytes_recv, bool stop) -> Results
{
	auto data = string_view{static_cast<char*>(recv_buf.data()), bytes_recv };
	recv_buf = recv_buf + bytes_recv;
	return Results{ *this, session, it, data, stop };
}

auto TaskBuilder::make_error_task(IncompleteTask it, const Error& error) -> Task::Result
{
	it.lg().info("HTTP error ", error.code, " ", error.details);
	auto t = move(it.t);
	t->make_error(error.code);
	return { t };
}
}