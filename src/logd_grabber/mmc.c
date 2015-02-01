#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/log.h"
#include "config.h"
#include "mmc.h"

static int mmc_write_sectors_per_uah;
static int mmc_read_sectors_per_uah;

static int get_number_sectors(uint64_t *_read, uint64_t *_written)
{
	const char *path = "/sys/block/mmcblk0/stat";
	FILE *fp = NULL;
	int ret;

	fp = fopen(path, "r");
	if (!fp) {
		ret = -errno;
		_E("fopen failed: %s", strerror(errno));
		return ret;
	}
	if (fscanf(fp, "%*s %*s %lld %*s %*s %*s %lld", _read, _written) < 2) {
		_E("Can't read number of sectors from %s", path);
		fclose(fp);
		return -1;
	}

	if (fclose(fp) < 0) {
		ret = -errno;
		_E("fclose failed: %s", strerror(errno));
		return ret;
	}

	return 0;
}

int mmc_init()
{
	mmc_write_sectors_per_uah =
		config_get_int("mmc_write_sectors_per_uah", 2820, NULL);
	mmc_read_sectors_per_uah =
		config_get_int("mmc_read_sectors_per_uah", 4708, NULL);

	return 0;
}

float mmc_power_cons()
{
	uint64_t _read, _written;

	if (get_number_sectors(&_read, &_written) < 0) {
		_E("get_number_sectors failed");
		return 0;
	}

	return _read / mmc_read_sectors_per_uah + _written / mmc_write_sectors_per_uah;
}
