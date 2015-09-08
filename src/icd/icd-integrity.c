/*
 * deviced
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include "sha2.h"
#include "core/log.h"

#define ICD_EXEC_PATH		"/usr/bin/icd"

#define INTEGRITY_NOT_COMPROMISED	1
#define INTEGRITY_COMPROMISED		0
#define ERR_FILE_READ			-1

#define TZIC_IOC_MAGIC		0x9E
#define TZIC_IOCTL_SET_FUSE_REQ	_IO(TZIC_IOC_MAGIC, 1)

static int write_file(const char *path, const char *value)
{
	int fd, ret, len;

	fd = open(path, O_WRONLY|O_CREAT, 0622);
	if (fd < 0)
		return -errno;

	len = strlen(value);

	do {
		ret = write(fd, value, len);
	} while (ret < 0 && errno == EINTR);

	close(fd);

	if (ret < 0)
		return -errno;

	return 0;
}

static int check_file_hash(const char *filename)
{
	int fd;
	struct stat info;
	int fsize = 0;
	int result = INTEGRITY_COMPROMISED;
	SECKM_SHA256_CTX ctx;
	unsigned char digest[SECKM_SHA256_DIGEST_LENGTH];
	unsigned char *input = 0;

	/* icd digest */
	unsigned char hashed[] =
	"\x08\x01\x77\xd8\x5e\xdf\xa2\xe3\x9c\x34\xe7\xd6\xdd\x86\xae\x88\xeb\x19\x1b\xc9\xb6\xdd\x3d\xa2\x80\xd1\xaa\xf5\x1e\x29\x41\x14";

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		goto out_err;

	if (fstat(fd, &info) < 0)
		goto out;

	fsize = info.st_size;

	input = (unsigned char *)malloc(fsize);
	if (!input)
		goto out;

	result = read(fd, input, fsize);
	if (result != fsize) {
		result = ERR_FILE_READ;
		goto out;
	}

	SECKM_SHA256_Init((SECKM_SHA256_CTX*) &ctx);
	SECKM_SHA256_Update((SECKM_SHA256_CTX*) &ctx, input, fsize);
	SECKM_SHA256_Final((SECKM_SHA256_CTX*) &ctx, digest);

	if ((memcmp(hashed, digest, SECKM_SHA256_DIGEST_LENGTH) == 0))
		result = INTEGRITY_NOT_COMPROMISED;
out:
	close(fd);
out_err:
	/*
	 * FIXME: temporarily skip a tamper flag setting
	 * icd package not working
	 */
#if 0
	if (result != INTEGRITY_NOT_COMPROMISED) {
		fd = open("/dev/tzic", O_RDWR);
		if (fd > 0) {
			ioctl(fd, TZIC_IOCTL_SET_FUSE_REQ, &result);
			close(fd);
		}
	}
#endif
	if (input)
		free(input);

	return result;
}

void icd_check_integrity(void)
{
	int ret;
	int check;

	check = check_file_hash(ICD_EXEC_PATH);
	if (check == INTEGRITY_NOT_COMPROMISED)
		ret = write_file("/dev/icd", "1");
	else
		ret = write_file("/dev/icd", "0");
	_I("icd status %d %d", check, ret);
}
