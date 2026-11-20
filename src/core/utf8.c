/* Public strict UTF-8 validation, sharing the parser's SIMD scanner. */
#include "scan.h"

bool nyxj_utf8_validate(const char* s, size_t len, size_t* bad_offset) {
    if (!s) {
        if (bad_offset) *bad_offset = 0;
        return len == 0;
    }
    size_t bad = nyxj_utf8_invalid_at((const unsigned char*)s, len);
    if (bad_offset) *bad_offset = bad == len ? 0 : bad;
    return bad == len;
}
