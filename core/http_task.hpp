#pragma once
#include "arena_imp.hpp"
#include "http_message.hpp"
#include "leak_checked.hpp"
#include "logger_imp.hpp"
#include "task_ident.hpp"
#include <boost/core/noncopyable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <memory>
#include <utility>

namespace tcp
{
class Session;
}

namespace http
{
struct RequestHandler;
class Router;
	
class Task:
	boost::noncopyable,
	LeakChecked<Task>
{
public:
	using Ident = TaskIdent;

	struct Ptr
	{
		explicit Ptr(std::shared_ptr<Task> t) noexcept: t{move(t)} {}

		auto lg() const noexcept -> TaskLogger& { return t->lg; }
		auto get_arena() const noexcept -> Arena& { return t->a; }

		std::shared_ptr<Task> t;
	};

	class Result;

	static constexpr Ident start_id = 1;

	Task(Ident id, std::shared_ptr<tcp::Session> session) noexcept;
	~Task();

	auto is_last() const { return !req.keep_alive; }
	auto resolve() noexcept -> bool;

private:
	static auto make(Ident id, std::shared_ptr<tcp::Session> session) -> std::shared_ptr<Task>;

	auto run() -> void;
	auto handle_request() -> void;
	auto make_error(Response::Status code) noexcept -> void;

	const Ident id;
	const std::shared_ptr<const tcp::Session> session; // keep session alive
	TaskLogger lg;
	ArenaImp a;
	Request req;
	Response resp;
	const Router& router;
	RequestHandler* handler = nullptr;
	bool drop_mode = false;

	friend class TaskBuilder;
	friend class ReadyTask;
	friend class IncompleteTask;
};

class Task::Result: public Ptr
// implements boost::asio::ConstBufferSequence
{
	Result(std::shared_ptr<Task> t) noexcept: Ptr{ move(t) } {}
	friend class TaskBuilder;
	friend class ReadyTask;

	struct BufferAdapter
	{
		const auto& operator()(string_view s) const noexcept
		{
			buffer = { s.data(), s.size() };
			return buffer;
		}
	private:
		mutable boost::asio::const_buffer buffer;
	};
public:
	Result(const Result&) = default;
	Result(Result&&) = default;
	Result& operator=(const Result&) = default;
	Result& operator=(Result&&) = default;
	~Result() = default;

	Ident get_id() const noexcept { return t->id; }
	bool operator<(const Result& rhs) const noexcept
	{
		return get_id() < rhs.get_id();
	}
	bool operator==(const Result& rhs) const noexcept
	{
		return t == rhs.t;
	}

	using value_type = boost::asio::const_buffer;
	using const_iterator = boost::transform_iterator<BufferAdapter,
		Response::const_iterator>;

	const_iterator begin() const { return const_iterator{ t->resp.begin() }; }
	const_iterator end()   const { return const_iterator{ t->resp.end() }; }
};

class ReadyTask : public Task::Ptr
{
	ReadyTask(std::shared_ptr<Task> t) noexcept: Ptr{ move(t) } {}
	friend class TaskBuilder;
public:
	ReadyTask(const ReadyTask&) = default;
	ReadyTask(ReadyTask&&) = default;
	ReadyTask& operator=(const ReadyTask&) = default;
	ReadyTask& operator=(ReadyTask&&) = default;
	~ReadyTask() = default;

	Task::Result run() const { t->run(); return { t }; }
};

class IncompleteTask : public Task::Ptr
{
	IncompleteTask(std::shared_ptr<Task> t) noexcept: Ptr{ move(t) } {}
	auto resolve() { return t->resolve(); }

	friend class TaskBuilder;

public:
	IncompleteTask(const IncompleteTask&) = default;
	IncompleteTask(IncompleteTask&&) = default;
	IncompleteTask& operator=(const IncompleteTask&) = default;
	IncompleteTask& operator=(IncompleteTask&&) = default;
	~IncompleteTask() = default;
};
}