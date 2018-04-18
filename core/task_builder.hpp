#pragma once

#include "utility.hpp"
#include "parser.hpp"
#include "task.hpp"
#include <boost/asio/buffer.hpp>
#include <boost/optional/optional.hpp>

class options;
class client;

class task_builder: noncopyable
{
public:
	task_builder(task::ident start_id, const options &opt, logger &lg) noexcept;
	~task_builder() = default;

	incomplete_task prepare_task(std::shared_ptr<client> cl);
	boost::asio::mutable_buffer get_memory(const incomplete_task &it);
	void feed(std::size_t bytes_recv, bool last) noexcept;

	enum class status
	{
		TRY_MAKE_AGAIN,
		FEED_AGAIN,
		STOP
	};
	struct result
	{
		boost::optional<ready_task> t;
		status s;
	};
	result try_make_task(incomplete_task &it, std::shared_ptr<client> cl);
	ready_task make_error_task(std::shared_ptr<client> cl) const;

private:
	result make_task(incomplete_task& it, std::shared_ptr<client> cl);
	result make_feed_again() const;
	result make_error(incomplete_task& it, const http_error& error) const;

	parser p;
	const options &opt;
	logger &lg;
	task::ident task_id;
	boost::asio::mutable_buffer recv_buf;
	string_view data_buf;
	bool last_buf = false;
	status feed_again_or_stop = status::FEED_AGAIN;

	friend struct parse_result_visitor;
};