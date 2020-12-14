#pragma once
#include "string_view.hpp"
#include "http_parser_.hpp"
#include "http_task.hpp"
#include <boost/core/noncopyable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <iterator>
#include <optional>
#include <variant>

class Options;
namespace tcp
{
class Session;
}

namespace http
{
class TaskBuilder: boost::noncopyable
{
public:
	class Results
	{
	public:
		using value = std::variant<IncompleteTask, ReadyTask, Task::Result>;

		class iterator : public boost::iterator_facade<
			iterator, value, std::input_iterator_tag, value>
		{
		public:
			explicit iterator(Results* r) noexcept: r{r} {}

		private:
			auto dereference() const
			{
				return current.value();
			}
			auto equal(const iterator& rhs) const noexcept
			{
				return !current == !rhs.current;
			}
			auto increment()
			{
				current = r->next();
			}

			Results* r;
			std::optional<value> current;

			friend class boost::iterator_core_access;
		};

		Results(TaskBuilder& builder, const std::shared_ptr<tcp::Session>& session,
		        IncompleteTask it, string_view data, bool stop);

		auto begin() -> iterator;
		auto end() -> iterator;

		auto next() -> std::optional<value>;

	private:
		auto make_ready_task(const std::shared_ptr<tcp::Session>& session, IncompleteTask& it) -> ReadyTask;
		
		string_view data;
		TaskBuilder& builder;
		const std::shared_ptr<tcp::Session>& session;
		IncompleteTask it;
		bool stop;
	};

	TaskBuilder(Task::Ident start_id, const Options& opt);

	auto prepare_task(const std::shared_ptr<tcp::Session>& session) -> IncompleteTask;
	auto get_memory(const IncompleteTask& it) -> boost::asio::mutable_buffer;
	auto make_tasks(const std::shared_ptr<tcp::Session>& session, const IncompleteTask& it,
		std::size_t bytes_recv, bool stop) -> Results;
	static auto make_error_task(IncompleteTask it, const Error& error) -> Task::Result;

private:
	Parser p;
	const Options& opt;
	Task::Ident task_id;
	boost::asio::mutable_buffer recv_buf;
};
}