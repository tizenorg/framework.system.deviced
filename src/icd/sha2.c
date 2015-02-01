/* Licensed to the Apache Software Foundation (ASF) under one or more
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
 * FILE:        sha2.c
 * AUTHOR:      Aaron D. Gifford <me@aarongifford.com>
 *
 * A licence was granted to the ASF by Aaron on 4 November 2003.
 */
#include <string.h>
#include <unistd.h>
#include "sha2.h"

//#define UINT64_C(x) x##ULL

/*** SHA-256/384/512 Various Length Definitions ***********************/
#define SECKM_SHA256_SHORT_BLOCK_LENGTH       (SECKM_SHA256_BLOCK_LENGTH - 8)

/*** ENDIAN REVERSAL MACROS *******************************************/
#define REVERSE32(w,x)  { \
        sha2_word32 tmp = (w); \
        tmp = (tmp >> 16) | (tmp << 16); \
        (x) = ((tmp & 0xff00ff00UL) >> 8) | ((tmp & 0x00ff00ffUL) << 8); \
}
#define REVERSE64(w,x)  { \
        sha2_word64 tmp = (w); \
        tmp = (tmp >> 32) | (tmp << 32); \
        tmp = ((tmp & UINT64_C(0xff00ff00ff00ff00)) >> 8) | \
              ((tmp & UINT64_C(0x00ff00ff00ff00ff)) << 8); \
        (x) = ((tmp & UINT64_C(0xffff0000ffff0000)) >> 16) | \
              ((tmp & UINT64_C(0x0000ffff0000ffff)) << 16); \
}

/*** SHA-256/384/512 Machine Architecture Definitions *****************/
typedef unsigned char   sha2_byte;         /* Exactly 1 byte */
typedef uint32_t sha2_word32; /* Exactly 4 bytes */
typedef uint64_t sha2_word64; /* Exactly 8 bytes */

/*
 * Macro for incrementally adding the unsigned 64-bit integer n to the
 * unsigned 128-bit integer (represented using a two-element array of
 * 64-bit words):
 */
#define ADDINC128(w,n)  { \
        (w)[0] += (sha2_word64)(n); \
        if ((w)[0] < (n)) { \
                (w)[1]++; \
        } \
}

#define MEMSET_BZERO(p,l)       memset((p), 0, (l))
#define MEMCPY_BCOPY(d,s,l)     memcpy((d), (s), (l))

/*** THE SIX LOGICAL FUNCTIONS ****************************************/
/*
 * Bit shifting and rotation (used by the six SHA-XYZ logical functions:
 *
 *   NOTE:  The naming of R and S appears backwards here (R is a SHIFT and
 *   S is a ROTATION) because the SHA-256/384/512 description document
 *   (see http://csrc.nist.gov/cryptval/shs/sha256-384-512.pdf) uses this
 *   same "backwards" definition.
 */
/* Shift-right (used in SHA-256, SHA-384, and SHA-512): */
#define R(b,x)          ((x) >> (b))
/* 32-bit Rotate-right (used in SHA-256): */
#define S32(b,x)        (((x) >> (b)) | ((x) << (32 - (b))))
/* 64-bit Rotate-right (used in SHA-384 and SHA-512): */
#define S64(b,x)        (((x) >> (b)) | ((x) << (64 - (b))))

/* Two of six logical functions used in SHA-256, SHA-384, and SHA-512: */
#define Ch(x,y,z)       (((x) & (y)) ^ ((~(x)) & (z)))
#define Maj(x,y,z)      (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

/* Four of six logical functions used in SHA-256: */
#define Sigma0_256(x)   (S32(2,  (x)) ^ S32(13, (x)) ^ S32(22, (x)))
#define Sigma1_256(x)   (S32(6,  (x)) ^ S32(11, (x)) ^ S32(25, (x)))
#define sigma0_256(x)   (S32(7,  (x)) ^ S32(18, (x)) ^ R(3 ,   (x)))
#define sigma1_256(x)   (S32(17, (x)) ^ S32(19, (x)) ^ R(10,   (x)))

/* Four of six logical functions used in SHA-384 and SHA-512: */
#define Sigma0_512(x)   (S64(28, (x)) ^ S64(34, (x)) ^ S64(39, (x)))
#define Sigma1_512(x)   (S64(14, (x)) ^ S64(18, (x)) ^ S64(41, (x)))
#define sigma0_512(x)   (S64( 1, (x)) ^ S64( 8, (x)) ^ R( 7,   (x)))
#define sigma1_512(x)   (S64(19, (x)) ^ S64(61, (x)) ^ R( 6,   (x)))

/*** INTERNAL FUNCTION PROTOTYPES *************************************/
/* NOTE: These should not be accessed directly from outside this
 * library -- they are intended for private internal visibility/use
 * only.
 */
void SECKM_SHA256_Transform(SECKM_SHA256_CTX*, const sha2_word32*);

/*** SHA-XYZ INITIAL HASH VALUES AND CONSTANTS ************************/
/* Hash constant words K for SHA-256: */
static const sha2_word32 K256[64] =
{ 0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL, 0x3956c25bUL,
        0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL, 0xd807aa98UL, 0x12835b01UL,
        0x243185beUL, 0x550c7dc3UL, 0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL,
        0xc19bf174UL, 0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
        0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL, 0x983e5152UL,
        0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL, 0xc6e00bf3UL, 0xd5a79147UL,
        0x06ca6351UL, 0x14292967UL, 0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL,
        0x53380d13UL, 0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
        0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL, 0xd192e819UL,
        0xd6990624UL, 0xf40e3585UL, 0x106aa070UL, 0x19a4c116UL, 0x1e376c08UL,
        0x2748774cUL, 0x34b0bcb5UL, 0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL,
        0x682e6ff3UL, 0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
        0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL };

/* Initial hash value H for SHA-256: */
static const sha2_word32 SECKM_SHA256_initial_hash_value[8] =
{ 0x6a09e667UL, 0xbb67ae85UL, 0x3c6ef372UL, 0xa54ff53aUL, 0x510e527fUL,
        0x9b05688cUL, 0x1f83d9abUL, 0x5be0cd19UL };

/*** SHA-256: *********************************************************/
void SECKM_SHA256_Init(SECKM_SHA256_CTX* context)
{
    if (context == (SECKM_SHA256_CTX*) 0)
    {
        return;
    }
    MEMCPY_BCOPY(context->state, SECKM_SHA256_initial_hash_value, SECKM_SHA256_DIGEST_LENGTH);
    MEMSET_BZERO(context->buffer, SECKM_SHA256_BLOCK_LENGTH);
    context->bitcount = 0;
}

void SECKM_SHA256_Transform(SECKM_SHA256_CTX* context, const sha2_word32* data)
{
    sha2_word32 a, b, c, d, e, f, g, h, s0, s1;
    sha2_word32 T1, T2, *W256;
    int j;

    W256 = (sha2_word32*) context->buffer;

    /* Initialize registers with the prev. intermediate value */
    a = context->state[0];
    b = context->state[1];
    c = context->state[2];
    d = context->state[3];
    e = context->state[4];
    f = context->state[5];
    g = context->state[6];
    h = context->state[7];

    j = 0;
    do
    {
        /* Copy data while converting to host byte order */
        REVERSE32(*data++,W256[j]);
        /* Apply the SHA-256 compression function to update a..h */
        T1 = h + Sigma1_256(e) + Ch(e, f, g) + K256[j] + W256[j];
        T2 = Sigma0_256(a) + Maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + T1;
        d = c;
        c = b;
        b = a;
        a = T1 + T2;

        j++;
    } while (j < 16);

    do
    {
        /* Part of the message block expansion: */
        s0 = W256[(j + 1) & 0x0f];
        s0 = sigma0_256(s0);
        s1 = W256[(j + 14) & 0x0f];
        s1 = sigma1_256(s1);

        /* Apply the SHA-256 compression function to update a..h */
        T1 = h + Sigma1_256(e) + Ch(e, f, g) + K256[j] + (W256[j & 0x0f] += s1
                + W256[(j + 9) & 0x0f] + s0);
        T2 = Sigma0_256(a) + Maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + T1;
        d = c;
        c = b;
        b = a;
        a = T1 + T2;

        j++;
    } while (j < 64);

    /* Compute the current intermediate hash value */
    context->state[0] += a;
    context->state[1] += b;
    context->state[2] += c;
    context->state[3] += d;
    context->state[4] += e;
    context->state[5] += f;
    context->state[6] += g;
    context->state[7] += h;

    /* Clean up */
    a = b = c = d = e = f = g = h = T1 = T2 = 0;
}

void SECKM_SHA256_Update(SECKM_SHA256_CTX* context, const sha2_byte *data, size_t len)
{
    unsigned int freespace, usedspace;

    if (len == 0)
    {
        /* Calling with no data is valid - we do nothing */
        return;
    }

    /* Sanity check: */
    if (context == (SECKM_SHA256_CTX*) 0 || data == (sha2_byte*) 0)
        return;

    usedspace = (unsigned int) ((context->bitcount >> 3) % SECKM_SHA256_BLOCK_LENGTH);
    if (usedspace > 0)
    {
        /* Calculate how much free space is available in the buffer */
        freespace = SECKM_SHA256_BLOCK_LENGTH - usedspace;

        if (len >= freespace)
        {
            /* Fill the buffer completely and process it */
            MEMCPY_BCOPY(&context->buffer[usedspace], data, freespace);
            context->bitcount += freespace << 3;
            len -= freespace;
            data += freespace;
            SECKM_SHA256_Transform(context, (sha2_word32*) context->buffer);
        }
        else
        {
            /* The buffer is not yet full */
            MEMCPY_BCOPY(&context->buffer[usedspace], data, len);
            context->bitcount += len << 3;
            /* Clean up: */
            usedspace = freespace = 0;
            return;
        }
    }
    while (len >= SECKM_SHA256_BLOCK_LENGTH)
    {
        /* Process as many complete blocks as we can */
        SECKM_SHA256_Transform(context, (sha2_word32*) data);
        context->bitcount += SECKM_SHA256_BLOCK_LENGTH << 3;
        len -= SECKM_SHA256_BLOCK_LENGTH;
        data += SECKM_SHA256_BLOCK_LENGTH;
    }
    if (len > 0)
    {
        /* There's left-overs, so save 'em */
        MEMCPY_BCOPY(context->buffer, data, len);
        context->bitcount += len << 3;
    }
    /* Clean up: */
    usedspace = freespace = 0;
}

void SECKM_SHA256_Final(SECKM_SHA256_CTX* context, sha2_byte digest[])
{
    sha2_word32 *d = (sha2_word32*) digest;
    unsigned int usedspace;

    /* Sanity check: */
    if (context == (SECKM_SHA256_CTX*) 0)
        return;

    /* If no digest buffer is passed, we don't bother doing this: */
    if (digest != (sha2_byte*) 0)
    {
        usedspace = (unsigned int) ((context->bitcount >> 3)
                % SECKM_SHA256_BLOCK_LENGTH);
        /* Convert FROM host byte order */
        REVERSE64(context->bitcount,context->bitcount);

        if (usedspace > 0)
        {
            /* Begin padding with a 1 bit: */
            context->buffer[usedspace++] = 0x80;

            if (usedspace <= SECKM_SHA256_SHORT_BLOCK_LENGTH)
            {
                /* Set-up for the last transform: */
                MEMSET_BZERO(&context->buffer[usedspace], SECKM_SHA256_SHORT_BLOCK_LENGTH - usedspace);
            }
            else
            {
                if (usedspace < SECKM_SHA256_BLOCK_LENGTH)
                {
                    MEMSET_BZERO(&context->buffer[usedspace], SECKM_SHA256_BLOCK_LENGTH - usedspace);
                }
                /* Do second-to-last transform: */
                SECKM_SHA256_Transform(context, (sha2_word32*) context->buffer);

                /* And set-up for the last transform: */
                MEMSET_BZERO(context->buffer, SECKM_SHA256_SHORT_BLOCK_LENGTH);
            }
        }
        else
        {
            /* Set-up for the last transform: */
            MEMSET_BZERO(context->buffer, SECKM_SHA256_SHORT_BLOCK_LENGTH);

            /* Begin padding with a 1 bit: */
            *context->buffer = 0x80;
        }
        /* Set the bit count: */
        *(sha2_word64*) &context->buffer[SECKM_SHA256_SHORT_BLOCK_LENGTH]
                = context->bitcount;

        /* Final transform: */
        SECKM_SHA256_Transform(context, (sha2_word32*) context->buffer);

        {
            /* Convert TO host byte order */
            int j;
            for (j = 0; j < 8; j++)
            {
                REVERSE32(context->state[j],context->state[j]);
                *d++ = context->state[j];
            }
        }
    }

    /* Clean up state data: */
    MEMSET_BZERO(context, sizeof(*context));
    usedspace = 0;
}

#ifdef FIPS_MODE

struct sha_testvec {
	unsigned char *plaintext;
	unsigned char *digest;
	unsigned char psize;
};

/*
 * SHA256 test vectors from from NIST
 */
#define SHA256_TEST_VECTORS	1

static struct sha_testvec SHA256_tv[] = {
	{
		.plaintext = (unsigned char *) "abc",
		.psize	= 3,
		.digest	= (unsigned char *)
			  "\xba\x78\x16\xbf\x8f\x01\xcf\xea"
			  "\x41\x41\x40\xde\x5d\xae\x22\x23"
			  "\xb0\x03\x61\xa3\x96\x17\x7a\x9c"
			  "\xb4\x10\xff\x61\xf2\x00\x15\xad",
	},
	{
		.plaintext = "\x3b\xc4\x10\xe5\xef\x25\xb2\xea\x5b\xb0\xe5\x15\xc9\xd6\x64\x07\xe9\xd6\x82\x96\xc6\xe1\xbd\x14\xf9\x7b\x95\xd4\x1c\x24\x9f\xeb\x82\x38\xd5\x34\x25\x14\x30\x97\xe0\x7a\x79\x8a\xdf\x37\x5c\xd7\x53\x02\xbe\xa0\x6a\xa8\x50\xa1\x7e\x66\x64\xb6\x8d\x96",
		.psize	= 62,
		.digest = "\x56\x4a\x57\xf4\xda\xa0\xcb\x7d\x2e\xef\x3d\x4c\x2e\xb1\xed\x29\xfd\xb6\xe9\xe8\x8e\x35\x62\x3c\x0e\x30\x8c\xb9\x10\xa8\x7d\xce",
	},
	{
		.plaintext = "\xef\x1a\x07\x28\x77\x22\x4d\x78\x67\xf8\x1c\x57\x21\x01\x92\x80\x46\xf3\xdf\x6f\x32\x44\xef\x74\xc5\xe3\x4a\x07\x69\x25\x38\x5b\x50\x02\x28\x36\xfe\x81\x52\xff\x08\x71\x9c\x19\x1b\x5a\xba\x6f\x3f\xb3\x9a\x50\x5d\x55\xff\x7e\xff\x30\x6c\x81\x58\x3d\x43",
		.psize	= 63,
		.digest = "\x69\x16\xf3\x45\xa6\x92\x0c\xc3\x13\xa8\xbe\xc7\x63\x55\x4c\x64\xbb\xbc\x78\x28\x07\xae\x4c\x78\x1f\xda\x6e\x48\xcd\x1c\x49\x1a",
	},
	{
		.plaintext = "\xc3\x11\xf1\x64\x83\xb3\xad\x91\x21\xa9\x99\xa3\xa5\x2f\x12\x58\x63\x96\x33\x2f\x73\xea\xeb\x28\x62\xd3\x53\xcb\x27\xb9\x3f\x6b\x59\x2b\x14\xac\x9f\x82\xd9\x43\x67\xc1\x90\xfd\xdb\xc0\xc6\x03\x10\x8a\x69\x5c\x81\x03\xda\xb3\xbd\xce\x43\x5b\x5c\x6d\x2b\x90",
		.psize	= 64,
		.digest = "\xa1\xf3\x34\xef\x0e\x59\x88\x90\x65\x48\x0d\xdf\x16\x47\x8f\xd3\xdf\xc3\x0f\xea\x33\xe7\x80\x92\xfe\x1e\x08\x85\x0b\xed\xb9\xea",
	},
};

/* Self Tests SHA256; Please refer to test_vectors.h for the data structures and test vectors used
 * Returns:
 *    0 - all tests passed
 *    1...SHA256_TEST_VECTORS - error (and corresponding test vector number, 1-based)
 */
int SECKM_SHA2_selftest (void) {
	int        i;
    SECKM_SHA256_CTX ctx;
    unsigned char output[SECKM_SHA256_DIGEST_LENGTH];

	for (i = 0; i < SHA256_TEST_VECTORS; i++ ) {
        SECKM_SHA256_Init(&ctx);
        SECKM_SHA256_Update(&ctx, SHA256_tv[i].plaintext, SHA256_tv[i].psize);
        SECKM_SHA256_Final(&ctx, output);

		if (memcmp(output, SHA256_tv[i].digest, SECKM_SHA256_DIGEST_LENGTH)) {
                        LOG_KM ("SHA2 Selftest failed: %d\n", i+1);
			return i+1;
                }
	}
	return 0;
}

#endif /* FIPS_MODE */
