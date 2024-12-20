#include "threadpool.hpp"
#include<exception>
void ThreadPool::init(size_t num)
{
    std::unique_lock<std::mutex> lock(mutex_);
    // 这个if是为了防止重复调用init，第一次成功调用init之后只是设置了threadNum_的值，使其不为0
    if (!threads_.empty() || threadNum_ != 0)
        throw std::logic_error("init不可重复调用");
    threadNum_ = num;
}
void ThreadPool::start()
{
    //下面在调用emplace_back之后，被构造出来的线程会立刻开始运行，然后会在run函数的首行加锁的时候阻塞
    std::unique_lock<std::mutex> lock(mutex_);
    if (!threads_.empty() || threadNum_ == 0)
        throw std::logic_error("start必须在init之后调用，且不可重复调用");
    for (size_t i = 0; i < threadNum_; i++)
    {
        //emplace_back直接在容器的末尾直接构造一个新元素，不需要先创建一个临时对象然后再移动到容器中
        threads_.emplace_back(std::make_unique<std::thread>(&ThreadPool::run, this));
    }
}
void ThreadPool::stop()
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        stop_.store(true);
        condition_.notify_all(); // 唤醒所有等待的线程，这些线程醒来之后会检查stop_及时知道该停止了
    }
    /*
    如果没有上面那个大括号，那么锁就不会及时释放，导致在下面调用join等待所有线程退出的时候，其他线程在调用get的时候也要获取锁，就会获取不到锁，而导致线程无法正常退出
    */
    for (auto iter = threads_.begin(); iter != threads_.end(); iter++)
    {
        if ((*iter)->joinable())
        {
            (*iter)->join();
        }
    }
    // 由于前面的线程已经完成了他们的任务且不再参与任何操作，所以这里可以不加锁
    threads_.clear();
}

bool ThreadPool::get(TaskFuncPtr &task)
{
    std::unique_lock<std::mutex> lock(mutex_); // 接下来要操作任务队列了，先加锁
    if (taskqueue_.empty())
    {
        // 任务队列是空的，则条件等待即可
        condition_.wait(lock, [this]
                        { return stop_.load() || !taskqueue_.empty(); });
        // 被唤醒只有两种可能：1. 整个线程池该终止了 2. 任务队列不为空
    }
    // 被唤醒之后再检查一下整个线程池是不是该终止了
    if (stop_.load())
        return false;
    // 再检查一下任务队列确实不为空，防止条件变量的虚假唤醒，同时也为了确保任务队列不为空时才取出任务
    if (!taskqueue_.empty())
    {
        task = std::move(taskqueue_.front()); // 避免拷贝构造，task是一个左值引用，指向外部变量，这里会触发移动构造，让外部的变量接管这里的资源
        taskqueue_.pop();
        return true;
    }
    return false;
}
void ThreadPool::run()
{
    /*
    不停地尝试获取任务，然后执行任务
    */
    while (!isTerminated())
    {
        TaskFuncPtr task;
        bool ok = get(task);
        if (ok)
        {
            runningNum_.fetch_add(1);
            task->func_(); // 执行任务类中提前保存好的lamdba函数
            runningNum_.fetch_sub(1);
            std::unique_lock<std::mutex> lock(mutex_);
            // 子线程执行完自己的任务之后，看看任务队列是否为空，且是否还有其他正在运行的线程。如果没有其他正在运行的线程，且此时任务队列为空，那说明所有分配的任务都执行结束了，可以通知主线程了（配合waitForAllDone使用）
            if (runningNum_.load() == 0 && taskqueue_.empty())
            {
                condition_.notify_all();
            }
        }
    }
}

bool ThreadPool::waitForAllDone(int millsecond)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (taskqueue_.empty() && runningNum_.load() == 0)
        return true;
    if (millsecond < 0)
    {
        condition_.wait(lock, [this]
                        { return taskqueue_.empty() && runningNum_.load() == 0; });
        return true;
    }
    else
        return condition_.wait_for(lock, std::chrono::milliseconds(millsecond), [this]
                                   { return taskqueue_.empty() && runningNum_.load() == 0; }); // wait_for函数可以指定一个超时的时间参数，如果没有在时间内等到条件满足就会返回false，否则返回true
}