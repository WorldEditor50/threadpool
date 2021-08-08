#include "threadpool.h"
Task* TaskQueue_NewTask(TaskQueue* pstQueue, void* (*pfExecute)(void* pvArg), void* pvArg)
{
    if (pstQueue == NULL) {
        THREADPOOL_MESSAGE("task queue is null");
        return NULL;
    }
    Task* pstTask = NULL;
    if (pstQueue->taskNumOfPool < 1) {
        pstTask = (Task*)malloc(sizeof(Task));
        if (pstTask == NULL) {
            THREADPOOL_MESSAGE("fail to create task");
            return NULL;
        }
    } else {
        Task* pstNext = pstQueue->pstMemPool->pstNext;
        pstTask = pstQueue->pstMemPool;
        pstQueue->pstMemPool = pstNext;
        pstQueue->taskNumOfPool--;
    }
    pstTask->pfExecute = pfExecute;
    pstTask->pvArg = pvArg;
    pstTask->pstNext = NULL;
    return pstTask;
}

TaskQueue* TaskQueue_New(int maxTaskNum, pthread_mutex_t* pstLock)
{
    if (maxTaskNum < 1 || pstLock == NULL) {
        THREADPOOL_MESSAGE("lock is empty or size error");
        return NULL;
    }
    TaskQueue* pstQueue = NULL;
    pstQueue = (TaskQueue*)malloc(sizeof(TaskQueue));
    if (pstQueue == NULL) {
        THREADPOOL_MESSAGE("fail to create task queue");
        return NULL;
    }
    int ret = 0;
    pstQueue->taskNum = 0;
    pstQueue->taskNumOfPool = 0;
    pstQueue->maxTaskNum = maxTaskNum;
    pstQueue->pstLock = pstLock;
    pstQueue->pstTail = NULL;
    pstQueue->pstMemPool = NULL;
    ret = pthread_cond_init(&pstQueue->stConditEmpty, NULL);
    if (ret != 0) {
        free(pstQueue);
        return NULL;
    }
    ret = pthread_cond_init(&pstQueue->stConditNotEmpty, NULL);
    if (ret != 0) {
        free(pstQueue);
        return NULL;
    }
    ret = pthread_cond_init(&pstQueue->stConditNotFull, NULL);
    if (ret != 0) {
        free(pstQueue);
        return NULL;
    }
    return pstQueue;
}

int TaskQueue_Delete(TaskQueue* pstQueue)
{
    if (pstQueue == NULL) {
        return THREADPOOL_NULL;
    }
    printf("mem pool task num = %d\n", pstQueue->taskNumOfPool);
    printf("task num = %d\n", pstQueue->taskNum);
    int ret;
    Task* pstTask = NULL;
    Task* pstTmp = NULL;
    if (pstQueue->pstMemPool != NULL) {
        pstTask = pstQueue->pstMemPool;
        while (pstTask != NULL) {
            pstTmp = pstTask->pstNext;
            free(pstTask);
            pstTask = pstTmp;
        }
    }
    ret = pthread_cond_destroy(&pstQueue->stConditEmpty);
    if (ret != 0) {
        perror("empty");
    }
    ret = pthread_cond_destroy(&pstQueue->stConditNotFull);
    if (ret != 0) {
        perror("not full");
    }
    ret = pthread_cond_destroy(&pstQueue->stConditNotEmpty);
    if (ret != 0) {
        perror("not empty");
    }
    free(pstQueue);
    return THREADPOOL_OK;
}

int TaskQueue_AddTask(TaskQueue* pstQueue, void* (*pfExecute)(void* pvArg), void* pvArg)
{
    if (pstQueue == NULL) {
        return THREADPOOL_NULL;
    }
    if (pfExecute == NULL) {
        return THREADPOOL_NULL;
    }
    /* if task queue is full, wait */
    while (pstQueue->taskNum >= pstQueue->maxTaskNum) {
        printf("%s\n", "wait queue not full");
        pthread_cond_wait(&pstQueue->stConditNotFull, pstQueue->pstLock);
    }
    /* add task */
    Task* pstTask = TaskQueue_NewTask(pstQueue, pfExecute, pvArg);
    if (pstTask == NULL) {
        return THREADPOOL_MEM_ERR;
    }
    if (pstQueue->taskNum < 1) {
        pstQueue->pstHead = pstTask;
        pstQueue->pstTail = pstQueue->pstHead;
    } else {
        pstQueue->pstTail->pstNext = pstTask;
        pstQueue->pstTail = pstTask;
    }
    pstQueue->taskNum++;
    /* if task queue is not empty, broadcast all threads to get task */
    if (pstQueue->taskNum > 0) {
        pthread_cond_broadcast(&pstQueue->stConditNotEmpty);
    }
    return THREADPOOL_OK;
}

Task* TaskQueue_GetTask(TaskQueue* pstQueue, char* pcPoolState)
{
    /* if task queue is empty, wait */
    while (pstQueue->taskNum < 1 && (*pcPoolState == THREADPOOL_RUNNING)) {
        printf("%s\n", "wait queue not empty");
        pthread_cond_wait(&pstQueue->stConditNotEmpty, pstQueue->pstLock);
    }
    /* get task */
    if (pstQueue->pstHead == NULL) {
        printf("state = %x\n", *pcPoolState);
        return NULL;
    }
    Task* pstTask = NULL;
    pstTask = pstQueue->pstHead;
    pstQueue->pstHead = pstQueue->pstHead->pstNext;
    pstQueue->taskNum--;
    /* if task num < max task num, broadcast all threads to get task */
    if (pstQueue->taskNum < pstQueue->maxTaskNum) {
        pthread_cond_broadcast(&pstQueue->stConditNotFull);
    }
    /* if task queue is empty, signal to close task queue */
    if (pstQueue->taskNum < 1) {
        pthread_cond_signal(&pstQueue->stConditEmpty);
    }
    return pstTask;
}
void TaskQueue_Recycle(TaskQueue* pstQueue, Task* pstTask)
{
    pstTask->pfExecute = NULL;
    pstTask->pvArg = NULL;
    pstTask->pstNext = NULL;
    /* reuse memory */
    pstTask->pstNext = pstQueue->pstMemPool;
    pstQueue->pstMemPool = pstTask;
    pstQueue->taskNumOfPool++;
    return;
}
void TaskQueue_Close(TaskQueue* pstQueue)
{
    pthread_cond_broadcast(&pstQueue->stConditNotFull);
    pthread_cond_broadcast(&pstQueue->stConditNotEmpty);
    while (pstQueue->taskNum > 0) {
        printf("tasknum = %d, %s\n", pstQueue->taskNum, "waiting queue empty");
        pthread_cond_wait(&pstQueue->stConditEmpty, pstQueue->pstLock);
    }
    return;
}

void ThreadPool_AddThread(ThreadPool* pstPool, int threadNum)
{
    int ret;
    int i;
    for (i = 0; i < threadNum; i++) {
        Thread* pstThread = NULL;
        pstThread = (Thread*)malloc(sizeof(Thread));
        if (pstThread == NULL) {
            THREADPOOL_MESSAGE("fail to allocte thread");
            continue;
        }
        pstThread->pstNext = pstPool->pstThreadList;
        pstPool->pstThreadList = pstThread;
        pstPool->threadNum++;
        ret = pthread_create(&pstThread->tid, &pstPool->stThreadAttr, ThreadPool_Working, (void*)pstPool);
        if (ret != 0) {
            THREADPOOL_MESSAGE("fail to create thread");
        }
    }
    return;
}

ThreadPool* ThreadPool_New(int minThreadNum, int maxThreadNum, int maxTaskQueueLen)
{
    if (minThreadNum < 1 || maxThreadNum < 1 || maxTaskQueueLen < 1) {
        return NULL;
    }
    if (minThreadNum > maxThreadNum) {
        return NULL;
    }
    ThreadPool* pstPool = NULL;
    do {
        pstPool = (ThreadPool*)malloc(sizeof(ThreadPool));
        if (pstPool == NULL) {
            THREADPOOL_MESSAGE("fail to create pool");
            break;
        }
        int ret;
        ret = pthread_attr_init(&pstPool->stThreadAttr);
        if (ret != 0) {
            THREADPOOL_MESSAGE("fail to init thread atttibute");
            break;
        }
        ret = pthread_mutex_init(&pstPool->stLock, NULL);
        if (ret != 0) {
            THREADPOOL_MESSAGE("fail to init lock");
            break;
        }
        ret = pthread_cond_init(&pstPool->stConditStart, NULL);
        if (ret != 0) {
            THREADPOOL_MESSAGE("fail to init condition");
            break;
        }
        ret = pthread_cond_init(&pstPool->stConditZeroThread, NULL);
        if (ret != 0) {
            THREADPOOL_MESSAGE("fail to init condition");
            break;
        }
        /* task queue */
        TaskQueue* pstQueue = TaskQueue_New(maxTaskQueueLen, &pstPool->stLock);
        if (pstQueue == NULL) {
            THREADPOOL_MESSAGE("fail to create task queue");
            break;
        }
        pstPool->pstTaskQueue = pstQueue;
        pstPool->minThreadNum = minThreadNum;
        pstPool->maxThreadNum = maxThreadNum;
        pstPool->threadNum = 0;
        pstPool->state = THREADPOOL_STOP;
        pstPool->pstThreadList = NULL;
        /* set thread attribute */
        ret = pthread_attr_setdetachstate(&pstPool->stThreadAttr, PTHREAD_CREATE_DETACHED);
        if (ret != 0) {
            THREADPOOL_MESSAGE("fail to set attribute of thread");
            break;
        }
        /* create thread */
        ThreadPool_AddThread(pstPool, minThreadNum);
        /* create admin thread */
        ret = pthread_create(&pstPool->stAdminThread, &pstPool->stThreadAttr, ThreadPool_Admin, (void*)pstPool);
        if (ret != 0) {
            THREADPOOL_MESSAGE("fail to create thread");
        }
        pstPool->threadNum++;
        return pstPool;
    } while (0);
    ThreadPool_Delete(pstPool);
    return NULL;
}

int ThreadPool_Delete(ThreadPool* pstPool)
{
    if (pstPool == NULL) {
        return THREADPOOL_NULL;
    }
    pthread_mutex_lock(&pstPool->stLock);
    pthread_mutex_destroy(&pstPool->stLock);
    pthread_attr_destroy(&pstPool->stThreadAttr);
    pthread_cond_destroy(&pstPool->stConditStart);
    pthread_cond_destroy(&pstPool->stConditZeroThread);
    if (pstPool->pstTaskQueue != NULL) {
        TaskQueue_Delete(pstPool->pstTaskQueue);
    }
    Thread* pstThread = pstPool->pstThreadList;
    Thread* pstTmp = NULL;
    while (pstThread != NULL) {
        pstTmp = pstThread->pstNext;
        free(pstThread);
        pstThread = pstTmp;
    }
    free(pstPool);
    return THREADPOOL_OK;
}

int ThreadPool_AddTask(ThreadPool* pstPool, void* (*pfExecute)(void* pvArg), void* pvArg)
{
    if (pstPool == NULL) {
        return THREADPOOL_NULL;
    }
    if (pfExecute == NULL) {
        return THREADPOOL_NULL;
    }
    int ret;
    pthread_mutex_lock(&pstPool->stLock);
    if (pstPool->state == THREADPOOL_TERMINATED) {
        pthread_mutex_unlock(&pstPool->stLock);
        return THREADPOOL_OK;
    }
    ret = TaskQueue_AddTask(pstPool->pstTaskQueue, pfExecute, pvArg);
    pthread_mutex_unlock(&pstPool->stLock);
    return ret;
}

int ThreadPool_Start(ThreadPool* pstPool)
{
    if (pstPool == NULL) {
        return THREADPOOL_NULL;
    }
    pthread_mutex_lock(&pstPool->stLock);
    pstPool->state = THREADPOOL_RUNNING;
    pthread_cond_broadcast(&pstPool->stConditStart);
    pthread_mutex_unlock(&pstPool->stLock);
    return THREADPOOL_OK;	
}

int ThreadPool_Stop(ThreadPool* pstPool)
{
    if (pstPool == NULL) {
        return THREADPOOL_NULL;
    }
    pthread_mutex_lock(&pstPool->stLock);
    TaskQueue_Close(pstPool->pstTaskQueue);
    pstPool->state = THREADPOOL_STOP;
    pthread_mutex_unlock(&pstPool->stLock);
    return THREADPOOL_OK;	
}

int ThreadPool_Shutdown(ThreadPool* pstPool)
{
    if (pstPool == NULL) {
        return THREADPOOL_NULL;
    }
    pthread_mutex_lock(&pstPool->stLock);
    TaskQueue_Close(pstPool->pstTaskQueue);
    pstPool->state = THREADPOOL_TERMINATED;
    /* wake up all threads to exit */
    printf("thread num = %d\n", pstPool->threadNum);
    pthread_cond_broadcast(&pstPool->stConditStart);
    while (pstPool->threadNum > 0) {
        pthread_cond_wait(&pstPool->stConditZeroThread, &pstPool->stLock);
    }
    pthread_mutex_unlock(&pstPool->stLock);
    return THREADPOOL_OK;	
}

int ThreadPool_GetTaskNum(ThreadPool* pstPool)
{
    if (pstPool == NULL || pstPool->pstTaskQueue == NULL) {
        return -1;
    }
    int taskNum = 0;
    pthread_mutex_lock(&pstPool->stLock);
    taskNum = pstPool->pstTaskQueue->taskNum;
    pthread_mutex_unlock(&pstPool->stLock);
    return taskNum;
}

int ThreadPool_GetLock(ThreadPool* pstPool, pthread_mutex_t** ppstLock)
{
    if (pstPool == NULL) {
        return THREADPOOL_NULL;
    }
    pthread_mutex_lock(&pstPool->stLock);
    *ppstLock = &(pstPool->stLock);
    pthread_mutex_unlock(&pstPool->stLock);
    return THREADPOOL_OK;
}
int ThreadPool_AddThreadWithLock(ThreadPool* pstPool, int threadNum)
{
    if (pstPool == NULL) {
        return THREADPOOL_NULL;
    }
    if (threadNum > (pstPool->maxThreadNum - pstPool->threadNum)) {
        return THREADPOOL_SIZE_ERR;
    }
    pthread_mutex_lock(&pstPool->stLock);
    ThreadPool_AddThread(pstPool, threadNum);
    pthread_mutex_unlock(&pstPool->stLock);
    return THREADPOOL_OK;
}

void* ThreadPool_Working(void* pvPool)
{
    ThreadPool* pstPool = (ThreadPool*)pvPool;
    while (1) {
        pthread_mutex_lock(&pstPool->stLock);
        /* wait signal to start */
        while (pstPool->state == THREADPOOL_STOP) {
            printf("%s\n", "waiting start");
            pthread_cond_wait(&pstPool->stConditStart, &pstPool->stLock);
        }
        /* get task */
        Task* pstTask = TaskQueue_GetTask(pstPool->pstTaskQueue, &pstPool->state);
        /* execute task */
        if (pstTask != NULL && pstTask->pfExecute != NULL) {
            pstTask->pfExecute(pstTask->pvArg);
            TaskQueue_Recycle(pstPool->pstTaskQueue, pstTask);
        }
        /* descrese thread when pool is not busy */
        if (pstPool->pstTaskQueue->taskNum < pstPool->pstTaskQueue->maxTaskNum / 2 &&
                pstPool->threadNum > pstPool->minThreadNum) {
            pstPool->threadNum--;
            pthread_mutex_unlock(&pstPool->stLock);
            break;
        }
        /* when all tasks have been executed */
        if (pstPool->pstTaskQueue->taskNum == 0) {
            /* exit */
            if (pstPool->state == THREADPOOL_TERMINATED) {
                pstPool->threadNum--;
                if (pstPool->threadNum == 0) {
                    pthread_cond_signal(&pstPool->stConditZeroThread);
                }
                pthread_mutex_unlock(&pstPool->stLock);
                break;
            } else {
                /* wait */
                pstPool->state = THREADPOOL_STOP;
            }
        }
        pthread_mutex_unlock(&pstPool->stLock);
    }
    return NULL;
}

void* ThreadPool_Admin(void* pvPool)
{
    ThreadPool* pstPool = (ThreadPool*)pvPool;
    while (1) {
        pthread_mutex_lock(&pstPool->stLock);
        /* wake up all thread when queue is not empty */
        if (pstPool->state == THREADPOOL_STOP && pstPool->pstTaskQueue->taskNum > 0) {
            pstPool->state = THREADPOOL_RUNNING;
            pthread_cond_broadcast(&pstPool->stConditStart);
        }
        /* increase thread when pool is busy */
        if (pstPool->pstTaskQueue->taskNum == pstPool->pstTaskQueue->maxTaskNum &&
                pstPool->threadNum < pstPool->maxThreadNum) {
            ThreadPool_AddThread(pstPool, 1);
        }
        /* if pool is shutdown, exit */
        if (pstPool->state == THREADPOOL_TERMINATED) {
            pstPool->threadNum--;
            if (pstPool->threadNum == 0) {
                pthread_cond_signal(&pstPool->stConditZeroThread);
            }
            pthread_mutex_unlock(&pstPool->stLock);
            break;
        }
        pthread_mutex_unlock(&pstPool->stLock);
        usleep(100);
    }
    return NULL;
}
