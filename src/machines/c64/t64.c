#include "t64.h"
#include <string.h>

static uint16_t le16(const uint8_t *p) { return p[0] | ((uint16_t)p[1] << 8); }
static uint32_t le32(const uint8_t *p) { return le16(p) | ((uint32_t)le16(p+2) << 16); }

// RUN is offered only for a structurally valid tokenised BASIC program at $0801.
// A BASIC SYS stub is fine; arbitrary machine code is loaded without guessing an entry point.
static bool basic(const uint8_t *p, uint32_t n, unsigned start) {
    if (start != 0x0801) return false;
    uint32_t off = 0;
    while (off + 2 <= n) {
        unsigned next = le16(p + off);
        if (!next) return off != 0;
        if (next < start + off + 5 || next > start + n - 2) return false;
        unsigned end = next - start;
        if (p[end - 1] != 0) return false;
        // Each line's first terminator must agree with its link.
        for (uint32_t i = off + 4; i + 1 < end; ++i) if (!p[i]) return false;
        off = end;
    }
    return false;
}

bool t64_entry_get(const struct t64_archive *a, unsigned index, struct t64_entry *out) {
    if (!a || !a->data || index >= a->count || !out) return false;
    for (unsigned i = 0; i < a->slots; ++i) {
        const uint8_t *p = a->data + 64 + i * 32;
        if (p[0] != 1) continue;
        if (index--) continue;
        out->start = le16(p+2);
        unsigned end = le16(p+4);
        out->length = (end ? end : 65536u) - out->start;
        out->offset = le32(p+8);
        for (unsigned j = 0; j < 16; ++j) {
            unsigned c = p[16+j];
            if (c >= 0xc1 && c <= 0xda) c -= 0x80;
            out->name[j] = c >= 32 && c < 127 ? (char)c : ' ';
        }
        out->name[16] = 0;
        for (int j = 15; j >= 0 && out->name[j] == ' '; --j) out->name[j] = 0;
        if (!out->name[0]) strcpy(out->name, "Untitled");
        out->runnable = basic(a->data + out->offset, out->length, out->start);
        return true;
    }
    return false;
}

const char *t64_open(struct t64_archive *out, const uint8_t *data, uint32_t size) {
    if (!out) return "No archive destination";
    memset(out, 0, sizeof *out);
    if (!data || size < 96) return "Truncated T64 header or directory";
    if (memcmp(data, "C64 tape image", 14) && memcmp(data, "C64S tape image", 15) &&
        memcmp(data, "C64S tape file", 13)) return "Not a T64 archive";
    unsigned version = le16(data+32), slots = le16(data+34);
    if (version != 0x100 && version != 0x101) return "Unsupported T64 version";
    if (!slots || slots > (size - 64) / 32) return "Truncated T64 directory";
    uint32_t directory_end = 64 + slots * 32, count = 0;
    // Ignore the often-stale 'used entries' header; inspect every allocated slot.
    for (unsigned i = 0; i < slots; ++i) {
        const uint8_t *p = data + 64 + i * 32;
        if (!p[0]) continue;
        if (p[0] != 1 || (p[1] != 0 && (p[1] & 7) != 2)) return "T64 contains unsupported entry types";
        if (++count > T64_MAX_PROGRAMS) return "T64 has more than 64 programs";
        uint32_t start = le16(p+2), end = le16(p+4), offset = le32(p+8);
        if (!end) end = 65536;
        if (end <= start || offset < directory_end || offset > size || end-start > size-offset)
            return "Invalid T64 program bounds";
        for (unsigned j = 0; j < i; ++j) {
            const uint8_t *q = data + 64 + j * 32;
            if (!q[0]) continue;
            uint32_t qs = le32(q+8), qe = le16(q+4);
            if (!qe) qe = 65536;
            qe = qs + qe - le16(q+2);
            if (offset < qe && qs < offset + end-start) return "Overlapping T64 programs";
        }
    }
    if (!count) return "T64 contains no programs";
    *out = (struct t64_archive){data, size, (uint16_t)slots, (uint16_t)count};
    return 0;
}

bool t64_load(const struct t64_archive *a, unsigned index, uint8_t *ram) {
    struct t64_entry e;
    if (!ram || !t64_entry_get(a, index, &e)) return false;
    memcpy(ram + e.start, a->data + e.offset, e.length);
    if (e.start == 0x0801) {
        unsigned end = e.start + e.length;
        for (unsigned p = 0x2d; p <= 0x31; p += 2) { ram[p] = end; ram[p+1] = end >> 8; }
    }
    return true;
}
