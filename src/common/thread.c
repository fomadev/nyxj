#if !defined(_WIN32) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "thread.h"

#include <stdlib.h>
#include <time.h>

#if !defined(_WIN32)
#include <sys/time.h>
#include <unistd.h>
#endif

#if defined(NYXJ_NO_THREADS)

void nyxj_mutex_init(nyxj_mutex* m) { (void)m; }
void nyxj_mutex_lock(nyxj_mutex* m) { (void)m; }
void nyxj_mutex_unlock(nyxj_mutex* m) { (void)m; }
void nyxj_mutex_destroy(nyxj_mutex* m) { (void)m; }
void nyxj_cond_init(nyxj_cond* c) { (void)c; }
void nyxj_cond_wait(nyxj_cond* c, nyxj_mutex* m) { (void)c; (void)m; }
bool nyxj_cond_timedwait(nyxj_cond* c, nyxj_mutex* m, unsigned ms) { (void)c; (void)m; (void)ms; return false; }
void nyxj_cond_signal(nyxj_cond* c) { (void)c; }
void nyxj_cond_broadcast(nyxj_cond* c) { (void)c; }
void nyxj_cond_destroy(nyxj_cond* c) { (void)c; }
bool nyxj_thread_create(nyxj_thread* t, nyxj_thread_fn fn, void* arg) { (void)t; (void)fn; (void)arg; return false; }
void nyxj_thread_join(nyxj_thread t) { (void)t; }

#elif defined(_WIN32)

void nyxj_mutex_init(nyxj_mutex* m) { InitializeSRWLock(m); }
void nyxj_mutex_lock(nyxj_mutex* m) { AcquireSRWLockExclusive(m); }
void nyxj_mutex_unlock(nyxj_mutex* m) { ReleaseSRWLockExclusive(m); }
void nyxj_mutex_destroy(nyxj_mutex* m) { (void)m; }

void nyxj_cond_init(nyxj_cond* c) { InitializeConditionVariable(c); }
void nyxj_cond_wait(nyxj_cond* c, nyxj_mutex* m) { SleepConditionVariableSRW(c, m, INFINITE, 0); }
bool nyxj_cond_timedwait(nyxj_cond* c, nyxj_mutex* m, unsigned ms) {
    return SleepConditionVariableSRW(c, m, ms, 0) != 0;
}
void nyxj_cond_signal(nyxj_cond* c) { WakeConditionVariable(c); }
void nyxj_cond_broadcast(nyxj_cond* c) { WakeAllConditionVariable(c); }
void nyxj_cond_destroy(nyxj_cond* c) { (void)c; }

typedef struct { nyxj_thread_fn fn; void* arg; } win_thread_start;

static DWORD WINAPI win_thread_entry(LPVOID p) {
    win_thread_start s = *(win_thread_start*)p;
    free(p);
    s.fn(s.arg);
    return 0;
}

bool nyxj_thread_create(nyxj_thread* t, nyxj_thread_fn fn, void* arg) {
    win_thread_start* s = malloc(sizeof(*s));
    if (!s) return false;
    s->fn = fn; s->arg = arg;
    *t = CreateThread(NULL, 0, win_thread_entry, s, 0, NULL);
    if (*t == NULL) { free(s); return false; }
    return true;
}

void nyxj_thread_join(nyxj_thread t) {
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
}

#else

void nyxj_mutex_init(nyxj_mutex* m) { pthread_mutex_init(m, NULL); }
void nyxj_mutex_lock(nyxj_mutex* m) { pthread_mutex_lock(m); }
void nyxj_mutex_unlock(nyxj_mutex* m) { pthread_mutex_unlock(m); }
void nyxj_mutex_destroy(nyxj_mutex* m) { pthread_mutex_destroy(m); }

void nyxj_cond_init(nyxj_cond* c) { pthread_cond_init(c, NULL); }
void nyxj_cond_wait(nyxj_cond* c, nyxj_mutex* m) { pthread_cond_wait(c, m); }
bool nyxj_cond_timedwait(nyxj_cond* c, nyxj_mutex* m, unsigned ms) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += ms / 1000;
    ts.tv_nsec += (long)(ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
    return pthread_cond_timedwait(c, m, &ts) == 0;
}
void nyxj_cond_signal(nyxj_cond* c) { pthread_cond_signal(c); }
void nyxj_cond_broadcast(nyxj_cond* c) { pthread_cond_broadcast(c); }
void nyxj_cond_destroy(nyxj_cond* c) { pthread_cond_destroy(c); }

bool nyxj_thread_create(nyxj_thread* t, nyxj_thread_fn fn, void* arg) {
    return pthread_create(t, NULL, fn, arg) == 0;
}
void nyxj_thread_join(nyxj_thread t) { pthread_join(t, NULL); }

#endif

/* --- Clock & system helpers (available in every build) --- */

int nyxj_cpu_count(void) {
#if defined(_WIN32)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwNumberOfProcessors > 0 ? (int)si.dwNumberOfProcessors : 1;
#elif defined(_SC_NPROCESSORS_ONLN)
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int)n : 1;
#else
    return 1;
#endif
}

uint64_t nyxj_now_ns(void) {
#if defined(_WIN32)
    static LARGE_INTEGER freq;
    LARGE_INTEGER now;
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    return (uint64_t)((double)now.QuadPart * 1e9 / (double)freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

uint64_t nyxj_now_ms(void) { return nyxj_now_ns() / 1000000ULL; }

uint64_t nyxj_wall_ms(void) {
#if defined(_WIN32)
    FILETIME ft;
    ULARGE_INTEGER u;
    GetSystemTimeAsFileTime(&ft);
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return (u.QuadPart - 116444736000000000ULL) / 10000ULL;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)tv.tv_usec / 1000ULL;
#endif
}

void nyxj_sleep_ms(unsigned ms) {
#if defined(_WIN32)
    Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}
