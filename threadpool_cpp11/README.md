# threadpool wrote in c++11
- 支持类的成员函数与类的实例载入任务
## 智能指针
- shared_ptr

- unique_ptr

- weak_ptr

## 线程类thread

## 条件变量与互斥锁
- condition_variable

- mutex

## 总结
- 多个互斥锁嵌套使用容易导致死锁
- 不能直接使用vector存放线程类thread, vector的扩容机制会极大地消耗线程资源
