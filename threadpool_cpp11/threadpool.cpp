#include "threadpool.h"
using namespace ThreadPool;
void TaskQueue::setMaxQueueLen(unsigned int maxQueueLen)
{
    m_maxQueueLen = maxQueueLen;
    return;
}

bool TaskQueue::isFull()
{
    return m_tasks.size() == m_maxQueueLen;
}

bool TaskQueue::isNotFull()
{
    return m_tasks.size() < m_maxQueueLen;
}

bool TaskQueue::isEmpty()
{
    return m_tasks.size() == 0;
}

bool TaskQueue::isNotEmpty()
{
    return m_tasks.size() > 0;
}

void TaskQueue::addTask(std::unique_lock<std::mutex>& ulock, std::shared_ptr<Task> spTask)
{
    /* wait queue not full when queue is full */
    while (TaskQueue::isFull()) {
        std::cout<<"queue is full"<<std::endl;
        m_notFullCondit.wait(ulock);
    }
    /* add task */
    m_tasks.push(spTask);
    /* if queue is not empty broadcast all thread to get task */
    if (TaskQueue::isNotEmpty()) {
        m_notEmptyCondit.notify_all(); 
    }
    return;
}

std::shared_ptr<Task> TaskQueue::getTask(std::unique_lock<std::mutex>& ulock, unsigned int state)
{
    /* wait queue not empty when queue is empty */
    while (TaskQueue::isEmpty() && state == THREADPOOL_RUNNING) {
        std::cout<<"queue is empty"<<std::endl;
        m_notEmptyCondit.wait(ulock);
    }
    if (state == THREADPOOL_SHUTDOWN) {
        return NULL;
    }
    /* get task from queue */
    std::shared_ptr<Task> spTask = std::move(m_tasks.front());
    m_tasks.pop();
    /* if queue is not full, signal to add task  */
    if (TaskQueue::isNotFull()) {
        m_notFullCondit.notify_all();
    }
    /* if queue is empty, signal main thread */
    if (TaskQueue::isEmpty()) {
        m_emptyCondit.notify_one();
    }
    return spTask;
}

void TaskQueue::close(std::unique_lock<std::mutex>& ulock)
{
    /* stop adding task */
    m_notEmptyCondit.notify_all();
    while (TaskQueue::isNotEmpty()) {
        m_emptyCondit.wait(ulock);
    }
    return;
}

unsigned int TaskQueue::size()
{
    return m_tasks.size();
}

TPool::~TPool()
{
    /* join admin thread */
    m_adminThread.join();
}

int TPool::createThread(unsigned int minThreadNum, unsigned int maxThreadNum, unsigned int maxQueueLen)
{
    if (minThreadNum > maxThreadNum) {
        return THREADPOOL_SIZE_ERR;
    }
    m_minThreadNum = minThreadNum;
    m_maxThreadNum = maxThreadNum;
    m_aliveThreadNum = 0;
    m_maxQueueLen = maxQueueLen;
    m_state = THREADPOOL_STOP;
    std::lock_guard<std::mutex> lockguard(m_mutex);
    /* init task queue */
    m_taskQueue.setMaxQueueLen(m_maxQueueLen);
    /* create working thread */
    for (unsigned int i = 0; i < m_minThreadNum; i++) {
        std::thread* pThread = new std::thread(&TPool::working, this);
        pThread->detach();
        m_aliveThreadNum++;
        m_threads.push_back(std::unique_ptr<std::thread>(pThread));
    }
    /* create admin thread */
    m_adminThread = std::thread(&TPool::admin, this);
    return THREADPOOL_OK;
}

int TPool::addTask(std::shared_ptr<Task> spTask)
{
    if (spTask == NULL) {
        return THREADPOOL_NULL;
    }
    std::unique_lock<std::mutex> ulock(m_mutex);
    if (m_state == THREADPOOL_SHUTDOWN) {
        return THREADPOOL_OK;
    }
    m_taskQueue.addTask(ulock, spTask);
    return THREADPOOL_OK;
}

void TPool::start()
{
    /* broadcast all thread to start */
    std::lock_guard<std::mutex> lockguard(m_mutex);
    m_state = THREADPOOL_RUNNING;
    m_startCondit.notify_all();
    return;
}

void TPool::stop()
{
    std::unique_lock<std::mutex> ulock(m_mutex);
    /* close task queue */
    m_taskQueue.close(ulock);
    /* signal all thread to stop */
    m_state = THREADPOOL_STOP;
    return;
}

void TPool::shutdown()
{
    std::unique_lock<std::mutex> ulock(m_mutex);
    /* close task queue */
    m_taskQueue.close(ulock);
    /* shutdown */
    m_state = THREADPOOL_SHUTDOWN;
    /* wake upa al thread to exit */
    m_startCondit.notify_all();
    /* wait all thread exit */
    while (m_aliveThreadNum > 0) {
        m_zeroThreadCondit.wait(ulock);
    }
    return;
}

int TPool::getThreadNum()
{
    int threadNum = 0;
    std::lock_guard<std::mutex> lockguard(m_mutex);
    threadNum = m_threads.size();
    return threadNum;
}

void TPool::working()
{
    std::cout<<"working threaed"<<std::endl;
    while (1) {
        std::unique_lock<std::mutex> ulock(m_mutex);
        /* wait condition to start */
        while (m_state == THREADPOOL_STOP) {
            std::cout<<"pool is stop"<<std::endl;
            m_startCondit.wait(ulock);
        }
        /* get task */
        std::shared_ptr<Task> spTask = m_taskQueue.getTask(ulock, m_state);
        /* execute task */
        if (spTask != NULL) {
            spTask->execute();
        }
        /* decrease thread when poo is not busy */
        if (m_taskQueue.size() < m_maxQueueLen / 2 && m_aliveThreadNum > m_minThreadNum) {
            m_aliveThreadNum--;
            break;
        }
        std::cout<<"queue size "<<m_taskQueue.size()<<std::endl;
        /* if all tasks have been executed, stop the pool */
        if (m_taskQueue.size() == 0) {
            /* exit when pool is ready to shutdown */
            if (m_state == THREADPOOL_SHUTDOWN) {
                m_aliveThreadNum--;
                if (m_aliveThreadNum == 0) {
                    m_zeroThreadCondit.notify_one();
                }
                break;
            } else {
                m_state = THREADPOOL_STOP;
            }
        }
    }
    return;
}

void TPool::admin()
{
    std::cout<<"admin threaed"<<std::endl;
    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::lock_guard<std::mutex> lockguard(m_mutex);
        /* wake up all thread to get task */
        if (m_state == THREADPOOL_STOP && m_taskQueue.size() > 0 && m_threads.size() >= m_minThreadNum) {
            m_state = THREADPOOL_RUNNING;
            m_startCondit.notify_all();
        }
        /* increase thread when pool is busy */
        if (m_taskQueue.size() >= m_maxQueueLen && m_threads.size() < m_maxThreadNum) {
            std::cout<<"admin add threaed"<<std::endl;
            std::thread* pThread = new std::thread(&TPool::working, this);
            pThread->detach();
            m_aliveThreadNum++;
            m_threads.push_back(std::unique_ptr<std::thread>(pThread));
        }
        /* exit when pool is shutdown */
        if (m_state == THREADPOOL_SHUTDOWN) {
            break;
        }
    }
    return;
}
