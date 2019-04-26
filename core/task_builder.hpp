#pragma once

#include "utility.hpp"
#include "parser.hpp"
#include "task.hpp"
#include <boost/core/noncopyable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/optional/optional.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/variant/variant.hpp>
#include <iterator>

class options;
class client;

class task_builder: boost::noncopyable
{
public:
	class results
	{
	public:
		using value = boost::variant<incomplete_task, ready_task, task::result>;

		class iterator : public boost::iterator_facade<
			iterator, value, std::input_iterator_tag, value>
		{
		public:
			explicit iterator(results *r) noexcept: r{r} {}

		private:
			auto dereference() const
			{
				return current.value();
			}
			auto equal(const iterator &rhs) const noexcept
			{
				return !current == !rhs.current;
			}
			auto increment() -> void;

			results *r;
			boost::optional<value> current;

			friend class boost::iterator_core_access;
		};

		results(task_builder &builder, const std::shared_ptr<client> &cl,
		        incomplete_task it, string_view data, bool stop);

		auto begin() -> iterator;
		auto end() -> iterator;

	private:
		auto make_ready_task(const std::shared_ptr<client> &cl, incomplete_task &it) -> ready_task;
		
		string_view data;
		task_builder &builder;
		const std::shared_ptr<client> &cl;
		incomplete_task it;
		bool stop;

		struct parse_result_visitor;
	};

	task_builder(task::ident start_id, const options &opt);

	auto prepare_task(const std::shared_ptr<client> &cl) -> incomplete_task;
	auto get_memory(const incomplete_task &it) -> boost::asio::mutable_buffer;
	auto make_tasks(const std::shared_ptr<client> &cl, const incomplete_task &it,
		std::size_t bytes_recv, bool stop) -> results;
	static auto make_error_task(incomplete_task it, response::status error) ->task::result;

private:
	parser p;
	const options &opt;
	task::ident task_id;
	boost::asio::mutable_buffer recv_buf;
};