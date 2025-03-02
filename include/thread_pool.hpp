#pragma once
#include <functional>
#include <thread>
#include <vector>
#include <memory>
#include <future>
#include <mutex>
#include <condition_variable>
#include <atomic>
class ThreadPool
{
private:
    // 线程容器
    std::vector<std::unique_ptr<std::thread>> threads_;
    // 任务队列，任务队列中存放的可执行对象所代表的函数并不是实际的任务函数，而是调用任务函数的函数
    std::queue<std::function<void()>> task_queue_;
    // 互斥锁和条件变量
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    // 是否关闭线程池
    std::atomic<bool> stop_;
    std::function<void()> getTask();

public:
    /*
    Func：表示 函数类型，可以是一个函数指针、函数对象（如 std::function<T>）、Lambda 表达式等。
    Args...：表示 可变参数模板，允许传入任意数量和类型的参数。
    */
    template <typename Func, typename... Args>
    auto addTask(Func &&func, Args &&...args) -> std::future<decltype(func(args...))>;
    /**
     * @brief 构造一个新线程池对象，并初始化然后启动线程池，启动线程池之后，所有线程会开始运行，所有线程首先会沉睡，直到有任务加入，有任务加入只能唤醒一次，而线程在第一次唤醒之后会再次沉睡，直到暂停条件不满足才会继续执行
     *
     * @param num_threads 初始线程数
     * @param queue_size 任务队列大小
     */
    ThreadPool(int num_threads);
    /**
     * @brief 销毁线程池对象，将停止标记位置为true，并通知所有线程，沉睡的线程会被唤醒然后检查到停止标记位为true，然后退出线程；清醒的正在执行任务的线程在执行完自己的任务之后会再次检查到停止标记位为true，然后退出线程
     */
    ~ThreadPool();

    /**
     * @brief 获取目前线程池中的线程个数
     *
     * @return int 线程个数
     */
    size_t getThreadSize()
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return threads_.size();
    }
    /**
     * @brief 获取目前剩余任务的个数
     *
     * @return size_t 任务的个数
     */
    size_t getTaskNum()
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return task_queue_.size();
    }
};

template <typename Func, typename... Args>
auto ThreadPool::addTask(Func &&func, Args &&...args) -> std::future<decltype(func(args...))>
{
    using ReturnType = decltype(func(args...));
    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
    std::future<ReturnType> result = task->get_future();
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (stop_)
            throw std::runtime_error("ThreadPool is stopped,can't add new task");
        task_queue_.emplace([task]
                            { (*task)(); });
    }
    condition_.notify_one();
    return result;
}