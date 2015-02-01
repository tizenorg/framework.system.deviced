/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*
 * FILE:        sha2.h
 * AUTHOR:      Aaron D. Gifford <me@aarongifford.com>
 *
 * A licence was granted to the ASF by Aaron on 4 November 2003.
 */

#ifndef __SHA2_H__
#define __SHA2_H__

#include <stdint.h>

/*** SHA-256 Various Length Definitions ***********************/
#define SECKM_SHA256_BLOCK_LENGTH             64
#define SECKM_SHA256_DIGEST_LENGTH            32
#define SECKM_SHA256_DIGEST_STRING_LENGTH     (SECKM_SHA256_DIGEST_LENGTH * 2 + 1)


/*** SHA-256 Context Structures *******************************/
typedef struct _SECKM_SHA256_CTX {
        uint32_t    state[8];
        uint64_t    bitcount;
        unsigned char      buffer[SECKM_SHA256_BLOCK_LENGTH];
} SECKM_SHA256_CTX;


/*** SHA-256 Function Prototypes ******************************/
void SECKM_SHA256_Init(SECKM_SHA256_CTX *);
void SECKM_SHA256_Update(SECKM_SHA256_CTX *, const unsigned char *, size_t);
void SECKM_SHA256_Final(SECKM_SHA256_CTX *, unsigned char [SECKM_SHA256_DIGEST_LENGTH]);

#endif /* __SHA2_H__ */


