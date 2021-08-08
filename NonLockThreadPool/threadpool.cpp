#include "threadpool.h"

void NLTP::Thread::createWorkingThread(unsigned int queueLen)
{
    alive.store(true);
    m_queueLen = queueLen;
    m_in = 0;
    m_out = 0;
    taskQueue.resize(queueLen);
    std::thread* pThread = new std::thread(&Thread::working, this);
    pThread->detach();
    m_upThread = std::unique_ptr<std::thread>(pThread);
    return;
}

void NLTP::Thread::stop()
{
    alive.store(false);
    m_upThread.reset(nullptr);
    return;
}

int NLTP::Thread::push(std::shared_ptr<Task> spTask)
{
    if ((m_in + 1) % m_queueLen == m_out) {
        return -1;
    }
    taskQueue[m_in] = std::move(spTask);
    m_in = (m_in + 1) % m_queueLen;
    return 0;
}

std::shared_ptr<NLTP::Task> NLTP::Thread::pop_back()
{
    std::shared_ptr<Task> spTask = nullptr;
    if (m_in != m_out) {
        spTask = std::move(taskQueue[m_in]);
        m_in = (m_in - 1) % m_queueLen;
    }
    return spTask;
}

void NLTP::Thread::migrate(std::shared_ptr<Thread> spThread)
{
    /* stop thread */

    /* migrate */
    std::shared_ptr<Task> spTask = nullptr;
    for (int i = 0; i < spThread->queueSize(); i++) {
        spTask = spThread->pop_back();
        if (spTask != nullptr) {
            this->push(spTask);
        }
    }
    spTask = spThread->pop_back();
    return;
}

int NLTP::Thread::queueSize()
{
    int taskNum = 0;
    if (m_in >= m_out) {
        taskNum = m_in - m_out;
    } else {
        taskNum = m_queueLen - m_in + m_out;
    }
    return taskNum;
}

void NLTP::Thread::working()
{
    int num = 0;
    while (alive.load()) {
        /* get task */
        std::shared_ptr<Task> spTask = nullptr; 
        if (m_in != m_out) {
            spTask = std::move(taskQueue[m_out]);
            m_out = (m_out + 1) % m_queueLen;
            num++;
        }
        /* execute task */
        if (spTask != nullptr) {
            spTask->execute();
            spTask.reset();
        }
    }
    std::cout<<"tid = "<<std::this_thread::get_id()<<" tasNum: "<<num<<std::endl;
    return;
}

int NLTP::NonLockThreadPool::createThreads(int minThreadNum, int maxThreadNum, int queueLen)
{
    if (minThreadNum > maxThreadNum) {
        return -1;
    }
    /* init assign */
    m_minThreadNum = minThreadNum;
    m_maxThreadNum = maxThreadNum;
    m_queueLen = queueLen;
    m_aliveThreadNum = 0;
    policy = ROUNDROBIN;
    m_threadIndex = 0;
    m_alive.store(true);
    doRebalance.store(false);
    /* create working thread */
    for (int i = 0; i < m_minThreadNum; i++) {
        std::shared_ptr<Thread> spThread = std::make_shared<Thread>();
        spThread->createWorkingThread(queueLen);
        m_threads.push_back(spThread);
        m_aliveThreadNum++;
    }
    /* create adnin thread */
    m_adminThread = std::thread(&NonLockThreadPool::admin, this);
    m_adminThread.detach();
    return 0;
}

void NLTP::NonLockThreadPool::setPolicy(Policy p)
{
    policy = p;
    return;
}

void NLTP::NonLockThreadPool::dispatchTaskByRoundRobin(std::shared_ptr<Task> spTask)
{
    int ret = 1;
    int iterateNum = 0;
    while (ret != 0) {
        /* add by round-robin */
        ret = m_threads.at(m_threadIndex)->push(spTask);
        m_threadIndex = (m_threadIndex + 1) % m_threads.size();
        iterateNum++;
        /* if all queue is full, then create a new thread */
        if (iterateNum == m_threads.size()) {
            if (m_aliveThreadNum < m_maxThreadNum) {
                increaseThread();
            } else {
                /* stop adding task */
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            iterateNum = 0;
        }
    }
    return;
}

void NLTP::NonLockThreadPool::dispatchTaskByRand(std::shared_ptr<Task> spTask)
{
    /* when queue is long enough */
    int i = rand() % m_threads.size();
    m_threads.at(i)->push(spTask);
    return;
}

void NLTP::NonLockThreadPool::dispatchTaskToLeastLoad(std::shared_ptr<Task> spTask)
{
    unsigned int index = 0;
    unsigned int minTaskNum = 1000000;
    /* find least load thread */
    for (int i = 0; i < m_threads.size(); i++) {
        if (m_threads.at(i)->alive.load()) {
            unsigned int taskNum = m_threads.at(i)->queueSize();
            std::cout<<"least load task: "<<taskNum<<std::endl;
            if (minTaskNum > taskNum) {
                minTaskNum = taskNum;
                index = i;
            }
        }
    }
    m_threads.at(index)->push(spTask);
    return;
}

void NLTP::NonLockThreadPool::rebalance()
{
    /* find max-task-num and min-task-num */
    unsigned int minIndex = 0;
    unsigned int maxIndex = 0;
    unsigned int minTaskNum = 1000000;
    unsigned int maxTaskNum = 0;
    for (unsigned int i = 0; i < m_threads.size(); i++) {
        if (m_threads.at(i)->alive.load()) {
            unsigned int taskNum = m_threads.at(i)->queueSize();
            if (minTaskNum > taskNum) {
                minTaskNum = taskNum;
                minIndex = i;
            }
            if (maxTaskNum < taskNum) {
                maxTaskNum = taskNum;
                maxIndex = i;
            }
        }
    }
    /* balance */
    std::cout<<"------------balance : "<<maxTaskNum - minTaskNum<<"-------------"<<std::endl;
    for (unsigned int i = 0; i < maxTaskNum - minTaskNum; i++) {
        std::shared_ptr<Task> spTask = m_threads.at(maxIndex)->pop_back();
        m_threads.at(minIndex)->push(spTask);
    }
    return;
}

int NLTP::NonLockThreadPool::addTask(std::shared_ptr<Task> spTask)
{
    if (spTask == nullptr) {
        return -1;
    }
    /* load balance */
    if (doRebalance.load()) {
        rebalance();
        doRebalance.store(false);
    }
    /* dispatch task */
    switch (policy) {
        case ROUNDROBIN:
            dispatchTaskByRoundRobin(spTask);
            break;
        case RAND:
            dispatchTaskByRand(spTask);
            break;
        case LEASTLOAD:
            dispatchTaskToLeastLoad(spTask);
            break;
        default:
            dispatchTaskByRoundRobin(spTask);
    }
    return 0;
}

int NLTP::NonLockThreadPool::getThreadNum()
{ 
    return m_aliveThreadNum;
}

void NLTP::NonLockThreadPool::increaseThread()
{
    /* do statistic */

    /* add thread */
    if (m_aliveThreadNum == m_threads.size()) {
        std::shared_ptr<Thread> spThread = std::make_shared<Thread>();
        spThread->createWorkingThread(m_queueLen);
        m_threads.push_back(spThread);
    } else {
        for (int i = 0; i < m_threads.size(); i++) {
            if (m_threads.at(i)->alive.load() == false) {
                m_threads.at(i)->createWorkingThread(m_queueLen);
                break;
            }
        }
    }
    m_aliveThreadNum++;
    return;
}


void NLTP::NonLockThreadPool::admin()
{
    while (m_alive.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        /* load balance */
        doRebalance.store(true);
    }
    return;
}
void NLTP::NonLockThreadPool::shutdown()
{
    std::cout<<"alive thread num :"<<m_threads.size()<<std::endl;
    for (int i = 0; i < m_threads.size(); i++) {
        if (m_threads.at(i)->alive.load()) {
            m_threads.at(i)->alive.store(false);
        }
    }
    m_alive.store(false);
    return;
}
