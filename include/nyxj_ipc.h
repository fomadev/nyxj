/*
 * Inter-process communication between NyxJ services on the same machine,
 * without the TCP stack:
 *
 * - Unix domain sockets (Linux, macOS, Windows 10 1803+) carrying
 *   length-prefixed frames, with JSON helpers.
 * - Shared memory ring buffer (single producer / single consumer, lock-free)
 *   for the lowest possible latency.
 */
#ifndef NYXJ_IPC_H
#define NYXJ_IPC_H

#include "nyxj.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- Unix domain sockets --- */

typedef struct nyxj_ipc nyxj_ipc;

nyxj_ipc* nyxj_ipc_listen(const char* path, nyxj_error* err);
nyxj_ipc* nyxj_ipc_accept(nyxj_ipc* listener, int timeout_ms, nyxj_error* err); /* -1 = wait forever */
nyxj_ipc* nyxj_ipc_connect(const char* path, nyxj_error* err);
bool      nyxj_ipc_send(nyxj_ipc* ch, const void* data, size_t len, nyxj_error* err);
void*     nyxj_ipc_recv(nyxj_ipc* ch, size_t* len, int timeout_ms, nyxj_error* err); /* Caller frees */
bool        nyxj_ipc_send_json(nyxj_ipc* ch, const nyxj_value* v, nyxj_error* err);
nyxj_value* nyxj_ipc_recv_json(nyxj_ipc* ch, int timeout_ms, nyxj_error* err);
void      nyxj_ipc_close(nyxj_ipc* ch);

/* --- Shared memory ring buffer --- */

typedef struct nyxj_shm nyxj_shm;

nyxj_shm* nyxj_shm_create(const char* name, size_t capacity, nyxj_error* err); /* Producer side, owns the segment */
nyxj_shm* nyxj_shm_open(const char* name, nyxj_error* err);                    /* Consumer side */
bool      nyxj_shm_write(nyxj_shm* shm, const void* msg, size_t len);          /* false when full */
void*     nyxj_shm_read(nyxj_shm* shm, size_t* len);                           /* NULL when empty */
size_t    nyxj_shm_capacity(const nyxj_shm* shm);
void      nyxj_shm_close(nyxj_shm* shm); /* The creator also unlinks the segment */

#ifdef __cplusplus
}
#endif

#endif
