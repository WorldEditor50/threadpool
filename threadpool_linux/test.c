#include "threadpool.h"
unsigned long g_ulNum = 0;
void* hello(void* word)
{
	char* str = (char*)word;
    pthread_t id = pthread_self();
	printf("tid = %lu, %s\n", id, str);
	return NULL;
}
void* heavy(void* word)
{
	char* str = (char*)word;
	g_ulNum++;
	printf("%s,  %lu\n", str, g_ulNum);
	sleep(1);
	return NULL;
}

void test_hello()
{
	int ret = 0;
	unsigned long  i = 0;
	/* create pool */
	ThreadPool *pstPool = NULL;
	pstPool = ThreadPool_New(4, 16, 500);
	if (pstPool == NULL) {
		printf("%s\n","pool create failed");
		return;
	}
	printf("%s\n","create pool");
	/* thread pool start */
	ThreadPool_Start(pstPool);
	/* add task */
	for(i = 0; i < 200000; i++) {
		ret = ThreadPool_AddTask(pstPool,hello,"hello");
		if(ret != THREADPOOL_OK) {
			printf("%s\n","pool add task failed");
		}
	}
	sleep(2);
	printf("%s\n","added task");
	ret = ThreadPool_Shutdown(pstPool);
	if (ret != THREADPOOL_OK) {
		return;
	}
	printf("%s\n","pool shutdown");
	/* release pool */
	ret = ThreadPool_Delete(pstPool);
	if(ret != THREADPOOL_OK) {
		printf("%s\n","pool create failed");
	}
	printf("%s\n","destroy pool");
	return;
}

void test_heavy()
{
	int ret = 0;
	unsigned long  i = 0;
	/* create pool */
	ThreadPool *pstPool = NULL;
	pstPool = ThreadPool_New(4, 16, 1000);
	if (pstPool == NULL) {
		printf("%s\n","pool create failed");
		return;
	}
	printf("%s\n","create pool");
	/* thread pool start */
	ThreadPool_Start(pstPool);
	/* add task */
	for(i = 0; i < 5000; i++) {
		ret = ThreadPool_AddTask(pstPool, heavy, "heavy");
		if(ret != THREADPOOL_OK) {
			printf("%s\n","pool add task failed");
		}
	}
	printf("%s\n","wait enter");
	sleep(10);
	printf("%s\n","added task");
	ret = ThreadPool_Shutdown(pstPool);
	if (ret != THREADPOOL_OK) {
		return;
	}
	printf("%s\n","pool shutdown");
	/* release pool */
	ret = ThreadPool_Delete(pstPool);
	if(ret != THREADPOOL_OK) {
		printf("%s\n","pool create failed");
	}
	printf("%s\n","destroy pool");
	return;
}

void test_restart()
{
	int ret = 0;
	unsigned long  i = 0;
	/* create pool */
	ThreadPool *pstPool = NULL;
	pstPool = ThreadPool_New(4, 16, 1000);
	if (pstPool == NULL) {
		printf("%s\n","pool create failed");
		return;
	}
	printf("%s\n","create pool");
	/* thread pool start */
	ThreadPool_Start(pstPool);
	/* add task */
	for(i = 0; i < 5000; i++) {
		ret = ThreadPool_AddTask(pstPool, hello, "hello");
		if(ret != THREADPOOL_OK) {
			printf("%s\n","pool add task failed");
		}
	}
	sleep(2);
	/* stop */
	ret = ThreadPool_Stop(pstPool);
	if (ret != THREADPOOL_OK) {
		return;
	}
	/* add task */
	for(i = 0; i < 20; i++) {
		ret = ThreadPool_AddTask(pstPool, heavy, "heavy");
		if(ret != THREADPOOL_OK) {
			printf("%s\n","pool add task failed");
		}
	}
	sleep(2);
	ThreadPool_Start(pstPool);
	printf("%s\n","added task");
	ret = ThreadPool_Shutdown(pstPool);
	if (ret != THREADPOOL_OK) {
		return;
	}
	printf("%s\n","pool shutdown");
	/* release pool */
	ret = ThreadPool_Delete(pstPool);
	if(ret != THREADPOOL_OK) {
		printf("%s\n","pool create failed");
	}
	printf("%s\n","destroy pool");
	return;
}

void test_wait()
{
	int ret = 0;
	unsigned long  i = 0;
	/* create pool */
	ThreadPool *pstPool = NULL;
	pstPool = ThreadPool_New(4, 16, 10);
	if (pstPool == NULL) {
		printf("%s\n","pool create failed");
		return;
	}
	printf("%s\n","create pool");
	/* thread pool start */
	ThreadPool_Start(pstPool);
	/* add task */
	for(i = 0; i < 20; i++) {
		ret = ThreadPool_AddTask(pstPool,hello,"hello");
		if(ret != THREADPOOL_OK) {
			printf("%s\n","pool add task failed");
		}
	}
	sleep(2);
	getchar();
	for(i = 0; i < 10; i++) {
		ret = ThreadPool_AddTask(pstPool,hello,"hello");
		if(ret != THREADPOOL_OK) {
			printf("%s\n","pool add task failed");
		}
	}
	printf("%s\n","added task");
	ret = ThreadPool_Shutdown(pstPool);
	if (ret != THREADPOOL_OK) {
		return;
	}
	printf("%s\n","pool shutdown");
	/* release pool */
	ret = ThreadPool_Delete(pstPool);
	if(ret != THREADPOOL_OK) {
		printf("%s\n","pool create failed");
	}
	printf("%s\n","destroy pool");
	return;
}
void test_addThread()
{
	int ret = 0;
	unsigned long  i = 0;
	/* create pool */
	ThreadPool *pstPool = NULL;
	pstPool = ThreadPool_New(4, 16, 1000);
	if (pstPool == NULL) {
		printf("%s\n","pool create failed");
		return;
	}
	printf("%s\n","create pool");
	/* thread pool start */
	ThreadPool_Start(pstPool);
	/* add task */
	for(i = 0; i < 5000; i++) {
		ret = ThreadPool_AddTask(pstPool,hello,"hello");
		if(ret != THREADPOOL_OK) {
			printf("%s\n","pool add task failed");
		}
	}
	ret = ThreadPool_AddThreadWithLock(pstPool, 4);
	if (ret != THREADPOOL_OK) {
		return;
	}
	for(i = 0; i < 10000; i++) {
		ret = ThreadPool_AddTask(pstPool,hello,"hello");
		if(ret != THREADPOOL_OK) {
			printf("%s\n","pool add task failed");
		}
	}
	sleep(2);
	printf("%s\n","added task");
	ret = ThreadPool_Shutdown(pstPool);
	if (ret != THREADPOOL_OK) {
		return;
	}
	printf("%s\n","pool shutdown");
	/* release pool */
	ret = ThreadPool_Delete(pstPool);
	if(ret != THREADPOOL_OK) {
		printf("%s\n","pool create failed");
	}
	printf("%s\n","destroy pool");
	return;
}
int main()
{
	test_hello();
	return 0;
}
