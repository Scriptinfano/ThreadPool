#include "thread_pool.hpp"
std::function<void()> ThreadPool::getTask()
{
    std::function<void()> task = std::move(task_queue_.front());
    task_queue_.pop();
    return task;
}
ThreadPool::ThreadPool(int num_threads) : stop_(false)
{
    for (int i = 0; i < num_threads; ++i)
    {
        threads_.emplace_back(std::make_unique<std::thread>([this]
                                                            {
            while(true)
            {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    // 这里解决了任务积压问题，当线程执行完自己的任务之后，并不会直接进入睡眠状态，而是会直接检查任务队列是否为空，如果不为空则会跳过睡眠状态，继续执行任务
                    while(!stop_&&task_queue_.empty())
                        condition_.wait(lock,[this]{return stop_ || !task_queue_.empty();});
                    if(stop_ && task_queue_.empty())
                        return;
                    task=getTask();
                }
                task();
            } }));
    }
}
ThreadPool ::~ThreadPool()
{
    stop_ = true;
    condition_.notify_all();
    for (auto &thread : threads_)
    {
        thread->join();
    }
}
