#include <iostream>
#include "thread_pool.hpp"
long long fibonacci(int n)
{
    if (n <= 1)
        return n;
    long long a = 0, b = 1, c = 0;
    for (int i = 2; i <= n; ++i)
    {
        c = a + b;
        a = b;
        b = c;
    }
    return c;
}
// 线程池任务
long long cpu_intensive_task(int n)
{
    std::cout << "\nComputing Fibonacci(" << n << ") in thread " << std::this_thread::get_id() << std::endl;
    long long result = fibonacci(n);
    return result;
}
int main()
{
    std::future<long long> f1, f2, f3, f4;
    // 利用线程池的生命周期来管理线程的生命周期
    {
        ThreadPool pool(8);
        f1 = pool.addTask(cpu_intensive_task, 34);
        f2 = pool.addTask(cpu_intensive_task, 45);
        f3 = pool.addTask(cpu_intensive_task, 56);
        f4 = pool.addTask(cpu_intensive_task, 76);
    }

    std::cout << "Fibonacci(2) = " << f1.get() << std::endl;
    std::cout << "Fibonacci(3) = " << f2.get() << std::endl;
    std::cout << "Fibonacci(4) = " << f3.get() << std::endl;
    std::cout << "Fibonacci(5) = " << f4.get() << std::endl;
    return 0;
}
