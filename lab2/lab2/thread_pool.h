#pragma once
#include <windows.h>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>

typedef std::function<void()> task_type;
typedef void (*FuncType) (HANDLE, HANDLE, DWORD, int, DWORD, DWORD);

class ThreadPool {
public:
	ThreadPool(int count_thread = 4);
	void start();
	void stop();
	void push_task(FuncType f, HANDLE arg1, HANDLE arg2, DWORD arg3, int arg4, DWORD arg5, DWORD arg6);
	void thread_func();
private:
	std::vector<std::thread> threads_;
	std::queue<task_type> task_queue_;
	std::mutex locker_;
	std::condition_variable event_holder_;
	volatile bool work_;
	int count_thread_;
};


class RequestHandler {
public:
	RequestHandler(int count_thread = 4);
	~RequestHandler();
	void push_task(FuncType f, HANDLE arg1, HANDLE arg2, DWORD arg3, int arg4, DWORD arg5, DWORD arg6);
private:
	ThreadPool tpool_;
};