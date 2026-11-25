/*
 * tinf - tiny inflate library (zlib / deflate decompressor)
 * Public domain by Joergen Ibsen
 */
#include "mini_inflate.h"

typedef struct {
    const uint8_t *source;
    uint32_t tag;
    int bitcount;
} TINF_DATA;

typedef struct {
    uint16_t table[16];
    uint16_t trans[288];
} TINF_TREE;

static uint32_t tinf_getbit(TINF_DATA *d)
{
    uint32_t bit = d->tag & 1;
    d->tag >>= 1;
    d->bitcount--;
    if (d->bitcount == 0)
    {
        d->tag = *d->source++;
        d->bitcount = 8;
    }
    return bit;
}

static uint32_t tinf_read_bits(TINF_DATA *d, int num, int base)
{
    uint32_t val = 0;
    if (num)
    {
        unsigned limit = 1 << num;
        for (unsigned mask = 1; mask < limit; mask <<= 1)
        {
            if (tinf_getbit(d)) val += mask;
        }
    }
    return val + (uint32_t) base;
}

static int tinf_decode_symbol(TINF_DATA *d, const TINF_TREE *t)
{
    int sum = 0, cur = 0, len = 0;
    do {
        cur = (cur << 1) + (int) tinf_getbit(d);
        len++;
        sum += (int) t->table[len];
        cur -= (int) t->table[len];
    } while (cur >= 0);
    return t->trans[sum + cur];
}

static void tinf_build_tree(TINF_TREE *t, const uint8_t *lengths, unsigned num)
{
    uint16_t offs[16];
    for (int i = 0; i < 16; ++i) t->table[i] = 0;
    for (unsigned i = 0; i < num; ++i) t->table[lengths[i]]++;
    t->table[0] = 0;

    offs[1] = 0;
    for (int i = 1; i < 15; ++i) offs[i + 1] = (uint16_t) (offs[i] + t->table[i]);
    for (unsigned i = 0; i < num; ++i)
    {
        if (lengths[i]) t->trans[offs[lengths[i]]++] = (uint16_t) i;
    }
}

static void tinf_build_fixed_trees(TINF_TREE *lt, TINF_TREE *dt)
{
    uint8_t lengths[288];
    for (int i = 0; i <= 143; ++i) lengths[i] = 8;
    for (int i = 144; i <= 255; ++i) lengths[i] = 9;
    for (int i = 256; i <= 279; ++i) lengths[i] = 7;
    for (int i = 280; i <= 287; ++i) lengths[i] = 8;
    tinf_build_tree(lt, lengths, 288);
    for (int i = 0; i < 32; ++i) lengths[i] = 5;
    tinf_build_tree(dt, lengths, 32);
}

static const uint8_t clcidx[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

static const uint16_t length_base[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
    67, 83, 99, 115, 131, 163, 195, 227, 258
};
static const uint8_t length_extra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
    4, 4, 4, 4, 5, 5, 5, 5, 0
};

static const uint16_t dist_base[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513,
    769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};
static const uint8_t dist_extra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8,
    9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

static int tinf_inflate_block_data(TINF_DATA *d, const TINF_TREE *lt, const TINF_TREE *dt,
                                   uint8_t *dst, size_t dst_len, size_t *dest_idx)
{
    while (1)
    {
        int sym = tinf_decode_symbol(d, lt);
        if (sym == 256) return 0; /* End of block */

        if (sym < 256)
        {
            if (*dest_idx >= dst_len) return -1;
            dst[(*dest_idx)++] = (uint8_t) sym;
        }
        else
        {
            sym -= 257;
            uint32_t length = tinf_read_bits(d, length_extra[sym], length_base[sym]);
            int dist_sym = tinf_decode_symbol(d, dt);
            uint32_t dist = tinf_read_bits(d, dist_extra[dist_sym], dist_base[dist_sym]);

            if (*dest_idx + length > dst_len || dist > *dest_idx) return -1;

            for (uint32_t i = 0; i < length; ++i)
            {
                dst[*dest_idx] = dst[*dest_idx - dist];
                (*dest_idx)++;
            }
        }
    }
}

int mini_zlib_decompress(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_len, size_t *out_len)
{
    if (src_len < 6 || dst == NULL) return -1;

    /* Check zlib header */
    uint8_t cmf = src[0];
    uint8_t flg = src[1];
    if ((cmf & 0x0F) != 8 || ((cmf * 256 + flg) % 31 != 0)) return -1; /* Not deflate */

    TINF_DATA d;
    d.source = src + 2; /* Skip 2-byte zlib header */
    d.tag = *d.source++;
    d.bitcount = 8;

    size_t dest_idx = 0;
    int bfinal;

    do {
        bfinal = (int) tinf_getbit(&d);
        int btype = (int) tinf_read_bits(&d, 2, 0);

        if (btype == 0) /* Uncompressed */
        {
            /* Byte align */
            d.bitcount = 0;
            uint32_t len = (uint32_t) (d.source[0] | (d.source[1] << 8));
            d.source += 4;
            if (dest_idx + len > dst_len) return -1;
            for (uint32_t i = 0; i < len; ++i) dst[dest_idx++] = *d.source++;
            d.tag = *d.source++;
            d.bitcount = 8;
        }
        else if (btype == 1) /* Fixed Huffman */
        {
            TINF_TREE lt, dt;
            tinf_build_fixed_trees(&lt, &dt);
            if (tinf_inflate_block_data(&d, &lt, &dt, dst, dst_len, &dest_idx) < 0) return -1;
        }
        else if (btype == 2) /* Dynamic Huffman */
        {
            TINF_TREE lt, dt;
            uint8_t lengths[288 + 32];
            uint32_t hlit = tinf_read_bits(&d, 5, 257);
            uint32_t hdist = tinf_read_bits(&d, 5, 1);
            uint32_t hclen = tinf_read_bits(&d, 4, 4);

            uint8_t cllengths[19] = {0};
            for (uint32_t i = 0; i < hclen; ++i) cllengths[clcidx[i]] = (uint8_t) tinf_read_bits(&d, 3, 0);
            TINF_TREE ct;
            tinf_build_tree(&ct, cllengths, 19);

            uint32_t num = hlit + hdist;
            for (uint32_t i = 0; i < num; )
            {
                int sym = tinf_decode_symbol(&d, &ct);
                if (sym < 16)
                {
                    lengths[i++] = (uint8_t) sym;
                }
                else if (sym == 16)
                {
                    uint32_t run = tinf_read_bits(&d, 2, 3);
                    uint8_t prev = lengths[i - 1];
                    while (run--) lengths[i++] = prev;
                }
                else if (sym == 17)
                {
                    uint32_t run = tinf_read_bits(&d, 3, 3);
                    while (run--) lengths[i++] = 0;
                }
                else
                {
                    uint32_t run = tinf_read_bits(&d, 7, 11);
                    while (run--) lengths[i++] = 0;
                }
            }
            tinf_build_tree(&lt, lengths, hlit);
            tinf_build_tree(&dt, lengths + hlit, hdist);
            if (tinf_inflate_block_data(&d, &lt, &dt, dst, dst_len, &dest_idx) < 0) return -1;
        }
        else
        {
            return -1;
        }
    } while (!bfinal);

    if (out_len) *out_len = dest_idx;
    return 0;
}
