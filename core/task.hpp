#pragma once

#include "task_ident.hpp"
#include "logger_imp.hpp"
#include "arena_imp.hpp"
#include "message.hpp"
#include "leak_checked.hpp"
#include <boost/core/noncopyable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <memory>
#include <utility>

struct request_handler;
class client;
class router;

class task:
	boost::noncopyable,
	leak_checked<task>
{
public:
	using ident = task_ident;

	struct ptr
	{
		explicit ptr(std::shared_ptr<task> t) noexcept: t{move(t)} {}

		task_logger &lg() const noexcept { return t->lg; }
		arena &get_arena() const noexcept { return t->a; }

		std::shared_ptr<task> t;
	};

	static constexpr ident start_id = 1;

	task(ident id, std::shared_ptr<client> cl) noexcept;
	~task();

private:
	static std::shared_ptr<task> make(ident id, std::shared_ptr<client> cl);

	void run();
	void handle_request(request_handler &h);
	void make_error(response::status code) noexcept;

	const ident id;
	const std::shared_ptr<const client> cl; // keep client alive
	task_logger lg;
	arena_imp a;
	request req;
	response resp;
	const std::shared_ptr<const router> rout;

	friend class task_builder;
	friend class task_result;
	friend class ready_task;
	friend class incomplete_task;
};

class task_result: public task::ptr
// implements boost::asio::ConstBufferSequence
{
	task_result(std::shared_ptr<task> t) noexcept: ptr{ move(t) } {}
	friend class task_builder;
	friend class ready_task;

	struct buffer_adapter
	{
		const boost::asio::const_buffer &operator()(string_view s) const noexcept
		{
			buffer = { s.data(), s.size() };
			return buffer;
		}
	private:
		mutable boost::asio::const_buffer buffer;
	};
public:
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

	using value_type = boost::asio::const_buffer;
	using const_iterator = boost::transform_iterator<buffer_adapter,
		response::const_iterator>;

	const_iterator begin() const { return const_iterator{ t->resp.begin() }; }
	const_iterator end()   const { return const_iterator{ t->resp.end() }; }
};

class ready_task : public task::ptr
{
	ready_task(std::shared_ptr<task> t) noexcept: ptr{ move(t) } {}
	friend class task_builder;
public:
	ready_task(const ready_task&) = default;
	ready_task(ready_task&&) = default;
	ready_task &operator=(const ready_task&) = default;
	ready_task &operator=(ready_task&&) = default;
	~ready_task() = default;

	task_result run() const { t->run(); return { t }; }
};

class incomplete_task : public task::ptr
{
	incomplete_task(std::shared_ptr<task> t) noexcept: ptr{ move(t) } {}
	friend class task_builder;
public:
	incomplete_task(const incomplete_task&) = default;
	incomplete_task(incomplete_task&&) = default;
	incomplete_task &operator=(const incomplete_task&) = default;
	incomplete_task &operator=(incomplete_task&&) = default;
	~incomplete_task() = default;
};
