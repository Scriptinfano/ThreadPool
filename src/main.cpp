#include <iostream>
#include "threadpool.hpp"
using namespace std;
void func0()
{
    cout << "func0()" << endl;
}
void func1(int a)
{
    cout << "func1() a=" << a << endl;
}
void func2(int a, string b)
{
    cout << "func1() a=" << a << ", b=" << b << endl;
}

void test1()
{
    ThreadPool threadpool;
    threadpool.init(3);
    threadpool.start();
    threadpool.exec(func0);
    threadpool.exec(func1, 2);
    threadpool.exec(func2, 1, "hello world");
    threadpool.stop();
}

int func1_future(int a)
{
    cout << "func1_future() a=" << a << endl;
    return a;
}
string func2_future(int a, string b)
{
    cout << "func2_future() a=" << a << ", b=" << b << endl;
    return b;
}

void test2_future()
{
    ThreadPool threadpool;
    threadpool.init(3);
    threadpool.start();
    auto res = threadpool.exec(func1_future, 2);
    auto res2 = threadpool.exec(func2_future, 2, "hello world");
    threadpool.waitForAllDone();//这里等待所有任务执行结束，这样就能确保下面future获得结果的时候不用阻塞
    cout << "res1=" << res.get() << endl;
    cout << "res2=" << res2.get() << endl;
    
    threadpool.stop();
}
class Test
{
    string name_;

public:
    int test(int i)
    {
        cout << name_ << ", i=" << i << endl;
        return i;
    }
    void setName(const string &str)
    {
        name_ = str;
    }
};
void test3_bind(){
    ThreadPool pool;
    pool.init(3);
    pool.start();
    Test t1;
    Test t2;
    t1.setName("test t1");
    t2.setName("test t2");
    auto res1=pool.exec(std::bind(&Test::test,&t1,std::placeholders::_1),10);
    auto res2=pool.exec(std::bind(&Test::test,&t2,std::placeholders::_1),11);
    pool.waitForAllDone();
    cout << "运行结束，结果分别是：" << res1.get() << " " << res2.get() << endl;
}

int main()
{
    //test3_bind();
    cout << __cplusplus << endl;
}