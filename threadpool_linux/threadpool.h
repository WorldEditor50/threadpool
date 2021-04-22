#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#ifdef __cplusplus
extern "C" {
#endif
enum THREADPOOL_ENUM {
    THREADPOOL_OK,
    THREADPOOL_ERR,
    THREADPOOL_NULL,
    THREADPOOL_SIZE_ERR,
    THREADPOOL_MEM_ERR
};
#define THREADPOOL_RUNNING          0x01
#define THREADPOOL_STOP             0x02
#define THREADPOOL_TERMINATED       0x03

#define THREADPOOL_MESSAGE(message) do { \
    printf("file: %s  function: %s  line: %d  message: %s\n", \
            __FILE__, __FUNCTION__, __LINE__, (message)); \
} while (0)
/* task */
typedef struct Task {
    void* (*pfExecute)(void* pvArg);
    void* pvArg;
    struct Task *pstNext;
} Task;
/* task queue */
typedef struct TaskQueue {
    Task* pstHead;
    Task* pstTail;
    Task* pstMemPool;
    int taskNum;
    int maxTaskNum;
    int taskNumOfPool;
    pthread_mutex_t* pstLock;
    pthread_cond_t stConditEmpty;
    pthread_cond_t stConditNotEmpty;
    pthread_cond_t stConditNotFull;
} TaskQueue;
typedef struct Thread {
    pthread_t tid;
    struct Thread* pstNext;
} Thread;
/* thread pool */
typedef struct ThreadPool {
    pthread_t stAdminThread;
    Thread* pstThreadList;
    TaskQueue* pstTaskQueue;
    pthread_attr_t stThreadAttr;
    pthread_mutex_t stLock;
    pthread_cond_t stConditStart;
    pthread_cond_t stConditZeroThread;
    int threadNum;
    int minThreadNum;
    int maxThreadNum;
    char state;
} ThreadPool;
Task* TaskQueue_NewTask(TaskQueue* pstQueue, void* (*pfExecute)(void* pvArg), void* pvArg);
TaskQueue* TaskQueue_New(int maxTaskNum, pthread_mutex_t* pstLock);
int TaskQueue_Delete(TaskQueue* pstQueue);
int TaskQueue_AddTask(TaskQueue* pstQueue, void* (*pfExecute)(void* pvArg), void* pvArg);
Task* TaskQueue_GetTask(TaskQueue* pstQueue, char* pcPoolState);
void TaskQueue_Recycle(TaskQueue* pstQueue, Task* pstTask);
void TaskQueue_Close(TaskQueue* pstQueue);
void* ThreadPool_Working(void* pvPool);
void* ThreadPool_Admin(void* pvPool);
/* api */
ThreadPool* ThreadPool_New(int minThreadNum, int maxThreadNum, int maxTaskQueueLen);
int ThreadPool_Delete(ThreadPool* pstPool);
int ThreadPool_AddTask(ThreadPool* pstPool, void* (*pfExecute)(void* pvArg), void* pvArg);
int ThreadPool_Start(ThreadPool* pstPool);
int ThreadPool_Stop(ThreadPool* pstPool);
int ThreadPool_Shutdown(ThreadPool* pstPool);
int ThreadPool_GetTaskNum(ThreadPool* pstPool);
int ThreadPool_GetLock(ThreadPool* pstPool, pthread_mutex_t** pstLock);
void ThreadPool_AddThread(ThreadPool* pstPool, int threadNum);
int ThreadPool_AddThreadWithLock(ThreadPool* pstPool, int threadNum);
#ifdef __cplusplus
}
#endif
#endif // THREADPOOL_H

