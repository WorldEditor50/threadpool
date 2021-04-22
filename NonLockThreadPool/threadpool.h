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
#define THREADPOOL_ROUNDROBIN 0
#define THREADPOOL_RAND       1
#define THREADPOOL_LEASTLOAD  2
#define THREADPOOL_RUNNING    0 
#define THREADPOOL_STOP       1 
#define THREADPOOL_SHUTDOWN   2 
    class Task {
        public:
            Task(){}
            virtual ~Task(){}
            virtual void execute() = 0;
    };
    class Thread {
        public:
            Thread(){}
            ~Thread(){}
            void createWorkingThread(unsigned int queueLen);
            int addTask(std::shared_ptr<Task> spTask);
            void migrate(std::shared_ptr<Thread> spThread);
            std::shared_ptr<Task> pop_back();
            int getTaskNum();
            void release();
            std::atomic<bool> m_alive;
        private:
            void working();
            unsigned int m_queueLen;
            std::unique_ptr<std::thread> m_upThread;
            unsigned int m_taskNum;
            std::atomic_uint m_in;
            std::atomic_uint m_out;
            std::vector<std::shared_ptr<Task> > m_tasks;
    };
    class NonLockThreadPool {
        public:
            NonLockThreadPool(){}
            ~NonLockThreadPool(){}
            int createThreads(unsigned int minThreadNum, unsigned int maxThreadNum, unsigned int queueLen); 
            int addTask(std::shared_ptr<Task> spTask);
            void setPolicy(unsigned char policy);
            int getThreadNum();
            void shutdown();
        private:
            void admin();
            void dispatchTaskByRoundRobin(std::shared_ptr<Task> spTask);
            void dispatchTaskByRand(std::shared_ptr<Task> spTask);
            void dispatchTaskToLeastLoad(std::shared_ptr<Task> spTask);
            void loadBalance();
            void increaseThread();
            void decreaseThread();
            unsigned char m_policy;
            unsigned int m_queueLen;
            unsigned int m_minThreadNum;
            unsigned int m_maxThreadNum;
            unsigned int m_threadIndex;
            std::atomic_uint m_aliveThreadNum;
            std::vector<std::shared_ptr<Thread> > m_threads;
            std::thread m_adminThread;
            std::atomic<bool> m_alive;
            std::atomic<bool> m_balance;
    };
}
#endif // THREADPOOL_H
