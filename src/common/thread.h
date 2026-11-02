/*
 * Internal portable threading primitives (POSIX threads / Win32).
 * Build with -DNYXJ_NO_THREADS (e.g. WebAssembly) to turn them into no-ops.
 */
#ifndef NYXJ_THREAD_H
#define NYXJ_THREAD_H

#include <stdbool.h>
#include <stdint.h>

#if defined(NYXJ_NO_THREADS)

typedef int nyxj_mutex;
typedef int nyxj_cond;
typedef int nyxj_thread;
#define NYXJ_MUTEX_INITIALIZER 0

#elif defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
typedef SRWLOCK nyxj_mutex;
typedef CONDITION_VARIABLE nyxj_cond;
typedef HANDLE nyxj_thread;
#define NYXJ_MUTEX_INITIALIZER SRWLOCK_INIT

#else

#include <pthread.h>
typedef pthread_mutex_t nyxj_mutex;
typedef pthread_cond_t nyxj_cond;
typedef pthread_t nyxj_thread;
#define NYXJ_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER

#endif

typedef void* (*nyxj_thread_fn)(void* arg);

void nyxj_mutex_init(nyxj_mutex* m);
void nyxj_mutex_lock(nyxj_mutex* m);
void nyxj_mutex_unlock(nyxj_mutex* m);
void nyxj_mutex_destroy(nyxj_mutex* m);

void nyxj_cond_init(nyxj_cond* c);
void nyxj_cond_wait(nyxj_cond* c, nyxj_mutex* m);
bool nyxj_cond_timedwait(nyxj_cond* c, nyxj_mutex* m, unsigned ms); /* false on timeout */
void nyxj_cond_signal(nyxj_cond* c);
void nyxj_cond_broadcast(nyxj_cond* c);
void nyxj_cond_destroy(nyxj_cond* c);

bool nyxj_thread_create(nyxj_thread* t, nyxj_thread_fn fn, void* arg);
void nyxj_thread_join(nyxj_thread t);

int      nyxj_cpu_count(void);
uint64_t nyxj_now_ms(void);  /* Monotonic milliseconds */
uint64_t nyxj_now_ns(void);  /* Monotonic nanoseconds */
uint64_t nyxj_wall_ms(void); /* Unix epoch milliseconds */
void     nyxj_sleep_ms(unsigned ms);

#endif
