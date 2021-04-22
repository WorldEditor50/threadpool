# 线程池

## 1. 分类

- 概述
  - 线程池是对系统线程资源的复用，可以避免线程多次创建与销毁，提高程序的运行效率

- 按同步方法分类
  - 有锁
  - 无锁
- 按任务分类
  - CPU密集
  - IO密集
  - 混合型

## 2. 原理

### 2. 1线程池的状态(java)

- RUNNING

  - 状态说明：线程池处在RUNNING状态时，能够接收新任务，以及对已添加的任务进行处理。 
  - 状态切换：线程池的初始化状态是RUNNING。换句话说，线程池被一旦被创建，就处于RUNNING状态，并且线程池中的任务数为0

- STOP

  - 状态说明：线程池处在STOP状态时，不接收新任务，不处理已添加的任务，并且会中断正在处理的任务。 
  - 状态切换：调用线程池的shutdownNow()接口时，线程池由(RUNNING or SHUTDOWN ) -> STOP。

- SHUTDOWN

  - 状态说明：线程池处在SHUTDOWN状态时，不接收新任务，但能处理已添加的任务。 
  - 状态切换：调用线程池的shutdown()接口时，线程池由RUNNING -> SHUTDOWN。

- TIDYING

  - 状态说明：当所有的任务已终止，ctl记录的”任务数量”为0，线程池会变为TIDYING状态。当线程池变为TIDYING状态时，会执行钩子函数terminated()。terminated()在ThreadPoolExecutor类中是空的，若用户想在线程池变为TIDYING时，进行相应的处理；可以通过重载terminated()函数来实现。 
  - 状态切换：当线程池在SHUTDOWN状态下，阻塞队列为空并且线程池中执行的任务也为空时，就会由 SHUTDOWN -> TIDYING。 

- TERMINATED

  - 状态说明：线程池彻底终止，就变成TERMINATED状态。 
  - 状态切换：线程池处在TIDYING状态时，执行完terminated()之后，就会由 TIDYING -> TERMINATED。

### 2.2 线程数量

假设要求一个系统的TPS（Transaction Per Second或者Task Per Second）至少为20，然后假设每个Transaction由一个线程完成，继续假设平均每个线程处理一个Transaction的时间为4s。那么问题转化为：

**如何设计线程池大小，使得可以在1s内处理完20个Transaction？**

计算过程很简单，每个线程的处理能力为0.25TPS，那么要达到20TPS，显然需要20/0.25=80个线程。

很显然这个估算方法很天真，因为它没有考虑到CPU数目。一般服务器的CPU核数为16或者32，如果有80个线程，那么肯定会带来太多不必要的线程上下文切换开销。
 
 再来第二种简单的但不知是否可行的方法（N为CPU总核数）：

- 如果是CPU密集型应用，则线程池大小设置为N+1
- 如果是IO密集型应用，则线程池大小设置为2N+1

如果一台服务器上只部署这一个应用并且只有这一个线程池，那么这种估算或许合理，具体还需自行测试验证。

接下来在这个文档：服务器性能IO优化 中发现一个估算公式：

| `1`  | `最佳线程数目 = （（线程等待时间+线程CPU时间）/线程CPU时间 ）* CPU数目` |
| ---- | ------------------------------------------------------------ |
|      |                                                              |

比如平均每个线程CPU运行时间为0.5s，而线程等待时间（非CPU运行时间，比如IO）为1.5s，CPU核心数为8，那么根据上面这个公式估算得到：((0.5+1.5)/0.5)*8=32。这个公式进一步转化为：

| `1`  | `最佳线程数目 = （线程等待时间与线程CPU时间之比 + 1）* CPU数目` |
| ---- | ------------------------------------------------------------ |
|      |                                                              |

可以得出一个结论：

**线程等待时间所占比例越高，需要越多线程。线程CPU时间所占比例越高，需要越少线程。**

上一种估算方法也和这个结论相合。

一个系统最快的部分是CPU，所以决定一个系统吞吐量上限的是CPU。增强CPU处理能力，可以提高系统吞吐量上限。但根据短板效应，真实的系统吞吐量并不能单纯根据CPU来计算。那要提高系统吞吐量，就需要从“系统短板”（比如网络延迟、IO）着手：

- 尽量提高短板操作的并行化比率，比如多线程下载技术
- 增强短板能力，比如用NIO替代IO

第一条可以联系到Amdahl定律，这条定律定义了串行系统并行化后的加速比计算公式：

| `1`  | `加速比=优化前系统耗时 / 优化后系统耗时` |
| ---- | ---------------------------------------- |
|      |                                          |

加速比越大，表明系统并行化的优化效果越好。Addahl定律还给出了系统并行度、CPU数目和加速比的关系，加速比为Speedup，系统串行化比率（指串行执行代码所占比率）为F，CPU数目为N：

| `1`  | `Speedup <=  1` `/ (F + (1 - F)/N)` |
| ---- | ----------------------------------- |
|      |                                     |

当N足够大时，串行化比率F越小，加速比Speedup越大。

### 2.3 线程回收

在任何一个时间点上，线程是可结合的（joinable），或者是分离的（detached）。**一个可结合的线程能够被其他线程收回其资源和杀死；在被其他线程回收之前，它的存储器资源（如栈）是不释放的。相反，一个分离的线程是不能被其他线程回收或杀死的，它的存储器资源在它终止时由系统自动释放。** 

默认情况下，线程被创建成可结合的。为了避免存储器泄漏，每个可结合线程都应该要么被显示地回收，即调用pthread_join；要么通过调用pthread_detach函数被分离。如果一个可结合线程结束运⾏行但没有被join，则它的状态类似于进程中的Zombie Process，  即还有一部分资源没有被回收，所以创建线程者应该调用pthread_join来等待线程运行结束，并可得到线程的退出代码，回收其资源。  由于调用pthread_join后，如果该线程没有运行结束，调用者会被阻塞，在有些情况下我们并不希望如此。例如，在Web服务器中当主线程为每个新来的连接请求创建一个子线程进 行处理的时候，主线程并不希望因为调用pthread_join而阻塞（因为还要继续处理之后到来 的连接请求）， 这时可以在子线程中加入代码：

```
pthread_detach(pthread_self())；
```

或者父线程调用时加入：

```
pthread_detach(thread_id)
```

这将该子线程的状态设置为分离的（detached），如此一来，该线程运⾏行结束后会自动释 放所有资源。

- 设置线程分离属性pthread_attr_setdetachstate
- 线程内调用pthread_detach(pthread_self())
- 调用pthread_join

### 2.4 线程同步方法

- 线程共享的资源

  - CPU ：由系统调度，调度策略可以通过pthread_attr_setschedpolicy设置，优先级可通过pthread_attr_setschedparam设置

  - 任务队列：线程互斥访问

- 信号量

  ```c
  #include <pthread.h>
  #include <stdio.h>
  #include <unistd.h>
  #include <semaphore.h>
  int g_iNum = 0;
  char p1[10] = "pthread1";
  char p2[10] = "pthread2";
  sem_t g_stSem;
  void* counting(void* arg)
  {
      char* p = (char*)arg;
      while(g_iNum < 40) {
          if (sem_wait(&g_stSem) == 0) { // semaphore  minus 1 by atomic operation
              g_iNum++;
              printf("g_iNum = %d id = %s\n", g_iNum,p);
              sem_post(&g_stSem); // semaphore plus 1 by atomic operation
          }
      }
      return NULL;
  }
  int main()
  {
      pthread_t tid1;
      pthread_t tid2;
      /* create semaphore */
      sem_init(&g_stSem, 0, 2);
      /* create pthread */
      pthread_create(&tid1, NULL, counting, (void*)p1);
      pthread_create(&tid2, NULL, counting, (void*)p2);
      sleep(1);
      /* recycle thread */
      pthread_join(tid1, NULL);
      pthread_join(tid2, NULL);
      printf("g_iNum = %d\n", g_iNum);
      /* destroy semaphore */
      sem_destroy(&g_stSem);
      return 0;
  }
  ```

- 互斥锁

  ```c
  #include <pthread.h>
  #include <stdio.h>
  #include <unistd.h>
  int g_iNum  =  0;
  char p1[10] = "pthread1";
  char p2[10] = "pthread2";
  pthread_mutex_t g_stMutex;
  void* counting(void* arg)
  {
      char* p = (char*)arg;
      while (g_iNum < 0) {
          /* lock */
          pthread_mutex_lock(&g_stMutex);
          g_iNum++;
          printf("num=%d id=%s\n", g_iNum, p);
          /* unlock */
          pthread_mutex_unlock(&g_stMutex);
      }
      return NULL;
  }
  int main()
  {
      pthread_t tid1;
      pthread_t tid2;
      /* init mutex */
      pthread_mutex_init(&g_stMutex, NULL);
      /* create thread */
      pthread_create(&tid1,  NULL,  counting,  (void*)p1);
      pthread_create(&tid2,  NULL,  counting,  (void*)p2);
      sleep(1);
      /* recycle thread */
      pthread_join(tid1, NULL);
      pthread_join(tid2, NULL);
      printf("num=%d\n", g_iNum);
      /* destroy mutex */
      pthread_mutex_destroy(&g_stMutex);
      return 0;
  }
  
  ```

  

- 区别

  - 互斥锁只允许一个线程进入临界区，而信号量允许多个线程同时进入临界区。

### 2.5 线程调度与优先级

#### 2.5.1 调度策略

- SCHED_OTHER：分时调度策略。

它是默认的线程分时调度策略，所有的线程的优先级别都是0，线程的调度是通过分时来完成的。简单地说，如果系统使用这种调度策略，程序将无法设置线程的优先级。请注意，这种调度策略也是抢占式的，当高优先级的线程准备运行的时候，当前线程将被抢占并进入等待队列。这种调度策略仅仅决定线程在可运行线程队列中的具有相同优先级的线程的运行次序。

- SCHED_FIFO：实时调度策略，先到先服务。一旦占用cpu则一直运行。一直运行直到有更高优先级任务到达或自己放弃。

它是一种实时的先进先出调用策略，且只能在超级用户下运行。这种调用策略仅仅被使用于优先级大于0的线程。它意味着，使用SCHED_FIFO的可运行线程将一直抢占使用SCHED_OTHER的运行线程J。此外SCHED_FIFO是一个非分时的简单调度策略，当一个线程变成可运行状态，它将被追加到对应优先级队列的尾部。当所有高优先级的线程终止或者阻塞时，它将被运行。对于相同优先级别的线程，按照简单的先进先运行的规则运行。我们考虑一种很坏的情况，如果有若干相同优先级的线程等待执行，然而最早执行的线程无终止或者阻塞动作，那么其他线程是无法执行的，除非当前线程调用如pthread_yield之类的函数，所以在使用SCHED_FIFO的时候要小心处理相同级别线程的动作。

- SCHED_RR：实时调度策略，时间片轮转。当进程的时间片用完，系统将重新分配时间片，并置于就绪队列尾。放在队列尾保证了所有具有相同优先级的RR任务的调度公平。

鉴于SCHED_FIFO调度策略的一些缺点，SCHED_RR对SCHED_FIFO做出了一些增强功能。从实质上看，它还是SCHED_FIFO调用策略。它使用最大运行时间来限制当前进程的运行，当运行时间大于等于最大运行时间的时候，当前线程将被切换并放置于相同优先级队列的最后。这样做的好处是其他具有相同级别的线程能在“自私“线程下执行。

系统创建线程时，默认的线程是SCHED_OTHER。所以如果我们要改变线程的调度策略的话，可以通过下面的这个函数实现：

```
int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy);
```

#### 2.5.2 Linux线程优先级设置

首先，可以通过以下两个函数来获得线程可以设置的最高和最低优先级，函数中的策略即上述三种策略的宏定义：

```
int sched_get_priority_max(int policy);
int sched_get_priority_min(int policy);
```

SCHED_OTHER是不支持优先级使用的，而SCHED_FIFO和SCHED_RR支持优先级的使用，他们分别为1和99，数值越大优先级越高。

设置和获取优先级通过以下两个函数：

```
int pthread_attr_setschedparam(pthread_attr_t *attr, const struct sched_param *param);
int pthread_attr_getschedparam(const pthread_attr_t *attr, struct sched_param *param);
```

上面的param使用了下面的这个数据结构：

```
struct sched_param
{
    int __sched_priority; //所要设定的线程优先级
};
```

## 3. 实现

### 3.1 生产者-消费者模型

```
Thread Pool     
 +----------------------------------------------------------------------------------------------+
 |    customer :  Threads                                                                       |
 |              +------------+------------+---------- +--------+------------+                   |
 |              | thread1    | thread2    | thread3   | ...... |  threadn   |                   |
 |              +------------+------------+---------- +--------+------------+                   |
 |              admin thread                                                                    |
 |              +------------+                                                                  |
 |              | admin      |                                                                  |
 |              +------------+                                                                  |
 |                     condition &                                               condition &    |
 |                     mutex lock                                                 mutex lock    |                    
 |  idle thread         |           Task Queue                                             |    |
 |       get task       |          +--------+--------+--------+--------+--------+          |    |  add task 
 |              <--------------    | task1  | task2  | task3  | ...... |  taskn | <------------------------- producer
 |                      |          +--------+--------+--------+--------+--------+          |    |
 +----------------------------------------------------------------------------------------------+
```

### 3.2 线程池状态

- STOP
  - 线程创建后默认为STOP状态，线程处于阻塞中，当有任务到来时，管理线程会通知所有线程去获取任务，所有线程由STOP状态跳转至RUNNING状态。
  - 当所有任务完全执行完毕，线程会RUNNING状态跳转至STOP状态。
  - 用户主动调用ThreadPool_Stop，所有线程将会跳转到STOP状态。
- RUNNING
  - 当有任务到来时，管理线程会通知所有线程去获取任务，线程由STOP状态跳转至RUNNING状态。
  - 用户主动调用ThreadPool_Start，所有线程将会跳转到RUNNING状态。
- TERMINATED
  - 用户主动调用ThreadPool_Shutdown，所有线程将会从STOP状态跳转至RUNNING，从RUNNING状态跳转到TREMINATED状态。

### 3.3 任务队列状态

- EMPTY

  当所有任务执行完成，没有添加新任务时，队列为空，此时会发信号给主线程停止等待关闭队列

- NOT FULL

  当队列有任务但数量小于队列容量时，发广播通知所有线程去获取任务

- FULL

  当任务队列已满时，阻塞添加任务，直到队列未满

### 3.4 执行流程

```
 main thread                                create pool 
 +-----------------------+                  +------------------------------+
 |    1. create pool     |                  |    1. create task's queue    |
 |    2. start           |                  |    2. create threads         |
 |    3. add task        |                  |    3. create admin thread    |
 |    4. shutdown        |                  |    4. create condition       |
 |    5. destroy pool    |                  |    5. create mutex lock      |
 +-----------------------+                  +------------------------------+
 
 add task                                   get task from queue
 +---------------------------------+        +---------------------------------------------+ 
 |    1. wait when queue is full   |        |    1. wait when queue is empty              | 
 |    2. add task                  |        |    2. get task                              | 
 |    3. queue is not empty ,      |        |    3. broadcast all threads to              | 
 |       broadcast all threads to  |        |       get task when queue is not full       | 
 |       get task                  |        |    4. signal to pool when queue is empty    | 
 +---------------------------------+        +---------------------------------------------+ 
 
 admin thread                               thread
 +---------------------------------+        +---------------------------------------------+
 |    1. wake up all thread when   |        |    1. wait condition of start               |
 |       queue is not empty        |        |    2. get task from queue                   |
 |    2. increase thread when      |        |    3. execute task                          |
 |       queue is full             |        |    4. descease thread when pool is not busy |
 |    3. exit when pool is         |        |    5. sleep when queue is empty             |
 |       ready to shutdown         |        |    6. exit when pool is ready to shutdown   |
 +---------------------------------+        +---------------------------------------------+

```

### 3.3 数据结构

```c
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
```

## 4. 总结

- 互斥锁的使用
  - 尽量避免多次上锁解锁，相关联的逻辑应尽量放在一次上锁解锁内
  - 同一个锁不能嵌套上锁
  - 释放条件变量前应先销毁互斥锁
- 任务执行时间
  - 任务执行时间不能过长，否则线程占用CPU时间过长会失去并发效果

**参考：**

- http://ifeve.com/how-to-calculate-threadpool-size/
- https://www.cnblogs.com/-wyl/p/9760670.html
- https://www.cnblogs.com/xiaojianliu/p/9689118.html
- https://www.cnblogs.com/xiaojianliu/p/9689186.html









