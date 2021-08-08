#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <vector>
#include <memory>
namespace THREADPOOL {

enum ERROR {
    THREADPOOL_OK = 0,
    THREADPOOL_ERR,
    THREADPOOL_NULL,
    THREADPOOL_SIZE_ERR,
    THREADPOOL_MEERR
};
class Task
{
public:
    Task(){}
    virtual ~Task(){}
    virtual void execute() = 0;
};
class TaskQueue
{
public:
    TaskQueue(){}
    ~TaskQueue(){}
    void setMaxQueueLen(unsigned int maxQueueLen);
    void addTask(std::unique_lock<std::mutex>& ulock, std::shared_ptr<Task> spTask);
    std::shared_ptr<Task> getTask(std::unique_lock<std::mutex>& ulock, unsigned int state);
    void close(std::unique_lock<std::mutex>& ulock);
    unsigned int size();
private:
    bool isFull();
    bool isNotFull();
    bool isEmpty();
    bool isNotEmpty();
    unsigned int maxQueueLen;
    unsigned int aliveThreadNum;
    std::condition_variable fullCondit;
    std::condition_variable notFullCondit;
    std::condition_variable emptyCondit;
    std::condition_variable notEmptyCondit;
    std::queue<std::shared_ptr<Task> > tasks;
};
class ThreadPool
{
public:
    enum STATE {
        RUNNING = 0,
        STOP,
        SHUTDOWN
    };
public:
    ThreadPool(){}
    ~ThreadPool();
    int createThread(unsigned int minThreadNum, unsigned int maxThreadNum, unsigned int maxQueueLen);
    int addTask(std::shared_ptr<Task> spTask);
    int getThreadNum();
    void start();
    void stop();
    void shutdown();
#define addTaskToPool(spTask) addTask(std::shared_ptr<Task>(spTask))
private:
    void working();
    void admin();
    STATE state;
    int minThreadNum;
    int maxThreadNum;
    int aliveThreadNum;
    int maxQueueLen;
    TaskQueue taskQueue;
    std::thread adminThread;
    std::vector<std::unique_ptr<std::thread> > threads;
    std::mutex mutex;
    std::condition_variable startCondit;
    std::condition_variable zeroThreadCondit;
};
}
#endif // THREADPOOL_H
