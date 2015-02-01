#include <db.h>
#include <errno.h>
#include <macro.h>
#include <padvisor.h>

int main(void)
{
	struct logd_power_advisor *lpa = NULL;
	struct device_power_consumption *dpc =
		logd_get_device_power_cons(1, 2000000000);

	if (!dpc) {
		puts("logd_get_device_power_cons failed");
		return -errno;
	}

	puts("Device id  | Working time  | persent");
	for (int i = 0; i < LOGD_OBJECT_MAX; ++i) {
		printf("%9d  | %12d  | %.4f\n", i, dpc->device_cons[i].time,
			dpc->device_cons[i].percentage * 100);
	}
	logd_free_device_power_cons(dpc);


	lpa = logd_get_power_advisor();
	if (lpa != NULL) {
		for (int i = 0; i < lpa->idle_devices_used_num; i++) {
			printf("device - %d, time - %lld\n",
				lpa->idle_devices[i].device,
				lpa->idle_devices[i].idle_time);
		}
		for (int i = 0; i < lpa->proc_stat_used_num; i++) {
			printf("application - %s, percentage - %.2f, power cons(uAh) - %f  utime - %lld, stime - %lld\n",
				lpa->procs[i].application, lpa->procs[i].percentage * 100,
				lpa->procs[i].utime_power_cons + lpa->procs[i].stime_power_cons,
				lpa->procs[i].utime, lpa->procs[i].stime);
		}
	} else {
		printf("logd_get_power_advisor() returned NULL\n");
		return -errno;
	}

	logd_free_power_advisor(lpa);

	return 0;
}
