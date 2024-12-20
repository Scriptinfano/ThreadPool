#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>

class ThreadPool
{
private:
    class TaskFunction
    {
    public:
        /*
        这里实际上不是任务函数，而是执行任务函数的lamdba函数
        */
        std::function<void()> func_;
        TaskFunction() {}
    };
    using TaskFuncPtr = std::shared_ptr<TaskFunction>;

    /*
    线程池中的线程，这里线程容器选择保存线程对象的智能指针，智能指针选择了unique_ptr，也就是说线程只能属于线程容器，其他任何实体不得持有线程对象的引用，而且在销毁线程的时候只需要将智能指针从容器中移除即可，不用手动清理内存
    */
    std::vector<std::unique_ptr<std::thread>> threads_;
    /*
    queue是一种先进先出的队列容器，所以这是一个存放任务的任务队列
    */
    std::queue<TaskFuncPtr> taskqueue_;
    /*
    任务队列的互斥锁
    */
    std::mutex mutex_;
    /*
    任务队列的条件变量
    */
    std::condition_variable condition_;
    /*
    在析构函数中将stop_的值设为true，如果想让线程池停止工作，将其置为true
    */
    std::atomic_bool stop_{false};
    /*
    初始线程数
    */
    size_t threadNum_ = 0;

    /*
    其实不能拿tasks_.empty()==true作为所有任务运行结束的标志，而是应该设立一个原子变量来统计还正在运行的线程的数量，线程在准备执行task中的lamdba函数之前就会给这个变量+1，执行结束之后就会立刻-1
    */
    std::atomic<int> runningNum_{0};

    /**
     * @brief 线程从任务队列中取出任务的动作，任务会被存储到task引用中
     *
     * @param task 这是一个输出型参数
     * @return true 成功取出
     * @return false 发生了某种意外情况（被唤醒之后发现任务队列是空的）或者整个线程池该终止运行了
     */
    bool get(TaskFuncPtr &task);

public:
    /**
     * @brief Construct a new Thread Pool object
     * 什么也不干，初始化工作留给init函数
     */
    ThreadPool() {}

    /*
    在析构函数中终止线程
    */
    ~ThreadPool()
    {
        stop();
    }

    /**
     * @brief 初始化线程池
     * 如果调用顺序不对，会抛出异常
     * @param num 指定线程池中最初的线程数
     */
    void init(size_t num);
    /**
     * @brief 启动线程池，在线程池中创建指定数量的线程。
     * 如果调用顺序不对，会抛出异常，必须先调用
     */
    void start();
    /**
     * @brief 终止线程池，置终止标记位为true
     *
     */
    void stop();
    /**
     * @brief 每一个线程的主函数
     *
     */
    void run();

    /**
     * @brief 获取目前线程池中的线程个数
     *
     * @return int 线程个数
     */
    size_t getThreadSize()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return threads_.size();
    }
    /**
     * @brief 获取目前任务的个数
     *
     * @return size_t 任务的个数
     */
    size_t getTaskNum()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return taskqueue_.size();
    }

    /**
     * @brief 等待当前任务队列中，所有工作全部结束
     * 这个是给主线程调用的，一定要先于stop调用，因为要确保所有任务都执行完之后再结束
     * @param millsecond 等待的时间，-1表示永远等待
     * @return true 所有工作都处理完成了
     * @return false 超时退出了
     */
    bool waitForAllDone(int millsecond = -1);

    /**
     * @brief 检查线程池的停止标记位是否被置为true
     *
     * @return true 线程池该停止运行了
     * @return false 线程池还没有停止运行
     */
    bool isTerminated()
    {
        return stop_.load();
    }

    /**
     * @brief 传入可执行对象和参数之后，封装任务，将任务将入任务队列
     *
     * @tparam F 可调用对象的类型，这个会被编译器推导出来，可能是函数指针类型，也可能是匿名函数类型，或者std::function类型
     * @tparam Args 模版参数包，表示一组类型参数的集合
     * @param timeout
     * @param f 具体的可调用对象
     * @param args Args&&...代表一个展开的参数包，每个参数都是万能引用
     * @return std::future<decltype(f(args...))> 返回任务函数的异步结果
     */
    template <class F, class... Args>
    auto exec(F &&f, Args &&...args) -> std::future<decltype(f(args...))>
    {
        using RetType = decltype(f(args...)); // 定义任务函数的返回类型
        auto task = std::make_shared<std::packaged_task<RetType()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        TaskFuncPtr fptr = std::make_shared<TaskFunction>();
        fptr->func_ = [task]()
        {
            (*task)();
        };
        std::unique_lock<std::mutex> lock(mutex_);
        taskqueue_.push(fptr);
        condition_.notify_one();
        return task->get_future();
    }
};
