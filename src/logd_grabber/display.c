#define _GNU_SOURCE
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "core/log.h"
#include "config.h"
#include "display.h"
#include "macro.h"

static int curr_brightness;
static int display_state;
static float total_power_cons = 0;
static time_t last_change_time;

static int get_brightness_file(char *file)
{
	const char *brightness_dir = "/sys/class/backlight/";
	struct dirent *entry = NULL;
	DIR *dir = NULL;
	int ret;
	char buf[PATH_MAX];

	dir = opendir(brightness_dir);
	if (!dir) {
		ret = -errno;
		_E("opendir failed: %s", strerror(errno));
		return ret;
	}

	do {
		entry = readdir(dir);
		if (!entry) {
			ret = -errno;
			_E("readdir failed: %s", strerror(errno));
			closedir(dir);
			return ret;
		}
	} while (entry->d_name[0] == '.');

	ret = sprintf(file, "%s/%s/brightness", brightness_dir, entry->d_name);
	if (ret < 0) {
		ret = -errno;
		_E("asprintf failed: %s", strerror(errno));
		closedir(dir);
		return ret;
	}
	ret = closedir(dir);
	if (ret < 0) {
		ret = -errno;
		_E("closedir failed: %s", strerror(errno));
		return ret;
	}

	return 0;
}

static int read_curr_brightness()
{
	FILE *fp = NULL;
	char file[PATH_MAX];
	int brightness;
	int ret;

	ret = get_brightness_file(file);
	if (ret < 0) {
		_E("get_brightness_file failed");
		return ret;
	}

	fp = fopen(file, "r");
	if (!fp) {
		ret = -errno;
		_E("fopen failed: %s", strerror(errno));
		return ret;
	}

	if (fscanf(fp, "%d", &brightness) != 1) {
		ret = -errno;
		fclose(fp);
		_E("fscanf failed: %s", strerror(errno));
		return ret;
	}

	if (fclose(fp) < 0) {
		ret = -errno;
		_E("close failed: %s", strerror(errno));
		return ret;
	}

	return brightness;
}

float backlight_curr_power_cons()
{
	double a = config_get_double("backlight_a", 0, NULL);
	double b = config_get_double("backlight_b", 0, NULL);
	double c = config_get_double("backlight_c", 0, NULL);
	double k = config_get_double("backlight_k", 0, NULL);

	double x = curr_brightness;

	if (!display_state)
		return 0;
	return a * x * x * x + b * x * x + c * x + k;
}

static void recalc_total_power_cons()
{
	float curr_power_cons = backlight_curr_power_cons();
	time_t t = getSecTime();

	total_power_cons +=
		(curr_power_cons / 3600 * 1000) * (t - last_change_time);
	last_change_time = t;
}

int display_init()
{
	display_state = 1; /* TODO: check it, may be neet to read it by vconf */
	curr_brightness = read_curr_brightness();
	if (curr_brightness < 0) {
		_E("read_curr_brightness failed: curr_brightness set as 0");
		curr_brightness = 0;
	}
	last_change_time = getSecTime();

	return 0;
}

int brightness_change_event_handler(struct logd_grabber_event *event)
{
	recalc_total_power_cons();

	curr_brightness = atoi(event->message);

	return 0;
}

int display_on_off_event_handler(struct logd_grabber_event *event)
{
	recalc_total_power_cons();

	if (event->action == LOGD_ON) {
		display_state = 1;
	} else if (event->action == LOGD_OFF) {
		display_state = 0;
	}

	return 0;
}

float get_display_curr_power_cons()
{
	return backlight_curr_power_cons();
}

float get_display_total_power_cons()
{
	recalc_total_power_cons();

	return total_power_cons;
}
