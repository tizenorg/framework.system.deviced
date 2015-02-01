#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vconf.h>

#include "macro.h"

static int default_timeout = 10;

struct calibration_test {
	const char *description;
	int (*func)(void *);
	void *param;
	int consumption;
};

static int run_test_battery(int timeout);
static int (*run_test)(int) = run_test_battery;

static int system_cmd(const char *cmd, char *const args[])
{
	pid_t pid = fork();


	if (pid == 0) {
		if (execv(cmd, args) < 0) {
			perror("execv: ");
		}
	} else if (pid == -1) {
		fprintf(stderr, "Can't run \"%s\"\n", cmd);
		return -errno;
	} else {
		wait(NULL);
	}

	return 0;
}

static int current_battery_level(void)
{
	const char *cmd = "/bin/cat /sys/class/power_supply/battery/uevent |\
/bin/grep POWER_SUPPLY_CAPACITY_RAW | /usr/bin/cut -d '=' -f 2";
	FILE *fp = NULL;
	int level;

	fp = popen(cmd, "r");
	if (fp == NULL) {
		fprintf(stderr, "Can't obtain battery level\n");
		return -errno;
	}

	if (fscanf(fp, "%d", &level) <= 0) {
		fprintf(stderr, "Can't read power supply\n");
		fclose(fp);
		return -EIO;
	}

	fclose(fp);

	return level;
}

static int power_supply(void)
{
	FILE *fp = NULL;
	int ps;

	/* TODO: move hardware depended settings to config file */
	fp = fopen("/sys/class/power_supply/max17047-fuelgauge/current_avg", "r");
	if (fp == NULL) {
		fprintf(stderr, "Can't open current_now\n");
		return -errno;
	}

	if (fscanf(fp, "%d", &ps) <= 0) {
		fprintf(stderr, "Can't read power supply\n");
		fclose(fp);
		return -EIO;
	}
	fclose(fp);

	return -ps;
}

static int write_number(const char *file, int number)
{
	FILE *fp = NULL;

	fp = fopen(file, "w");
	if (fp == NULL) {
		fprintf(stderr, "Can't open current_now\n");
		return -errno;
	}

	fprintf(fp, "%d", number);
	fclose(fp);

	return 0;
}

static int run_test_battery(int timeout)
{
	int level1 = current_battery_level();
	sleep(timeout);
	int level2 = current_battery_level();

	return level1 - level2;
}

static int run_test_power_supply(int timeout)
{
	sleep(timeout);

	return power_supply();
}

/* display */
static void set_brightness(int value)
{
	write_number("/sys/class/backlight/s6d6aa1-bl/brightness", value);
}

static void display_on(void)
{
	char cmd[] = "/usr/bin/xset";
	char *const args[] = { cmd, "dpms", "force", "on", NULL };
	system_cmd(cmd, args);
}

static void display_off(void)
{
	char cmd[] = "/usr/bin/xset";
	char *const args[] = { cmd, "dpms", "force", "off", NULL };
	system_cmd(cmd, args);
}

static int run_display_test(void *brightness)
{
	int consumption = 0;
	display_on();
	set_brightness((int)brightness);
	consumption = run_test(default_timeout);
	display_off();

	return consumption;
}

/* default */
static int run_default_test(void *user_data)
{
	display_off();

	return run_test(default_timeout);
}

/* wi-fi */
static int wifi_on(void)
{
	char cmd[] = "/usr/bin/wifi-qs";
	char *const args[] = { cmd, NULL };
	system_cmd(cmd, args);
	sleep(10);

	return 0;
}

static int run_wifi_test(void *user_data)
{
	if (wifi_on() < 0)
		return -1;

	return run_test(default_timeout);
}

/* gps */
static int run_gps_test(void *user_data)
{
	vconf_set_int(VCONFKEY_LOCATION_ENABLED, 1);
	int consumption = run_test(default_timeout);
	vconf_set_int(VCONFKEY_LOCATION_ENABLED, 0);
	return consumption;
}

int main(int argc, char *argv[])
{
	struct calibration_test tests[] = {
		{"default", run_default_test, NULL, 0},
		{"display 1", run_display_test, (void*)1, 0},
		{"display 20", run_display_test, (void*)20, 0},
		{"display 40", run_display_test, (void*)40, 0},
		{"display 60", run_display_test, (void*)60, 0},
		{"display 80", run_display_test, (void*)80, 0},
		{"display 100", run_display_test, (void*)100, 0},
		{"gps", run_gps_test, NULL, 0},
		{"wifi", run_wifi_test, NULL, 0},
	};

	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-p") == 0) {
			run_test = run_test_power_supply;
			default_timeout = 30;
		}
	}
	signal(SIGHUP, SIG_IGN);
	printf("timeout = %d\n", default_timeout);

	for (size_t i = 0; i < ARRAY_SIZE(tests); ++i) {
		tests[i].consumption = tests[i].func(tests[i].param);
		if (tests[i].consumption >= 0) {
			printf("%s: %d %.2f\n", tests[i].description, tests[i].consumption,
				 (float)tests[i].consumption / tests[0].consumption);
		} else {
			fprintf(stderr, "Can't run \"%s\" test\n", tests[i].description);
		}
	}

	return 0;
}
