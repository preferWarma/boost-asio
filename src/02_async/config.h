#ifndef CONFIG_H_
#define CONFIG_H_

// 采用IO Service Pool 服务池
#define USE_IOSERVICE_POOL

// 采用协程(C++20)
#if __cplusplus >= 202002L
#define USE_COROUTINE
#endif

#endif
