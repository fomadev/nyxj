#if !defined(_WIN32) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#endif

#include "../../include/nyxj_ipc.h"
#include "../core/internal.h"

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
typedef SOCKET sock_t;
#define BAD_SOCK INVALID_SOCKET
#define close_sock closesocket
#ifndef AF_UNIX
#define AF_UNIX 1
#endif
/* <afunix.h> is missing from some MinGW toolchains: declare the layout. */
struct nyxj_sockaddr_un {
    ADDRESS_FAMILY sun_family;
    char sun_path[108];
};
static void ensure_wsa(void) {
    static volatile LONG done = 0;
    if (InterlockedCompareExchange(&done, 1, 0) == 0) {
        WSADATA d;
        WSAStartup(MAKEWORD(2, 2), &d);
    }
}
static void unlink_path(const char* p) { DeleteFileA(p); }
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
typedef int sock_t;
#define BAD_SOCK (-1)
#define close_sock close
#define nyxj_sockaddr_un sockaddr_un
static void ensure_wsa(void) {}
static void unlink_path(const char* p) { unlink(p); }
#endif

#define MAX_FRAME (64u * 1024u * 1024u)

/* --- Unix domain sockets --- */

struct nyxj_ipc {
    sock_t s;
    bool listener;
    char path[108];
};

static nyxj_ipc* ipc_new(sock_t s, const char* path, bool listener) {
    nyxj_ipc* ch = calloc(1, sizeof(*ch));
    if (!ch) { close_sock(s); return NULL; }
    ch->s = s;
    ch->listener = listener;
    if (path) snprintf(ch->path, sizeof(ch->path), "%s", path);
    return ch;
}

static bool make_addr(const char* path, struct nyxj_sockaddr_un* addr, nyxj_error* err) {
    memset(addr, 0, sizeof(*addr));
    addr->sun_family = AF_UNIX;
    if (!path || strlen(path) >= sizeof(addr->sun_path)) {
        nyxj_error_set(err, NYXJ_ERR_INVALID_ARGUMENT, "socket path is empty or too long");
        return false;
    }
    memcpy(addr->sun_path, path, strlen(path) + 1);
    return true;
}

static void sock_error(nyxj_error* err, const char* what) {
#if defined(_WIN32)
    int code = WSAGetLastError();
    nyxj_error_set(err, code == WSAEAFNOSUPPORT ? NYXJ_ERR_UNSUPPORTED : NYXJ_ERR_IO, "%s failed (WSA error %d)", what, code);
#else
    nyxj_error_set(err, NYXJ_ERR_IO, "%s failed: %s", what, strerror(errno));
#endif
}

/* 1 = readable, 0 = timeout, -1 = error */
static int wait_readable(sock_t s, int timeout_ms) {
    fd_set rf;
    FD_ZERO(&rf);
    FD_SET(s, &rf);
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    int r = select((int)s + 1, &rf, NULL, NULL, timeout_ms < 0 ? NULL : &tv);
    return r > 0 ? 1 : r;
}

nyxj_ipc* nyxj_ipc_listen(const char* path, nyxj_error* err) {
    nyxj_error_clear(err);
    ensure_wsa();
    struct nyxj_sockaddr_un addr;
    if (!make_addr(path, &addr, err)) return NULL;
    sock_t s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s == BAD_SOCK) { sock_error(err, "socket"); return NULL; }
    unlink_path(path);
    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) != 0) { sock_error(err, "bind"); close_sock(s); return NULL; }
    if (listen(s, 16) != 0) { sock_error(err, "listen"); close_sock(s); unlink_path(path); return NULL; }
    return ipc_new(s, path, true);
}

nyxj_ipc* nyxj_ipc_accept(nyxj_ipc* listener, int timeout_ms, nyxj_error* err) {
    nyxj_error_clear(err);
    if (!listener || !listener->listener) { nyxj_error_set(err, NYXJ_ERR_INVALID_ARGUMENT, NULL); return NULL; }
    int r = wait_readable(listener->s, timeout_ms);
    if (r == 0) { nyxj_error_set(err, NYXJ_ERR_TIMEOUT, NULL); return NULL; }
    if (r < 0) { sock_error(err, "select"); return NULL; }
    sock_t c = accept(listener->s, NULL, NULL);
    if (c == BAD_SOCK) { sock_error(err, "accept"); return NULL; }
    return ipc_new(c, NULL, false);
}

nyxj_ipc* nyxj_ipc_connect(const char* path, nyxj_error* err) {
    nyxj_error_clear(err);
    ensure_wsa();
    struct nyxj_sockaddr_un addr;
    if (!make_addr(path, &addr, err)) return NULL;
    sock_t s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s == BAD_SOCK) { sock_error(err, "socket"); return NULL; }
    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) != 0) { sock_error(err, "connect"); close_sock(s); return NULL; }
    return ipc_new(s, NULL, false);
}

static bool send_all(sock_t s, const char* p, size_t n) {
    while (n > 0) {
        int chunk = n > 0x40000000u ? 0x40000000 : (int)n;
        int w = (int)send(s, p, chunk, 0);
        if (w <= 0) return false;
        p += w;
        n -= (size_t)w;
    }
    return true;
}

static int recv_all(sock_t s, char* p, size_t n, int timeout_ms) {
    while (n > 0) {
        int r = wait_readable(s, timeout_ms);
        if (r <= 0) return r == 0 ? 0 : -1;
        int chunk = n > 0x40000000u ? 0x40000000 : (int)n;
        int got = (int)recv(s, p, chunk, 0);
        if (got <= 0) return -1;
        p += got;
        n -= (size_t)got;
    }
    return 1;
}

bool nyxj_ipc_send(nyxj_ipc* ch, const void* data, size_t len, nyxj_error* err) {
    nyxj_error_clear(err);
    if (!ch || ch->listener || (!data && len)) { nyxj_error_set(err, NYXJ_ERR_INVALID_ARGUMENT, NULL); return false; }
    if (len > MAX_FRAME) { nyxj_error_set(err, NYXJ_ERR_SIZE_LIMIT, "frame larger than 64 MiB"); return false; }
    unsigned char hdr[4] = {(unsigned char)len, (unsigned char)(len >> 8), (unsigned char)(len >> 16), (unsigned char)(len >> 24)};
    if (!send_all(ch->s, (const char*)hdr, 4) || !send_all(ch->s, data, len)) { sock_error(err, "send"); return false; }
    return true;
}

void* nyxj_ipc_recv(nyxj_ipc* ch, size_t* len, int timeout_ms, nyxj_error* err) {
    nyxj_error_clear(err);
    if (len) *len = 0;
    if (!ch || ch->listener) { nyxj_error_set(err, NYXJ_ERR_INVALID_ARGUMENT, NULL); return NULL; }
    unsigned char hdr[4];
    int r = recv_all(ch->s, (char*)hdr, 4, timeout_ms);
    if (r == 0) { nyxj_error_set(err, NYXJ_ERR_TIMEOUT, NULL); return NULL; }
    if (r < 0) { nyxj_error_set(err, NYXJ_ERR_IO, "connection closed or failed"); return NULL; }
    size_t n = (size_t)hdr[0] | ((size_t)hdr[1] << 8) | ((size_t)hdr[2] << 16) | ((size_t)hdr[3] << 24);
    if (n > MAX_FRAME) { nyxj_error_set(err, NYXJ_ERR_SIZE_LIMIT, "frame larger than 64 MiB"); return NULL; }
    char* buf = malloc(n + 1);
    if (!buf) { nyxj_error_set(err, NYXJ_ERR_OUT_OF_MEMORY, NULL); return NULL; }
    if (n && recv_all(ch->s, buf, n, timeout_ms < 0 ? -1 : timeout_ms + 1000) != 1) {
        free(buf);
        nyxj_error_set(err, NYXJ_ERR_IO, "truncated frame");
        return NULL;
    }
    buf[n] = '\0';
    if (len) *len = n;
    return buf;
}

bool nyxj_ipc_send_json(nyxj_ipc* ch, const nyxj_value* v, nyxj_error* err) {
    size_t n;
    char* text = nyxj_serialize_ex(v, NULL, &n);
    if (!text) { nyxj_error_set(err, NYXJ_ERR_OUT_OF_MEMORY, NULL); return false; }
    bool ok = nyxj_ipc_send(ch, text, n, err);
    free(text);
    return ok;
}

nyxj_value* nyxj_ipc_recv_json(nyxj_ipc* ch, int timeout_ms, nyxj_error* err) {
    size_t n;
    char* text = nyxj_ipc_recv(ch, &n, timeout_ms, err);
    if (!text) return NULL;
    nyxj_result r = nyxj_parse_n(text, n);
    free(text);
    if (!r.is_valid && err) *err = r.error;
    return r.root;
}

void nyxj_ipc_close(nyxj_ipc* ch) {
    if (!ch) return;
    close_sock(ch->s);
    if (ch->listener && ch->path[0]) unlink_path(ch->path);
    free(ch);
}

/* --- Shared memory SPSC ring --- */

#define SHM_MAGIC 0x4A58594Eu /* "NYXJ" */

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t capacity;
    _Atomic uint64_t head; /* Consumer position */
    _Atomic uint64_t tail; /* Producer position */
} shm_header;

struct nyxj_shm {
    shm_header* hdr;
    unsigned char* data;
    size_t map_size;
    bool owner;
    char name[64];
#if defined(_WIN32)
    HANDLE mapping;
#endif
};

static void shm_name(const char* name, char* out, size_t n) {
#if defined(_WIN32)
    snprintf(out, n, "Local\\nyxj-%s", name);
#else
    snprintf(out, n, "/nyxj-%.24s", name); /* macOS limits names to 31 bytes */
#endif
}

nyxj_shm* nyxj_shm_create(const char* name, size_t capacity, nyxj_error* err) {
    nyxj_error_clear(err);
    if (!name || !*name || capacity < 64) { nyxj_error_set(err, NYXJ_ERR_INVALID_ARGUMENT, "invalid name or capacity"); return NULL; }
    nyxj_shm* s = calloc(1, sizeof(*s));
    if (!s) { nyxj_error_set(err, NYXJ_ERR_OUT_OF_MEMORY, NULL); return NULL; }
    shm_name(name, s->name, sizeof(s->name));
    s->map_size = sizeof(shm_header) + capacity;
    s->owner = true;
#if defined(_WIN32)
    s->mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, (DWORD)((uint64_t)s->map_size >> 32),
                                    (DWORD)(s->map_size & 0xFFFFFFFFu), s->name);
    if (!s->mapping) { nyxj_error_set(err, NYXJ_ERR_IO, "CreateFileMapping failed (%lu)", GetLastError()); free(s); return NULL; }
    void* p = MapViewOfFile(s->mapping, FILE_MAP_ALL_ACCESS, 0, 0, s->map_size);
    if (!p) { CloseHandle(s->mapping); free(s); nyxj_error_set(err, NYXJ_ERR_IO, "MapViewOfFile failed"); return NULL; }
#else
    shm_unlink(s->name);
    int fd = shm_open(s->name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0) { nyxj_error_set(err, NYXJ_ERR_IO, "shm_open failed: %s", strerror(errno)); free(s); return NULL; }
    if (ftruncate(fd, (off_t)s->map_size) != 0) {
        nyxj_error_set(err, NYXJ_ERR_IO, "ftruncate failed: %s", strerror(errno));
        close(fd);
        shm_unlink(s->name);
        free(s);
        return NULL;
    }
    void* p = mmap(NULL, s->map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (p == MAP_FAILED) { nyxj_error_set(err, NYXJ_ERR_IO, "mmap failed: %s", strerror(errno)); shm_unlink(s->name); free(s); return NULL; }
#endif
    s->hdr = p;
    s->data = (unsigned char*)p + sizeof(shm_header);
    s->hdr->magic = SHM_MAGIC;
    s->hdr->version = 1;
    s->hdr->capacity = capacity;
    atomic_store(&s->hdr->head, 0);
    atomic_store(&s->hdr->tail, 0);
    return s;
}

nyxj_shm* nyxj_shm_open(const char* name, nyxj_error* err) {
    nyxj_error_clear(err);
    if (!name || !*name) { nyxj_error_set(err, NYXJ_ERR_INVALID_ARGUMENT, NULL); return NULL; }
    nyxj_shm* s = calloc(1, sizeof(*s));
    if (!s) { nyxj_error_set(err, NYXJ_ERR_OUT_OF_MEMORY, NULL); return NULL; }
    shm_name(name, s->name, sizeof(s->name));
#if defined(_WIN32)
    s->mapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, s->name);
    if (!s->mapping) { nyxj_error_set(err, NYXJ_ERR_NOT_FOUND, "shared memory '%s' not found", name); free(s); return NULL; }
    void* p = MapViewOfFile(s->mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!p) { CloseHandle(s->mapping); free(s); nyxj_error_set(err, NYXJ_ERR_IO, "MapViewOfFile failed"); return NULL; }
    s->hdr = p;
    s->map_size = sizeof(shm_header) + (size_t)s->hdr->capacity;
#else
    int fd = shm_open(s->name, O_RDWR, 0600);
    if (fd < 0) { nyxj_error_set(err, NYXJ_ERR_NOT_FOUND, "shared memory '%s' not found", name); free(s); return NULL; }
    struct stat st;
    if (fstat(fd, &st) != 0 || (size_t)st.st_size < sizeof(shm_header)) {
        close(fd);
        free(s);
        nyxj_error_set(err, NYXJ_ERR_IO, "invalid shared memory segment");
        return NULL;
    }
    s->map_size = (size_t)st.st_size;
    void* p = mmap(NULL, s->map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (p == MAP_FAILED) { free(s); nyxj_error_set(err, NYXJ_ERR_IO, "mmap failed"); return NULL; }
    s->hdr = p;
#endif
    if (s->hdr->magic != SHM_MAGIC) {
        nyxj_error_set(err, NYXJ_ERR_INTEGRITY, "segment is not a NyxJ ring buffer");
        s->owner = false;
        nyxj_shm_close(s);
        return NULL;
    }
    s->data = (unsigned char*)s->hdr + sizeof(shm_header);
    return s;
}

size_t nyxj_shm_capacity(const nyxj_shm* s) { return s ? (size_t)s->hdr->capacity : 0; }

static void ring_copy_in(nyxj_shm* s, uint64_t pos, const void* src, size_t n) {
    size_t cap = (size_t)s->hdr->capacity;
    size_t off = (size_t)(pos % cap);
    size_t first = n < cap - off ? n : cap - off;
    memcpy(s->data + off, src, first);
    memcpy(s->data, (const unsigned char*)src + first, n - first);
}

static void ring_copy_out(nyxj_shm* s, uint64_t pos, void* dst, size_t n) {
    size_t cap = (size_t)s->hdr->capacity;
    size_t off = (size_t)(pos % cap);
    size_t first = n < cap - off ? n : cap - off;
    memcpy(dst, s->data + off, first);
    memcpy((unsigned char*)dst + first, s->data, n - first);
}

bool nyxj_shm_write(nyxj_shm* s, const void* msg, size_t len) {
    if (!s || (!msg && len) || len > 0xFFFFFFFFu) return false;
    uint64_t head = atomic_load_explicit(&s->hdr->head, memory_order_acquire);
    uint64_t tail = atomic_load_explicit(&s->hdr->tail, memory_order_relaxed);
    uint64_t need = 4 + (uint64_t)len;
    if (s->hdr->capacity - (tail - head) < need) return false;
    uint32_t n32 = (uint32_t)len;
    ring_copy_in(s, tail, &n32, 4);
    ring_copy_in(s, tail + 4, msg, len);
    atomic_store_explicit(&s->hdr->tail, tail + need, memory_order_release);
    return true;
}

void* nyxj_shm_read(nyxj_shm* s, size_t* len) {
    if (len) *len = 0;
    if (!s) return NULL;
    uint64_t tail = atomic_load_explicit(&s->hdr->tail, memory_order_acquire);
    uint64_t head = atomic_load_explicit(&s->hdr->head, memory_order_relaxed);
    if (head == tail) return NULL;
    uint32_t n32;
    ring_copy_out(s, head, &n32, 4);
    if ((uint64_t)n32 + 4 > tail - head) return NULL; /* Corrupted header */
    char* out = malloc((size_t)n32 + 1);
    if (!out) return NULL;
    ring_copy_out(s, head + 4, out, n32);
    out[n32] = '\0';
    atomic_store_explicit(&s->hdr->head, head + 4 + n32, memory_order_release);
    if (len) *len = n32;
    return out;
}

void nyxj_shm_close(nyxj_shm* s) {
    if (!s) return;
#if defined(_WIN32)
    if (s->hdr) UnmapViewOfFile(s->hdr);
    if (s->mapping) CloseHandle(s->mapping);
#else
    if (s->hdr) munmap(s->hdr, s->map_size);
    if (s->owner) shm_unlink(s->name);
#endif
    free(s);
}
