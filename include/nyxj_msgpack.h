/*
 * MessagePack binary serialization: 30-50% smaller than JSON text and much
 * faster to decode. Ideal for NyxJ-to-NyxJ communication.
 */
#ifndef NYXJ_MSGPACK_H
#define NYXJ_MSGPACK_H

#include <stdint.h>

#include "nyxj.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Encodes a value. Returns a heap buffer (caller frees) and its length. */
uint8_t* nyxj_msgpack_encode(const nyxj_value* v, size_t* out_len);

/* Decodes a MessagePack buffer. Binary payloads become strings. */
nyxj_value* nyxj_msgpack_decode(const uint8_t* data, size_t len, nyxj_error* err);

/* Converts JSON text straight to MessagePack and back. */
uint8_t* nyxj_json_to_msgpack(const char* json, size_t len, size_t* out_len, nyxj_error* err);
char*    nyxj_msgpack_to_json(const uint8_t* data, size_t len, nyxj_error* err);

#ifdef __cplusplus
}
#endif

#endif
