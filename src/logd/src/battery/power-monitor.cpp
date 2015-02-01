#include <algorithm>
#include <logd.h>
#include <logd-db.h>
#include <iostream>
#include <map>
#include <string>
#include <stdlib.h>
#include <vector>
#include <string.h>
#include <unistd.h>
#include <time.h>

using namespace std;

static uint64_t getTimeUSec()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

struct Stat {
	string appid;
	float totalCons; /* uAh */
	float currentCons; /* mA */
};

static vector<Stat> newStat;
static map<string, Stat> oldStat;

static enum logd_db_query proc_stat_cb(const struct logd_proc_stat *proc_stat, void *user_data)
{
	Stat stat;

	stat.totalCons = proc_stat->stime_power_cons + proc_stat->utime_power_cons;
	stat.appid = proc_stat->application;
	newStat.push_back(stat);


	return LOGD_DB_QUERY_CONTINUE;
}

int main(int argc, char **argv)
{
	float totalCurrent = 0;
	float total = 0;
	uint64_t lastTime = 0;

	while (1) {
		uint64_t curTime = getTimeUSec();
		newStat.clear();
		logd_foreach_proc_stat(&proc_stat_cb, NULL);
		totalCurrent = total = 0;

		for (auto it = newStat.begin(); it != newStat.end(); ++it) {
			it->currentCons = 0;
			if (oldStat.count(it->appid)) {
				total += it->totalCons;
				it->currentCons =
					(it->totalCons - oldStat[it->appid].totalCons) * 3.6 /
					(curTime - lastTime) * 1000;
				totalCurrent += it->currentCons;
			}
		}
		oldStat.clear();
		lastTime = curTime;

		sort(newStat.begin(), newStat.end(),
			[] (Stat lhs, Stat rhs)
			{
				return lhs.currentCons > rhs.currentCons;
			});


		printf("%-50.50s %15s %15s\n", "Application", "power cons, uah", "current, mA");
		auto it = newStat.begin();
		for (size_t i = 0; i < newStat.size(); ++i, ++it) {
			if (i < 20 && totalCurrent)
				printf("%-50.50s %15.2f %15.2f (%.2f%%)\n", it->appid.c_str(),
					it->totalCons, it->currentCons, it->currentCons / totalCurrent * 100);
			oldStat[it->appid] = *it;
		}
		printf("\n%-50.50s %15.2f  %15.4f\n", "Total", total, totalCurrent);
		sleep(1);
		printf("\033[2J\033[1;1H");
	}
}
