#include "thread_pool.h"


ThreadPool::ThreadPool(int count_thread) {
	count_thread_ = min(count_thread, 10);
	work_ = false;
}

void ThreadPool::start() {
	work_ = true;
	for (int i = 0; i < count_thread_; ++i) {
		threads_.push_back(std::thread(&ThreadPool::thread_func, this));
	}
}

void ThreadPool::stop() {
	work_ = false;
	event_holder_.notify_all();
	for (auto& t : threads_) {
		t.join();
	}
}

void ThreadPool::push_task(FuncType f, HANDLE arg1, HANDLE arg2, DWORD arg3, int arg4, DWORD arg5, DWORD arg6) {
	std::lock_guard<std::mutex> lock(locker_);
	task_type new_task([=]() { f(arg1, arg2, arg3, arg4, arg5, arg6); });
	task_queue_.push(new_task);
	
	event_holder_.notify_one();
}

void ThreadPool::thread_func() {
	while (true) {
		task_type task_to_do;
		
		std::unique_lock<std::mutex> lock(locker_);
		if (task_queue_.empty() && !work_) return;
		if (task_queue_.empty()) {
			event_holder_.wait(lock, [&]() { return !task_queue_.empty() || !work_; });
		}
		if (!task_queue_.empty()) {
			task_to_do = task_queue_.front();
			task_queue_.pop();
		}
		lock.unlock();

		if (task_to_do) {
			task_to_do();
		}
	}
}


RequestHandler::RequestHandler(int count_thread) : tpool_(count_thread) {
	tpool_.start();
}

RequestHandler::~RequestHandler() {
	tpool_.stop();
}

void RequestHandler::push_task(FuncType f, HANDLE arg1, HANDLE arg2, DWORD arg3, int arg4, DWORD arg5, DWORD arg6) {
	tpool_.push_task(f, arg1, arg2, arg3, arg4, arg5, arg6);
}

