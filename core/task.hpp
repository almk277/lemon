#pragma once

#include "utility.hpp"
#include "logger_imp.hpp"
#include "message.hpp"
#include "arena.hpp"
#include <boost/asio/buffer.hpp>
#include <memory>
#include <vector>
#include <cstdint>

struct request_handler;
class client;
class router;

class task:
	noncopyable
{
public:
	using ident = std::uint32_t;
	static constexpr ident start_id = 1;

private:
	using buffer = boost::asio::const_buffer;
	using buffer_list = std::vector<buffer, arena::allocator<buffer>>;
	struct buffer_builder;

	static std::shared_ptr<task> make(ident id, std::shared_ptr<client> cl);

	task(ident id, std::shared_ptr<client> cl, arena &a) noexcept;
	~task();

	void run();
	void handle_request(request_handler &h);
	void serialize_resp();
	void make_error(response_status code) noexcept;

	const ident id;
	const std::shared_ptr<client> cl; // keep client alive
	logger_imp lg;
	arena &a;
	request req;
	response resp;
	buffer_list resp_buf;
	const std::shared_ptr<const router> rout;

	friend class task_builder;
	friend class task_result;
	friend class ready_task;
	friend class incomplete_task;
};

class task_result
// implements boost::asio::ConstBufferSequence
{
	std::shared_ptr<task> t;
	friend class ready_task;
public:
	task_result(std::shared_ptr<task> t) : t{ move(t) } {}
	task_result(const task_result&) = default;
	task_result(task_result&&) = default;
	task_result &operator=(const task_result&) = default;
	task_result &operator=(task_result&&) = default;
	~task_result() = default;

	task::ident get_id() const noexcept { return t->id; }
	bool operator<(const task_result &rhs) const noexcept
	{
		return get_id() < rhs.get_id();
	}
	bool operator==(const task_result &rhs) const noexcept
	{
		return t == rhs.t;
	}
	arena &get_arena() const { return t->a; }

	using value_type = task::buffer_list::value_type;
	using const_iterator = task::buffer_list::const_iterator;

	const_iterator begin() const { return t->resp_buf.cbegin(); }
	const_iterator end() const { return t->resp_buf.cend(); }
};

class ready_task
{
	ready_task(std::shared_ptr<task> t) : t{ move(t) } {}
	std::shared_ptr<task> t;
	friend class task;
	friend class task_builder;
public:
	ready_task(const ready_task&) = default;
	ready_task(ready_task&&) = default;
	ready_task &operator=(const ready_task&) = default;
	ready_task &operator=(ready_task&&) = default;
	~ready_task() = default;

	arena &get_arena() const { return t->a; }

	task_result run() { t->run(); return { t }; }
};

class incomplete_task
{
	incomplete_task(std::shared_ptr<task> t): t{ move(t) } {}
	std::shared_ptr<task> t;
	friend class task_builder;
public:
	incomplete_task(const incomplete_task&) = default;
	incomplete_task(incomplete_task&&) = default;
	incomplete_task &operator=(const incomplete_task&) = default;
	incomplete_task &operator=(incomplete_task&&) = default;
	~incomplete_task() = default;
	arena &get_arena() const { return t->a; }
};
