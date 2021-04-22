#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <vector>
#include <memory>
namespace ThreadPool {
#define THREADPOOL_RUNNING      0
#define THREADPOOL_STOP         1
#define THREADPOOL_SHUTDOWN     2
enum THREADPOOL_ENUM{
    THREADPOOL_OK,
    THREADPOOL_ERR,
    THREADPOOL_NULL,
    THREADPOOL_SIZE_ERR,
    THREADPOOL_MEM_ERR
};
    class Task {
        public:
            Task(){}
            virtual ~Task(){}
            virtual void execute() = 0;
    };
    class TaskQueue {
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
            unsigned int m_maxQueueLen;
            unsigned int m_aliveThreadNum;
            std::condition_variable m_fullCondit;
            std::condition_variable m_notFullCondit;
            std::condition_variable m_emptyCondit;
            std::condition_variable m_notEmptyCondit;
            std::queue<std::shared_ptr<Task> > m_tasks;
    };
    class TPool {
        public:
            TPool(){}
            ~TPool();
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
            unsigned int m_state;
            unsigned int m_minThreadNum;
            unsigned int m_maxThreadNum;
            unsigned int m_aliveThreadNum;
            unsigned int m_maxQueueLen;
            TaskQueue m_taskQueue;
            std::thread m_adminThread;
            std::vector<std::unique_ptr<std::thread> > m_threads;
            std::mutex m_mutex;
            std::condition_variable m_startCondit;
            std::condition_variable m_zeroThreadCondit;
    };
}
#endif // THREADPOOL_H
