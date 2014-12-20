/*
Copyright (c) 2014 Mek Entertainment, Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#ifndef __MREDIS_H
#define __MREDIS_H

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

#if !defined(bool) && !defined(__cplusplus)
typedef int bool;
#endif

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

typedef struct mredis_s *mredis;

typedef enum {
    MR_STR,
    MR_ERR,
    MR_BIN,
    MR_ARR
} mredis_return_t;

#ifdef __cplusplus
extern "C" {
#endif

mredis mredis_init(const char *ip, int port, const char *auth);
void mredis_close(mredis rd);

bool mredis_sendMessage(mredis rd, const char *msg, size_t sz);
mredis_return_t mredis_recieveReply(mredis rd, char **ret);
size_t mredis_recieveArray(mredis rd, char ***data);

bool mredis_flushdb(mredis rd);
bool mredis_select(mredis rd, uint8_t i);

void mredis_sendSORT(mredis rd, const char *key, size_t count, const char **cmd);

bool mredis_SET(mredis rd, const char *key, const char *value);
void mredis_sendGET(mredis rd, const char *key);

bool mredis_HSET(mredis rd, const char *key, const char *field, const char *value);
void mredis_sendHMGET(mredis rd, const char *key, size_t count, const char **fields);

bool mredis_ZADD(mredis rd, const char *key, uint32_t score, const char *member);
size_t mredis_ZCARD(mredis rd, const char *key);
void mredis_sendZRANGE(mredis rd, const char *key, int start, int stop, bool withScores);

#ifdef __cplusplus
}
#endif

#endif /* __MREDIS_H */
