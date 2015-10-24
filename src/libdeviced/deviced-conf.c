/*
 * deviced
 *
 * Copyright (c) 2012 - 2013 Samsung Electronics Co., Ltd.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>

#include "log.h"
#include "deviced-priv.h"
#include "dd-deviced.h"
#include "dbus.h"
#include "score-defines.h"

#define OOMADJ_SET		"oomadj_set"

int util_oomadj_set(int pid, int oomadj_val)
{
	char *pa[4];
	char buf1[SYSTEM_NOTI_MAXARG];
	char buf2[SYSTEM_NOTI_MAXARG];
	int ret;

	snprintf(buf1, sizeof(buf1), "%d", pid);
	snprintf(buf2, sizeof(buf2), "%d", oomadj_val);

	pa[0] = OOMADJ_SET;
	pa[1] = "2";
	pa[2] = buf1;
	pa[3] = buf2;

	ret = dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_PROCESS, DEVICED_INTERFACE_PROCESS,
			pa[0], "siss", pa);

	_D("%s-%s : %d", DEVICED_INTERFACE_PROCESS, pa[0], ret);
	return ret;
}

API int deviced_conf_set_mempolicy_bypid(int pid, enum mem_policy mempol)
{
	if (pid < 1)
		return -1;

	int oomadj_val = 0;

	switch (mempol) {
	case OOM_LIKELY:
		oomadj_val = OOMADJ_BACKGRD_UNLOCKED;
		break;
	case OOM_IGNORE:
		oomadj_val = OOMADJ_SU;
		break;
	default:
		return -1;
	}

	return util_oomadj_set(pid, oomadj_val);
}
