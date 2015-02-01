#ifndef _LOGD_MACRO_H_
#define _LOGD_MACRO_H_

#include <time.h>
#include "core/common.h"

#define USEC_PER_SEC 1000000ull
#define SEC_PER_DAY 86400
#define DAYS_PER_WEEK 7
#define time_after(a, b) ((int64_t)(b) - (int64_t)(a) < 0)
#define time_after_eq(a, b) ((int64_t)(b) - (int64_t)(a) <= 0)

//#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

#define STRINGIZE(x) #x
#define STRINGIZE_VALUE_OF(x) STRINGIZE(x)

#define API __attribute__ ((visibility ("default")))

#define STATIC_ASSERT(CONDITION,MESSAGE) typedef char static_assert_\
##MESSAGE[(CONDITION)?0:-1]

#define CHECK_RET(ret, func_name)					\
	do {								\
		if (ret < 0) {						\
			_E(func_name" failed: error code %d", ret);	\
			return ret;					\
		}							\
	} while (0);

__attribute__ ((unused)) static time_t getSecTime()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec;
}

#endif /* _LOGD_MACRO_H_ */
