/*
 * test
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
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

#include <sys/stat.h>
#include <fcntl.h>
#include <archive.h>
#include <archive_entry.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "test.h"

#define DEFAULT_READ_BLOCK_SIZE	10240

struct archive_data {
	ssize_t len;
	char *buff;
};

static mode_t default_mode = S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;

static int file_is_dir(const char *file)
{
	struct stat st;

	if (stat(file, &st) < 0)
		return 0;
	if (S_ISDIR(st.st_mode))
		return 1;
	return 0;
}

static struct archive_data* get_data_from_archive(char *path, char *item)
{
	struct archive *arch;
	struct archive_entry *entry;
	struct archive_data *data = NULL;
	int fd;
	int ret;

	if (!path || !item)
		return NULL;

	_R("%s comp path %s, file name %s", __func__, path, item);

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		_E("open fail");
		return NULL;
	}
	arch = archive_read_new();
	if (arch == NULL) {
		_E("Couldn't create archive reader.");
		goto out;
	}
	if (archive_read_support_compression_all(arch) != ARCHIVE_OK) {
		_E("%s(%d)", archive_error_string( arch ), archive_errno( arch ));
		goto out;
	}
	if (archive_read_support_format_all(arch) != ARCHIVE_OK) {
		_E("%s(%d)", archive_error_string( arch ), archive_errno( arch ));
		goto out;
	}
	if (archive_read_open_fd(arch, fd, DEFAULT_READ_BLOCK_SIZE) != ARCHIVE_OK) {
		_E("%s(%d)", archive_error_string( arch ), archive_errno( arch ));
		goto out;
	}

	while ((ret = archive_read_next_header(arch, &entry)) == ARCHIVE_OK)  {
		if (!S_ISREG(archive_entry_mode(entry)))
			continue;
		if (strncmp(archive_entry_pathname(entry), item, strlen(item)))
			continue;
		data = (struct archive_data *)malloc(sizeof(struct archive_data));
		if (!data) {
			_E("malloc fail");
			goto finish;
		}
		data->len = archive_entry_size(entry);
		data->buff = (char *)malloc(sizeof(char)*data->len);
		if (!data->buff) {
			_E("malloc fail");
			goto finish;
		}
		_R("%s file %s size %d", __func__, archive_entry_pathname(entry), data->len);
		ret = archive_read_data(arch, data->buff, data->len);
		if (ret < 0)
			_E("read fail");
		break;
	}
finish:
	/* Close the archives.  */
	if (archive_read_finish(arch) != ARCHIVE_OK)
		_E("Error closing input archive");
out:
	close(fd);
	return data;
}

static void test_save_data_to_file(char *name, struct archive_data *data)
{
	char path[PATH_MAX];
	char ss[PATH_MAX];
	struct stat st;
	int fd;
	int ret;
	int i;

	if (!name || !data) {
		_E("input fail");
		return;
	}
	snprintf(path, sizeof(path), "/tmp/%s", name);
	if (file_is_dir(path))
		return;
	for (i = 0; path[i] != '\0'; ss[i] = path[i], i++)
	{
		if (i == sizeof(ss) - 1) {
			_E("fail");
			return;
		}
		if (((path[i] == '/') || (path[i] == '\\')) && (i > 0)) {
			ss[i] = '\0';
			if (stat(ss, &st) < 0) {
				ret = mkdir(ss, default_mode);
				if (ret < 0) {
					_E("Make Directory is failed");
					return;
				}
			} else if (!S_ISDIR(st.st_mode)) {
				_E("fail to check dir %s", ss);
				return;
			}
		}
	}
	ss[i] = '\0';
	fd = open(ss, O_CREAT | O_TRUNC | O_WRONLY, 0644);
	if (fd < 0) {
		_E("open fail(%s)", strerror(errno));
		return;
	}
	ret = write(fd, data->buff, data->len);
	if (ret < 0)
		_E("write fail");
	close(fd);
	_R("%s %s", __func__, path);
}

static int archive_unit(int argc, char **argv)
{
	struct archive_data *data = NULL;
	struct timespec start;
	struct timespec end;
	long delta;

	if (argc != 4) {
		_E("arg fail");
		goto out;
	}
	clock_gettime(CLOCK_REALTIME, &start);
	data = get_data_from_archive(argv[2], argv[3]);
	clock_gettime(CLOCK_REALTIME, &end);
	delta = (long)((end.tv_sec*1000 + end.tv_nsec/1000000) -
		(start.tv_sec*1000 + start.tv_nsec/1000000));

	_R("%s operation time %ldms", __func__, delta);
	if (!data)
		goto out;
	if (!data->buff) {
		free(data);
		goto out;
	}
	test_save_data_to_file(argv[3], data);
	free(data->buff);
	free(data);
out:
	return 0;
}

static void unit(char *unit, char *status)
{
}

static void archive_init(void *data)
{
}

static void archive_exit(void *data)
{
}

static const struct test_ops archive_test_ops = {
	.priority = TEST_PRIORITY_NORMAL,
	.name     = "archive",
	.init     = archive_init,
	.exit    = archive_exit,
	.unit    = archive_unit,
};

TEST_OPS_REGISTER(&archive_test_ops)
