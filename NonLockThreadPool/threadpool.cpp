#include "threadpool.h"
using namespace NLTP;

void Thread::createWorkingThread(unsigned int queueLen)
{
    m_alive.store(true);
    m_queueLen = queueLen;
    m_in = 0;
    m_out = 0;
    m_tasks.resize(queueLen);
    std::thread* pThread = new std::thread(&Thread::working, this);
    pThread->detach();
    m_upThread = std::unique_ptr<std::thread>(pThread);
    return;
}

void Thread::release()
{
    m_alive.store(false);
    m_upThread.reset(nullptr);
    return;
}

int Thread::addTask(std::shared_ptr<Task> spTask)
{
    if ((m_in + 1) % m_queueLen == m_out) {
        return -1;
    }
    m_tasks[m_in] = std::move(spTask);
    m_in = (m_in + 1) % m_queueLen;
    return 0;
}

std::shared_ptr<Task> Thread::pop_back()
{
    std::shared_ptr<Task> spTask = nullptr;
    if (m_in != m_out) {
        spTask = std::move(m_tasks[m_in]);
        m_in = (m_in - 1) % m_queueLen;
    }
    return spTask;
}

void Thread::migrate(std::shared_ptr<Thread> spThread)
{
    /* stop thread */

    /* migrate */
    std::shared_ptr<Task> spTask = nullptr;
    int taskNum = spThread->getTaskNum();
    for (unsigned int i = 0; i < taskNum; i++) {
        spTask = spTask->pop_back();
        if (spTask != nullptr) {
            this->addTask(spTask);
        }
    }
    spTask = spThread->pop_back();
    return;
}

int Thread::getTaskNum()
{
    int taskNum = 0;
    if (m_in >= m_out) {
        taskNum = m_in - m_out;
    } else {
        taskNum = m_queueLen - m_in + m_out;
    }
    return taskNum;
}

void Thread::working()
{
    int num = 0;
    while (m_alive.load()) {
        /* get task */
        std::shared_ptr<Task> spTask = nullptr; 
        if (m_in != m_out) {
            spTask = std::move(m_tasks[m_out]);
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

int NonLockThreadPool::createThreads(unsigned int minThreadNum, unsigned int maxThreadNum, unsigned int queueLen)
{
    if (minThreadNum > maxThreadNum) {
        return -1;
    }
    /* init assign */
    m_minThreadNum = minThreadNum;
    m_maxThreadNum = maxThreadNum;
    m_queueLen = queueLen;
    m_aliveThreadNum = 0;
    m_policy = THREADPOOL_ROUNDROBIN;
    m_threadIndex = 0;
    m_alive.store(true);
    m_balance.store(false);
    /* create working thread */
    for (unsigned int i = 0; i < m_minThreadNum; i++) {
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

void NonLockThreadPool::setPolicy(unsigned char policy)
{
    m_policy = policy;
    return;
}

void NonLockThreadPool::dispatchTaskByRoundRobin(std::shared_ptr<Task> spTask)
{
    int ret = 1;
    int iterateNum = 0;
    while (ret != 0) {
        /* add by round-robin */
        ret = m_threads.at(m_threadIndex)->addTask(spTask);
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

void NonLockThreadPool::dispatchTaskByRand(std::shared_ptr<Task> spTask)
{
    /* when queue is long enough */
    int i = rand() % m_threads.size();
    m_threads.at(i)->addTask(spTask);
    return;
}

void NonLockThreadPool::dispatchTaskToLeastLoad(std::shared_ptr<Task> spTask)
{
    unsigned int index = 0;
    unsigned int minTaskNum = 1000000;
    /* find least load thread */
    for (unsigned int i = 0; i < m_threads.size(); i++) {
        if (m_threads.at(i)->m_alive.load()) {
            unsigned int taskNum = m_threads.at(i)->getTaskNum();
            std::cout<<"least load task: "<<taskNum<<std::endl;
            if (minTaskNum > taskNum) {
                minTaskNum = taskNum;
                index = i;
            }
        }
    }
    m_threads.at(index)->addTask(spTask);
    return;
}

void NonLockThreadPool::loadBalance()
{
    /* find max-task-num and min-task-num */
    unsigned int minIndex = 0;
    unsigned int maxIndex = 0;
    unsigned int minTaskNum = 1000000;
    unsigned int maxTaskNum = 0;
    for (unsigned int i = 0; i < m_threads.size(); i++) {
        if (m_threads.at(i)->m_alive.load()) {
            unsigned int taskNum = m_threads.at(i)->getTaskNum();
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
        m_threads.at(minIndex)->addTask(spTask);
    }
    return;
}

int NonLockThreadPool::addTask(std::shared_ptr<Task> spTask)
{
    if (spTask == nullptr) {
        return -1;
    }
    /* load balance */
    if (m_balance.load()) {
        loadBalance();
        m_balance.store(false);
    }
    /* dispatch task */
    switch (m_policy) {
        case THREADPOOL_ROUNDROBIN:
            dispatchTaskByRoundRobin(spTask);
            break;
        case THREADPOOL_RAND:
            dispatchTaskByRand(spTask);
            break;
        case THREADPOOL_LEASTLOAD:
            dispatchTaskToLeastLoad(spTask);
            break;
        default:
            dispatchTaskByRoundRobin(spTask);
    }
    return 0;
}

int NonLockThreadPool::getThreadNum()
{ 
    return m_aliveThreadNum;
}

void NonLockThreadPool::increaseThread()
{
    /* do statistic */

    /* add thread */
    if (m_aliveThreadNum == m_threads.size()) {
        std::shared_ptr<Thread> spThread = std::make_shared<Thread>();
        spThread->createWorkingThread(m_queueLen);
        m_threads.push_back(spThread);
    } else {
        for (unsigned int i = 0; i < m_threads.size(); i++) {
            if (m_threads.at(i)->m_alive.load() == false) {
                m_threads.at(i)->createWorkingThread(m_queueLen);
                break;
            }
        }
    }
    m_aliveThreadNum++;
    return;
}

void NonLockThreadPool::decreaseThread()
{   
    /* do statistic */
    /* stop thread */
    unsigned int from = m_threads.size() - 1;
    m_threads.at(from)->release();
    /* migrate task */
    unsigned int to = (index - 1) % (m_threads.size() - 1);
    m_thread.at(to).migrate(m_threads.at(from));   
    m_aliveThreadNum--;
    return;
}

void NonLockThreadPool::admin()
{
    while (m_alive.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        /* load balance */
        m_balance.store(true);
        /* decrease thread */
        if (false) {
            decreaseThread();
        }
    }
    return;
}
void NonLockThreadPool::shutdown()
{
    std::cout<<"alive thread num :"<<m_threads.size()<<std::endl;
    for (unsigned int i = 0; i < m_threads.size(); i++) {
        if (m_threads.at(i)->m_alive.load()) {
            m_threads.at(i)->m_alive.store(false);
        }
    }
    m_alive.store(false);
    return;
}
