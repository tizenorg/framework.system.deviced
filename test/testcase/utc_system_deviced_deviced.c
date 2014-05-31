/*
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd All Rights Reserved
 * PROPRIETARY/CONFIDENTIAL
 *
 * This software is the confidential and proprietary information of SAMSUNG
 * ELECTRONICS ("Confidential Information"). You agree and acknowledge that
 * this software is owned by Samsung and you shall not disclose such
 * Confidential Information and shall use it only in accordance with the terms
 * of the license agreement you entered into with SAMSUNG ELECTRONICS. SAMSUNG
 * make no representations or warranties about the suitability of the software,
 * either express or implied, including but not limited to the implied
 * warranties of merchantability, fitness for a particular purpose, or
 * non-infringement. SAMSUNG shall not be liable for any damages suffered by
 * licensee arising out of or related to this software.
 *
 */
#include <tet_api.h>
#include <dd-deviced.h>

#define API_NAME_DEVICED_GET_CMDLINE_NAME "deviced_get_cmdline_name"
#define API_NAME_DEVICED_GET_APPPATH "deviced_get_apppath"
#define API_NAME_DEVICED_CONF_SET_MEMPOLICY "deviced_conf_set_mempolicy"
#define API_NAME_DEVICED_CONF_SET_MEMPOLICY_BYPID "deviced_conf_set_mempolicy_bypid"
#define API_NAME_DEVICED_CONF_SET_PERMANENT "deviced_conf_set_permanent"
#define API_NAME_DEVICED_CONF_SET_PERMANENT_BYPID "deviced_conf_set_permanent_bypid"
#define API_NAME_DEVICED_CONF_SET_VIP "deviced_conf_set_vip"
#define API_NAME_DEVICED_CONF_IS_VIP "deviced_conf_is_vip"
#define API_NAME_DEVICED_SET_TIMEZONE "deviced_set_timezone"
#define API_NAME_DEVICED_INFORM_FOREGRD "deviced_inform_foregrd"
#define API_NAME_DEVICED_INFORM_BACKGRD "deviced_inform_backgrd"
#define API_NAME_DEVICED_INFORM_ACTIVE "deviced_inform_active"
#define API_NAME_DEVICED_INFORM_INACTIVE "deviced_inform_active"
#define API_NAME_DEVICED_REQUEST_POWEROFF "deviced_request_poweroff"
#define API_NAME_DEVICED_REQUEST_ENTERSLEEP "deviced_request_entersleep"
#define API_NAME_DEVICED_REQUEST_LEAVESLEEP "deviced_request_leavesleep"
#define API_NAME_DEVICED_REQUEST_REBOOT "deviced_request_reboot"
#define API_NAME_DEVICED_REQUEST_SET_CPU_MAX_FREQUENCY "deviced_request_set_cpu_max_frequency"
#define API_NAME_DEVICED_REQUEST_SET_CPU_MIN_FREQUENCY "deviced_request_set_cpu_min_frequency"
#define API_NAME_DEVICED_RELEASE_CPU_MAX_FREQUENCY "deviced_release_cpu_max_frequency"
#define API_NAME_DEVICED_RELEASE_CPU_MIN_FREQUENCY "deviced_release_cpu_min_frequency"
#define API_NAME_DEVICED_REQUEST_SET_FACTORY_MODE "deviced_request_set_factory_mode"
#define API_NAME_DEVICED_REQUEST_DUMP_LOG "deviced_request_dump_log"
#define API_NAME_DEVICED_REQUEST_DELETE_DUMP "deviced_request_delete_dump"

static void startup(void);
static void cleanup(void);

void (*tet_startup)(void) = startup;
void (*tet_cleanup)(void) = cleanup;


static void utc_system_deviced_deviced_get_cmdline_name_p(void);
static void utc_system_deviced_deviced_get_cmdline_name_n_1(void);
static void utc_system_deviced_deviced_get_cmdline_name_n_2(void);
static void utc_system_deviced_deviced_get_cmdline_name_n_3(void);
static void utc_system_deviced_deviced_get_apppath_p(void);
static void utc_system_deviced_deviced_get_apppath_n_1(void);
static void utc_system_deviced_deviced_get_apppath_n_2(void);
static void utc_system_deviced_deviced_get_apppath_n_3(void);
static void utc_system_deviced_deviced_conf_set_mempolicy_p(void);
static void utc_system_deviced_deviced_conf_set_mempolicy_n(void);
static void utc_system_deviced_deviced_conf_set_mempolicy_bypid_p(void);
static void utc_system_deviced_deviced_conf_set_mempolicy_bypid_n_1(void);
static void utc_system_deviced_deviced_conf_set_mempolicy_bypid_n_2(void);
static void utc_system_deviced_deviced_conf_set_permanent_p(void);
static void utc_system_deviced_deviced_conf_set_permanent_bypid_p(void);
static void utc_system_deviced_deviced_conf_set_permanent_bypid_n(void);
static void utc_system_deviced_deviced_conf_set_vip_p(void);
static void utc_system_deviced_deviced_conf_set_vip_n(void);
static void utc_system_deviced_deviced_conf_is_vip_p(void);
static void utc_system_deviced_deviced_conf_is_vip_n(void);
static void utc_system_deviced_deviced_set_timezone_p(void);
static void utc_system_deviced_deviced_set_timezone_n(void);
static void utc_system_deviced_deviced_inform_foregrd_p(void);
static void utc_system_deviced_deviced_inform_backgrd_p(void);
static void utc_system_deviced_deviced_inform_active_p(void);
static void utc_system_deviced_deviced_inform_active_n(void);
static void utc_system_deviced_deviced_inform_inactive_p(void);
static void utc_system_deviced_deviced_inform_inactive_n(void);
static void utc_system_deviced_deviced_request_poweroff_p(void);
static void utc_system_deviced_deviced_request_entersleep_p(void);
static void utc_system_deviced_deviced_request_leavesleep_p(void);
static void utc_system_deviced_deviced_request_reboot_p(void);
static void utc_system_deviced_deviced_request_set_cpu_max_frequency_p(void);
static void utc_system_deviced_deviced_request_set_cpu_max_frequency_n(void);
static void utc_system_deviced_deviced_request_set_cpu_min_frequency_p(void);
static void utc_system_deviced_deviced_request_set_cpu_min_frequency_n(void);
static void utc_system_deviced_deviced_release_cpu_max_frequency_p(void);
static void utc_system_deviced_deviced_release_cpu_min_frequency_p(void);
static void utc_system_deviced_deviced_request_set_factory_mode_p_1(void);
static void utc_system_deviced_deviced_request_set_factory_mode_p_2(void);
static void utc_system_deviced_deviced_request_set_factory_mode_n(void);
static void utc_system_deviced_deviced_request_dump_log_p_1(void);
static void utc_system_deviced_deviced_request_dump_log_p_2(void);
static void utc_system_deviced_deviced_request_dump_log_n(void);
static void utc_system_deviced_deviced_request_delete_dump_p(void);
static void utc_system_deviced_deviced_request_delete_dump_n(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	{ utc_system_deviced_deviced_get_cmdline_name_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_get_cmdline_name_n_1, NEGATIVE_TC_IDX },
	{ utc_system_deviced_deviced_get_cmdline_name_n_2, NEGATIVE_TC_IDX },
	{ utc_system_deviced_deviced_get_cmdline_name_n_3, NEGATIVE_TC_IDX },
	{ utc_system_deviced_deviced_get_apppath_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_get_apppath_n_1, NEGATIVE_TC_IDX },
	{ utc_system_deviced_deviced_get_apppath_n_2, NEGATIVE_TC_IDX },
	{ utc_system_deviced_deviced_get_apppath_n_3, NEGATIVE_TC_IDX },
	{ utc_system_deviced_deviced_conf_set_mempolicy_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_conf_set_mempolicy_n, NEGATIVE_TC_IDX },
	{ utc_system_deviced_deviced_conf_set_mempolicy_bypid_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_conf_set_mempolicy_bypid_n_1, NEGATIVE_TC_IDX },
	{ utc_system_deviced_deviced_conf_set_mempolicy_bypid_n_2, NEGATIVE_TC_IDX },
	{ utc_system_deviced_deviced_conf_set_permanent_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_conf_set_permanent_bypid_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_conf_set_permanent_bypid_n, NEGATIVE_TC_IDX },
	{ utc_system_deviced_deviced_conf_set_vip_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_conf_set_vip_n, NEGATIVE_TC_IDX },
	{ utc_system_deviced_deviced_conf_is_vip_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_conf_is_vip_n, NEGATIVE_TC_IDX },
	{ utc_system_deviced_deviced_set_timezone_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_set_timezone_n, NEGATIVE_TC_IDX },
	{ utc_system_deviced_deviced_inform_foregrd_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_inform_backgrd_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_inform_active_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_inform_active_n, NEGATIVE_TC_IDX },
	{ utc_system_deviced_deviced_inform_inactive_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_inform_inactive_n, NEGATIVE_TC_IDX },
//	{ utc_system_deviced_deviced_request_poweroff_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_request_entersleep_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_request_leavesleep_p, POSITIVE_TC_IDX },
//	{ utc_system_deviced_deviced_request_reboot_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_request_set_cpu_max_frequency_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_request_set_cpu_max_frequency_n, NEGATIVE_TC_IDX },
	{ utc_system_deviced_deviced_request_set_cpu_min_frequency_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_request_set_cpu_min_frequency_n, NEGATIVE_TC_IDX },
	{ utc_system_deviced_deviced_release_cpu_max_frequency_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_release_cpu_min_frequency_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_request_set_factory_mode_p_1, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_request_set_factory_mode_p_2, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_request_set_factory_mode_n, NEGATIVE_TC_IDX },
	{ utc_system_deviced_deviced_request_dump_log_p_1, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_request_dump_log_p_2, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_request_dump_log_n, NEGATIVE_TC_IDX },
	{ utc_system_deviced_deviced_request_delete_dump_p, POSITIVE_TC_IDX },
	{ utc_system_deviced_deviced_request_delete_dump_n, NEGATIVE_TC_IDX },
	{ NULL, 0 },
};

static void startup(void)
{
}

static void cleanup(void)
{
}

/**
 * @brief Positive test case of deviced_get_cmdline_name()
 */
static void utc_system_deviced_deviced_get_cmdline_name_p(void)
{
	pid_t pid;
	char str[1024];
	int ret;

	pid = getpid();

	ret = deviced_get_cmdline_name(pid, str, sizeof(str));
	dts_check_eq(API_NAME_DEVICED_GET_CMDLINE_NAME, ret, 0);
}

/**
 * @brief Negative test case of deviced_get_cmdline_name()
 */
static void utc_system_deviced_deviced_get_cmdline_name_n_1(void)
{
	char str[1024];
	int ret;

	ret = deviced_get_cmdline_name(-1, str, sizeof(str));
	dts_check_ne(API_NAME_DEVICED_GET_CMDLINE_NAME, ret, 0);
}

/**
 * @brief Negative test case of deviced_get_cmdline_name()
 */
static void utc_system_deviced_deviced_get_cmdline_name_n_2(void)
{
	pid_t pid;
	char str[1024];
	int ret;

	pid = getpid();

	ret = deviced_get_cmdline_name(pid, NULL, sizeof(str));
	dts_check_ne(API_NAME_DEVICED_GET_CMDLINE_NAME, ret, 0);
}

/**
 * @brief Negative test case of deviced_get_cmdline_name()
 */
static void utc_system_deviced_deviced_get_cmdline_name_n_3(void)
{
	pid_t pid;
	char str[1024];
	int ret;

	pid = getpid();

	ret = deviced_get_cmdline_name(pid, str, -1);
	dts_check_ne(API_NAME_DEVICED_GET_CMDLINE_NAME, ret, 0);
}

/**
 * @brief Positive test case of deviced_get_apppath()
 */
static void utc_system_deviced_deviced_get_apppath_p(void)
{
	pid_t pid;
	char str[1024];
	int ret;

	pid = getpid();

	ret = deviced_get_apppath(pid, str, sizeof(str));
	dts_check_eq(API_NAME_DEVICED_GET_APPPATH, ret, 0);
}

/**
 * @brief Negative test case of deviced_get_apppath()
 */
static void utc_system_deviced_deviced_get_apppath_n_1(void)
{
	char str[1024];
	int ret;

	ret = deviced_get_apppath(-1, str, sizeof(str));
	dts_check_ne(API_NAME_DEVICED_GET_APPPATH, ret, 0);
}

/**
 * @brief Negative test case of deviced_get_apppath()
 */
static void utc_system_deviced_deviced_get_apppath_n_2(void)
{
	pid_t pid;
	char str[1024];
	int ret;

	pid = getpid();

	ret = deviced_get_apppath(pid, NULL, sizeof(str));
	dts_check_ne(API_NAME_DEVICED_GET_APPPATH, ret, 0);
}

/**
 * @brief Negative test case of deviced_get_apppath()
 */
static void utc_system_deviced_deviced_get_apppath_n_3(void)
{
	pid_t pid;
	char str[1024];
	int ret;

	pid = getpid();

	ret = deviced_get_apppath(pid, str, -1);
	dts_check_ne(API_NAME_DEVICED_GET_APPPATH, ret, 0);
}

/**
 * @brief Positive test case of deviced_conf_set_mempolicy()
 */
static void utc_system_deviced_deviced_conf_set_mempolicy_p(void)
{
	int ret;

	ret = deviced_conf_set_mempolicy(OOM_LIKELY);
	dts_check_eq(API_NAME_DEVICED_CONF_SET_MEMPOLICY, ret, 0);
}

/**
 * @brief Negative test case of deviced_conf_set_mempolicy()
 */
static void utc_system_deviced_deviced_conf_set_mempolicy_n(void)
{
	int ret;

	ret = deviced_conf_set_mempolicy(-1);
	dts_check_ne(API_NAME_DEVICED_CONF_SET_MEMPOLICY, ret, 0);
}

/**
 * @brief Positive test case of deviced_conf_set_mempolicy_bypid()
 */
static void utc_system_deviced_deviced_conf_set_mempolicy_bypid_p(void)
{
	pid_t pid;
	int ret;

	pid = getpid();

	ret = deviced_conf_set_mempolicy_bypid(pid, OOM_LIKELY);
	dts_check_eq(API_NAME_DEVICED_CONF_SET_MEMPOLICY_BYPID, ret, 0);
}

/**
 * @brief Negative test case of deviced_conf_set_mempolicy_bypid()
 */
static void utc_system_deviced_deviced_conf_set_mempolicy_bypid_n_1(void)
{
	int ret;

	ret = deviced_conf_set_mempolicy_bypid(-1, OOM_LIKELY);
	dts_check_ne(API_NAME_DEVICED_CONF_SET_MEMPOLICY_BYPID, ret, 0);
}

/**
 * @brief Negative test case of deviced_conf_set_mempolicy_bypid()
 */
static void utc_system_deviced_deviced_conf_set_mempolicy_bypid_n_2(void)
{
	pid_t pid;
	int ret;

	pid = getpid();

	ret = deviced_conf_set_mempolicy_bypid(pid, -1);
	dts_check_ne(API_NAME_DEVICED_CONF_SET_MEMPOLICY_BYPID, ret, 0);
}

/**
 * @brief Positive test case of deviced_conf_set_permanent()
 */
static void utc_system_deviced_deviced_conf_set_permanent_p(void)
{
	int ret;

	ret = deviced_conf_set_permanent();
	dts_check_eq(API_NAME_DEVICED_CONF_SET_PERMANENT, ret, 0);
}

/**
 * @brief Positive test case of deviced_conf_set_permanent_bypid()
 */
static void utc_system_deviced_deviced_conf_set_permanent_bypid_p(void)
{
	pid_t pid;
	int ret;

	pid = getpid();

	ret = deviced_conf_set_permanent_bypid(pid);
	dts_check_eq(API_NAME_DEVICED_CONF_SET_PERMANENT_BYPID, ret, 0);
}

/**
 * @brief Negative test case of deviced_conf_set_permanent_bypid()
 */
static void utc_system_deviced_deviced_conf_set_permanent_bypid_n(void)
{
	int ret;

	ret = deviced_conf_set_permanent_bypid(-1);
	dts_check_ne(API_NAME_DEVICED_CONF_SET_PERMANENT_BYPID, ret, 0);
}

/**
 * @brief Positive test case of deviced_conf_set_vip()
 */
static void utc_system_deviced_deviced_conf_set_vip_p(void)
{
	pid_t pid;
	int ret;

	pid = getpid();

	ret = deviced_conf_set_vip(pid);
	dts_check_eq(API_NAME_DEVICED_CONF_SET_VIP, ret, 0);
}

/**
 * @brief Negative test case of deviced_conf_set_vip()
 */
static void utc_system_deviced_deviced_conf_set_vip_n(void)
{
	int ret;

	ret = deviced_conf_set_vip(-1);
	dts_check_ne(API_NAME_DEVICED_CONF_SET_VIP, ret, 0);
}

/**
 * @brief Positive test case of deviced_conf_is_vip()
 */
static void utc_system_deviced_deviced_conf_is_vip_p(void)
{
	pid_t pid;
	int ret;

	pid = getpid();

	ret = deviced_conf_is_vip(pid);
	dts_check_ge(API_NAME_DEVICED_CONF_IS_VIP, ret, 0);
}

/**
 * @brief Negative test case of deviced_conf_is_vip()
 */
static void utc_system_deviced_deviced_conf_is_vip_n(void)
{
	int ret;

	ret = deviced_conf_is_vip(-1);
	dts_check_lt(API_NAME_DEVICED_CONF_IS_VIP, ret, 0);
}

/**
 * @brief Positive test case of deviced_set_timezone()
 */
static void utc_system_deviced_deviced_set_timezone_p(void)
{
	int ret;

	ret = deviced_set_timezone("/usr/share/zoneinfo/Asia/Seoul");
	dts_check_eq(API_NAME_DEVICED_SET_TIMEZONE, ret, 0);
}

/**
 * @brief Negative test case of deviced_set_timezone()
 */
static void utc_system_deviced_deviced_set_timezone_n(void)
{
	int ret;

	ret = deviced_set_timezone(NULL);
	dts_check_ne(API_NAME_DEVICED_SET_TIMEZONE, ret, 0);
}

/**
 * @brief Positive test case of deviced_inform_foregrd()
 */
static void utc_system_deviced_deviced_inform_foregrd_p(void)
{
	int ret;

	ret = deviced_inform_foregrd();
	dts_check_eq(API_NAME_DEVICED_INFORM_FOREGRD, ret, 0);
}

/**
 * @brief Positive test case of deviced_inform_backgrd()
 */
static void utc_system_deviced_deviced_inform_backgrd_p(void)
{
	int ret;

	ret = deviced_inform_backgrd();
	dts_check_eq(API_NAME_DEVICED_INFORM_BACKGRD, ret, 0);
}

/**
 * @brief Positive test case of deviced_inform_active()
 */
static void utc_system_deviced_deviced_inform_active_p(void)
{
	pid_t pid;
	int ret;

	pid = getpid();

	ret = deviced_inform_active(pid);
	dts_check_eq(API_NAME_DEVICED_INFORM_ACTIVE, ret, 0);
}

/**
 * @brief Negative test case of deviced_inform_active()
 */
static void utc_system_deviced_deviced_inform_active_n(void)
{
	int ret;

	ret = deviced_inform_active(-1);
	dts_check_ne(API_NAME_DEVICED_INFORM_ACTIVE, ret, 0);
}

/**
 * @brief Positive test case of deviced_inform_inactive()
 */
static void utc_system_deviced_deviced_inform_inactive_p(void)
{
	pid_t pid;
	int ret;

	pid = getpid();

	ret = deviced_inform_inactive(pid);
	dts_check_eq(API_NAME_DEVICED_INFORM_INACTIVE, ret, 0);
}

/**
 * @brief Negative test case of deviced_inform_inactive()
 */
static void utc_system_deviced_deviced_inform_inactive_n(void)
{
	int ret;

	ret = deviced_inform_inactive(-1);
	dts_check_ne(API_NAME_DEVICED_INFORM_INACTIVE, ret, 0);
}

/**
 * @brief Positive test case of deviced_request_poweroff()
 */
static void utc_system_deviced_deviced_request_poweroff_p(void)
{
	int ret;

	ret = deviced_request_poweroff();
	dts_check_eq(API_NAME_DEVICED_REQUEST_POWEROFF, ret, 0);
}

/**
 * @brief Positive test case of deviced_request_entersleep()
 */
static void utc_system_deviced_deviced_request_entersleep_p(void)
{
	int ret;

	ret = deviced_request_entersleep();
	dts_check_eq(API_NAME_DEVICED_REQUEST_ENTERSLEEP, ret, 0);
}

/**
 * @brief Positive test case of deviced_request_leavesleep()
 */
static void utc_system_deviced_deviced_request_leavesleep_p(void)
{
	int ret;

	ret = deviced_request_leavesleep();
	dts_check_eq(API_NAME_DEVICED_REQUEST_LEAVESLEEP, ret, 0);
}

/**
 * @brief Positive test case of deviced_request_reboot()
 */
static void utc_system_deviced_deviced_request_reboot_p(void)
{
	int ret;

	ret = deviced_request_reboot();
	dts_check_eq(API_NAME_DEVICED_REQUEST_REBOOT, ret, 0);
}

/**
 * @brief Positive test case of deviced_request_set_cpu_max_frequency()
 */
static void utc_system_deviced_deviced_request_set_cpu_max_frequency_p(void)
{
	int ret;

	ret = deviced_request_set_cpu_max_frequency(60);
	dts_check_eq(API_NAME_DEVICED_REQUEST_SET_CPU_MAX_FREQUENCY, ret, 0);
}

/**
 * @brief Negative test case of deviced_request_set_cpu_max_frequency()
 */
static void utc_system_deviced_deviced_request_set_cpu_max_frequency_n(void)
{
	int ret;

	ret = deviced_request_set_cpu_max_frequency(-1);
	dts_check_ne(API_NAME_DEVICED_REQUEST_SET_CPU_MAX_FREQUENCY, ret, 0);
}

/**
 * @brief Positive test case of deviced_request_set_cpu_min_frequency()
 */
static void utc_system_deviced_deviced_request_set_cpu_min_frequency_p(void)
{
	int ret;

	ret = deviced_request_set_cpu_min_frequency(60);
	dts_check_eq(API_NAME_DEVICED_REQUEST_SET_CPU_MIN_FREQUENCY, ret, 0);
}

/**
 * @brief Negative test case of deviced_request_set_cpu_min_frequency()
 */
static void utc_system_deviced_deviced_request_set_cpu_min_frequency_n(void)
{
	int ret;

	ret = deviced_request_set_cpu_min_frequency(-1);
	dts_check_ne(API_NAME_DEVICED_REQUEST_SET_CPU_MIN_FREQUENCY, ret, 0);
}

/**
 * @brief Positive test case of deviced_release_cpu_max_frequency()
 */
static void utc_system_deviced_deviced_release_cpu_max_frequency_p(void)
{
	int ret;

	ret = deviced_release_cpu_max_frequency();
	dts_check_eq(API_NAME_DEVICED_RELEASE_CPU_MAX_FREQUENCY, ret, 0);
}

/**
 * @brief Positive test case of deviced_release_cpu_min_frequency()
 */
static void utc_system_deviced_deviced_release_cpu_min_frequency_p(void)
{
	int ret;

	ret = deviced_release_cpu_min_frequency();
	dts_check_eq(API_NAME_DEVICED_RELEASE_CPU_MIN_FREQUENCY, ret, 0);
}

/**
 * @brief Positive test case of deviced_request_set_factory_mode()
 */
static void utc_system_deviced_deviced_request_set_factory_mode_p_1(void)
{
	int ret;

	ret = deviced_request_set_factory_mode(1);
	dts_check_eq(API_NAME_DEVICED_REQUEST_SET_FACTORY_MODE, ret, 1, "Enable factory mode");
}

/**
 * @brief Positive test case of deviced_request_set_factory_mode()
 */
static void utc_system_deviced_deviced_request_set_factory_mode_p_2(void)
{
	int ret;

	ret = deviced_request_set_factory_mode(0);
	dts_check_eq(API_NAME_DEVICED_REQUEST_SET_FACTORY_MODE, ret, 0, "Disable factory mode");
}

/**
 * @brief Negative test case of deviced_request_set_factory_mode()
 */
static void utc_system_deviced_deviced_request_set_factory_mode_n(void)
{
	int ret;

	ret = deviced_request_set_factory_mode(-1);
	dts_check_ne(API_NAME_DEVICED_REQUEST_SET_FACTORY_MODE, ret, 0);
}

/**
 * @brief Positive test case of deviced_request_dump_log()
 */
static void utc_system_deviced_deviced_request_dump_log_p_1(void)
{
	int ret;

	ret = deviced_request_dump_log(AP_DUMP);
	dts_check_ge(API_NAME_DEVICED_REQUEST_DUMP_LOG, ret, 0);
}

/**
 * @brief Positive test case of deviced_request_dump_log()
 */
static void utc_system_deviced_deviced_request_dump_log_p_2(void)
{
	int ret;

	ret = deviced_request_dump_log(CP_DUMP);
	dts_check_eq(API_NAME_DEVICED_REQUEST_DUMP_LOG, ret, 0);
}

/**
 * @brief Positive test case of deviced_request_dump_log()
 */
static void utc_system_deviced_deviced_request_dump_log_n(void)
{
	int ret;

	ret = deviced_request_dump_log(-1);
	dts_check_ne(API_NAME_DEVICED_REQUEST_DUMP_LOG, ret, 0);
}

/**
 * @brief Positive test case of deviced_request_delete_dump()
 */
static void utc_system_deviced_deviced_request_delete_dump_p(void)
{
	int ret;

	ret = deviced_request_delete_dump("");
	dts_check_eq(API_NAME_DEVICED_REQUEST_DELETE_DUMP, ret, 0);
}

/**
 * @brief Negative test case of deviced_request_delete_dump()
 */
static void utc_system_deviced_deviced_request_delete_dump_n(void)
{
	int ret;

	ret = deviced_request_delete_dump(NULL);
	dts_check_ne(API_NAME_DEVICED_REQUEST_DELETE_DUMP, ret, 0);
}
