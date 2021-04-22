#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>
#include "threadpool.h"
using namespace std;
using namespace ThreadPool;
int g_num = 0;
class MyTask:public Task {
    public:
        MyTask(){}
        ~MyTask(){}
        void execute()
        {
            cout<<"hello to you"<<endl;
#if 0
            ofstream outfile;
            outfile.open("./hello", ios::app);
            outfile<<"hello to you"<<endl;
            outfile.close();
#endif
            g_num++;
            return;
        }
};
class Student:public Task {
    public:
        Student(){}
        ~Student(){}
        Student(string name, int age, int score)
        {
            m_name = name;
            m_age = age;
            m_score = score;
        }
        void execute()
        {
            cout<<"name: "<<m_name<<" age: "<<m_age<<" score: "<<m_score<<endl;
            g_num++;
            return;
        }
    private:
        string m_name;
        int m_age;
        int m_score;
};
void test_hello()
{
    TPool pool;
    int ret = pool.createThread(4, 16, 2000);
    if (ret != THREADPOOL_OK) {
        return;
    }
    pool.start();
    for (int i = 0; i < 20000; i++) {
        shared_ptr<MyTask> spTask = make_shared<MyTask>();
        pool.addTask(shared_ptr<Task>(spTask));
    }
    sleep(1);
    cout<<"num = "<<g_num <<endl;
    int threadNum = pool.getThreadNum();
    cout<<" thread num: "<<threadNum<<endl;
    pool.shutdown();
    return;
}
void test_student()
{
    TPool pool;
    int ret = pool.createThread(2, 4, 10);
    if (ret != THREADPOOL_OK) {
        return;
    }
    pool.start();
    for (int i = 0; i < 20; i++) {
        shared_ptr<Student> spStu = make_shared<Student>("gandalf", 3000, 100);
        pool.addTaskToPool(spStu);
    }
    sleep(1);
    cout<<"num = "<<g_num <<endl;
    pool.shutdown();
    return;
}
int main()
{
    test_hello();
    return 0;
}
