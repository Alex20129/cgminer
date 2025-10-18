/*
 * Copyright 2016 Mikeqin <Fengling.Qin@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.  See COPYING for more details.
 */
#ifndef LIBSSPLUS_H
#define LIBSSPLUS_H

#define HT_SIZE (1 << 22)
#define HT_PRB_LMT 1
#define HT_PRB_C1 0
#define HT_PRB_C2 1

typedef u_int32_t ssp_pair[2];

struct testcase
{
    unsigned char *coinbase;
    unsigned char (*merkle_branches)[32];
    unsigned int coinbase_len;
    unsigned int merkles;
    unsigned int n2size;
    unsigned int nonce2_offset;
};

int  ssp_hasher_init(void);
void ssp_hasher_update_stratum(struct pool *pool, bool clean);
void ssp_hasher_test(void);

void ssp_sorter_init(u_int32_t max_size, u_int32_t limit, u_int32_t c1, u_int32_t c2);
void ssp_sorter_flush(void);
int  ssp_sorter_get_pair(ssp_pair pair);

u_int32_t gen_merkle_root(struct pool *pool, uint64_t nonce2);

#endif /* LIBSSPLUS_H */
