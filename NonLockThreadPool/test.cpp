#include <iostream>
#include <string>
#include <unistd.h>
#include "threadpool.h"
using namespace std;
using namespace NLTP;

int g_num = 0;
class MyTask:public Task {
    public:
        MyTask(){}
        ~MyTask(){}
        void execute()
        {
            cout<<"hello to you"<<endl;
            g_num++;
            return;
        }
};

int main()
{
    srand((unsigned int)time(nullptr));
    NonLockThreadPool pool;
    pool.createThreads(4, 8, 1000);
    pool.setPolicy(THREADPOOL_ROUNDROBIN);
    for (int i = 0; i < 100000; i++) {
        shared_ptr<MyTask> spTask = make_shared<MyTask>();
        pool.addTask(shared_ptr<Task>(spTask));
    }
    sleep(8);
    cout<<"finished task num: "<<g_num<<endl;
    pool.shutdown();
	return 0;
}
