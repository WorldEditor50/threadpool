#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <thread>
#include <atomic>
#include <cstdlib>
namespace NLTP {

class Task
{
public:
    Task(){}
    virtual ~Task(){}
    virtual void execute() = 0;
};
class Thread
{
public:
    Thread(){}
    ~Thread(){}
    void createWorkingThread(unsigned int queueLen);
    int push(std::shared_ptr<Task> spTask);
    void migrate(std::shared_ptr<Thread> spThread);
    std::shared_ptr<Task> pop_back();
    int queueSize();
    void stop();
    std::atomic<bool> alive;
private:
    void working();
    unsigned int m_queueLen;
    std::unique_ptr<std::thread> m_upThread;
    std::atomic_uint m_in;
    std::atomic_uint m_out;
    std::vector<std::shared_ptr<Task> > taskQueue;
};
class NonLockThreadPool
{
public:
    enum Policy {
        ROUNDROBIN =  0,
        RAND,
        LEASTLOAD
    };
    enum State {
        RUNNING = 0,
        STOP ,
        SHUTDOWN,
    };
public:
    NonLockThreadPool(){}
    ~NonLockThreadPool(){}
    int createThreads(int minThreadNum, int maxThreadNum, int queueLen);
    int addTask(std::shared_ptr<Task> spTask);
    void setPolicy(Policy p);
    int getThreadNum();
    void shutdown();
private:
    void admin();
    void dispatchTaskByRoundRobin(std::shared_ptr<Task> spTask);
    void dispatchTaskByRand(std::shared_ptr<Task> spTask);
    void dispatchTaskToLeastLoad(std::shared_ptr<Task> spTask);
    void rebalance();
    void increaseThread();
    Policy policy;
    int m_queueLen;
    int m_minThreadNum;
    int m_maxThreadNum;
    int m_threadIndex;
    std::atomic_uint m_aliveThreadNum;
    std::vector<std::shared_ptr<Thread> > m_threads;
    std::thread m_adminThread;
    std::atomic<bool> m_alive;
    std::atomic<bool> doRebalance;
};
}
#endif // THREADPOOL_H
