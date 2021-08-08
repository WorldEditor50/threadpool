#include "threadpool.h"

void THREADPOOL::TaskQueue::setMaxQueueLen(unsigned int maxQueueLen)
{
    maxQueueLen = maxQueueLen;
    return;
}

bool THREADPOOL::TaskQueue::isFull()
{
    return tasks.size() == maxQueueLen;
}

bool THREADPOOL::TaskQueue::isNotFull()
{
    return tasks.size() < maxQueueLen;
}

bool THREADPOOL::TaskQueue::isEmpty()
{
    return tasks.size() == 0;
}

bool THREADPOOL::TaskQueue::isNotEmpty()
{
    return tasks.size() > 0;
}

void THREADPOOL::TaskQueue::addTask(std::unique_lock<std::mutex>& ulock, std::shared_ptr<Task> spTask)
{
    /* wait queue not full when queue is full */
    while (TaskQueue::isFull()) {
        std::cout<<"queue is full"<<std::endl;
        notFullCondit.wait(ulock);
    }
    /* add task */
    tasks.push(spTask);
    /* if queue is not empty broadcast all thread to get task */
    if (TaskQueue::isNotEmpty()) {
        notEmptyCondit.notify_all();
    }
    return;
}

std::shared_ptr<THREADPOOL::Task> THREADPOOL::TaskQueue::getTask(std::unique_lock<std::mutex>& ulock, unsigned int state)
{
    /* wait queue not empty when queue is empty */
    while (TaskQueue::isEmpty() && state == ThreadPool::RUNNING) {
        std::cout<<"queue is empty"<<std::endl;
        notEmptyCondit.wait(ulock);
    }
    if (state == ThreadPool::SHUTDOWN) {
        return nullptr;
    }
    /* get task from queue */
    std::shared_ptr<Task> spTask = std::move(tasks.front());
    tasks.pop();
    /* if queue is not full, signal to add task  */
    if (TaskQueue::isNotFull()) {
        notFullCondit.notify_all();
    }
    /* if queue is empty, signal main thread */
    if (TaskQueue::isEmpty()) {
        emptyCondit.notify_one();
    }
    return spTask;
}

void THREADPOOL::TaskQueue::close(std::unique_lock<std::mutex>& ulock)
{
    /* stop adding task */
    notEmptyCondit.notify_all();
    while (TaskQueue::isNotEmpty()) {
        emptyCondit.wait(ulock);
    }
    return;
}

unsigned int THREADPOOL::TaskQueue::size()
{
    return tasks.size();
}

THREADPOOL::ThreadPool::~ThreadPool()
{
    /* join admin thread */
    adminThread.join();
}

int THREADPOOL::ThreadPool::createThread(unsigned int minThreadNum, unsigned int maxThreadNum, unsigned int maxQueueLen)
{
    if (minThreadNum > maxThreadNum) {
        return THREADPOOL_SIZE_ERR;
    }
    this->minThreadNum = minThreadNum;
    this->maxThreadNum = maxThreadNum;
    aliveThreadNum = 0;
    this->maxQueueLen = maxQueueLen;
    state = STOP;
    std::lock_guard<std::mutex> lockguard(mutex);
    /* init task queue */
    taskQueue.setMaxQueueLen(maxQueueLen);
    /* create working thread */
    for (unsigned int i = 0; i < minThreadNum; i++) {
        std::thread* pThread = new std::thread(&ThreadPool::working, this);
        pThread->detach();
        aliveThreadNum++;
        threads.push_back(std::unique_ptr<std::thread>(pThread));
    }
    /* create admin thread */
    adminThread = std::thread(&ThreadPool::admin, this);
    return THREADPOOL_OK;
}

int THREADPOOL::ThreadPool::addTask(std::shared_ptr<Task> spTask)
{
    if (spTask == nullptr) {
        return THREADPOOL_NULL;
    }
    std::unique_lock<std::mutex> ulock(mutex);
    if (state == SHUTDOWN) {
        return THREADPOOL_OK;
    }
    taskQueue.addTask(ulock, spTask);
    return THREADPOOL_OK;
}

void THREADPOOL::ThreadPool::start()
{
    /* broadcast all thread to start */
    std::lock_guard<std::mutex> lockguard(mutex);
    state = RUNNING;
    startCondit.notify_all();
    return;
}

void THREADPOOL::ThreadPool::stop()
{
    std::unique_lock<std::mutex> ulock(mutex);
    /* close task queue */
    taskQueue.close(ulock);
    /* signal all thread to stop */
    state = STOP;
    return;
}

void THREADPOOL::ThreadPool::shutdown()
{
    std::unique_lock<std::mutex> ulock(mutex);
    /* close task queue */
    taskQueue.close(ulock);
    /* shutdown */
    state = SHUTDOWN;
    /* wake upa al thread to exit */
    startCondit.notify_all();
    /* wait all thread exit */
    while (aliveThreadNum > 0) {
        zeroThreadCondit.wait(ulock);
    }
    return;
}

int THREADPOOL::ThreadPool::getThreadNum()
{
    int threadNum = 0;
    std::lock_guard<std::mutex> lockguard(mutex);
    threadNum = threads.size();
    return threadNum;
}

void THREADPOOL::ThreadPool::working()
{
    std::cout<<"working threaed"<<std::endl;
    while (1) {
        std::unique_lock<std::mutex> ulock(mutex);
        /* wait condition to start */
        while (state == STOP) {
            std::cout<<"pool is stop"<<std::endl;
            startCondit.wait(ulock);
        }
        /* get task */
        std::shared_ptr<Task> spTask = taskQueue.getTask(ulock, state);
        /* execute task */
        if (spTask != nullptr) {
            spTask->execute();
        }
        /* decrease thread when poo is not busy */
        if (taskQueue.size() < maxQueueLen / 2 && aliveThreadNum > minThreadNum) {
            aliveThreadNum--;
            break;
        }
        std::cout<<"queue size "<<taskQueue.size()<<std::endl;
        /* if all tasks have been executed, stop the pool */
        if (taskQueue.size() == 0) {
            /* exit when pool is ready to shutdown */
            if (state == SHUTDOWN) {
                aliveThreadNum--;
                if (aliveThreadNum == 0) {
                    zeroThreadCondit.notify_one();
                }
                break;
            } else {
                state = STOP;
            }
        }
    }
    return;
}

void THREADPOOL::ThreadPool::admin()
{
    std::cout<<"admin threaed"<<std::endl;
    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::lock_guard<std::mutex> lockguard(mutex);
        /* wake up all thread to get task */
        if (state == STOP && taskQueue.size() > 0 && threads.size() >= minThreadNum) {
            state = RUNNING;
            startCondit.notify_all();
        }
        /* increase thread when pool is busy */
        if (taskQueue.size() >= maxQueueLen && threads.size() < maxThreadNum) {
            std::cout<<"admin add threaed"<<std::endl;
            std::thread* pThread = new std::thread(&ThreadPool::working, this);
            pThread->detach();
            aliveThreadNum++;
            threads.push_back(std::unique_ptr<std::thread>(pThread));
        }
        /* exit when pool is shutdown */
        if (state == SHUTDOWN) {
            break;
        }
    }
    return;
}
