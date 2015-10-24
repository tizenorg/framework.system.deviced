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


#ifndef __DD_DEVICED__
#define __DD_DEVICED__

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include "dd-deviced-managed.h"

/**
 * @file        dd-deviced.h
 * @defgroup    CAPI_SYSTEM_DEVICED_UTIL_MODULE Util
 * @ingroup     CAPI_SYSTEM_DEVICED
 * @section CAPI_SYSTEM_DEVICED_UTIL_MODULE_HEADER Required Header
 *   \#include <dd-deviced.h>
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup CAPI_SYSTEM_DEVICED_UTIL_MODULE
 * @{
 */

/**
 * @brief Policy for low memory
 */
enum mem_policy {
	OOM_LIKELY,	/**< For miscellaneous applications */
	OOM_IGNORE	/**< For daemons */
};

#define CRASH_TICKET_PATH               "/opt/usr/share/crash/ticket"
#define FORCED_DUMP_DIR_PATH            "/opt/usr/media/SLP_debug"
#define FORCED_DUMP_ZIP_PATH            "/opt/usr/media/SLP_debug/zip"

/**
 * @brief Logs dump type
 */
enum dump_log_type {
	AP_DUMP = 0,	/**< Application logs dump */
	CP_DUMP = 1,	/**< Modem logs dump */
	ALL_DUMP = 2	/**< All logs dump - application and modem */
};

/* deviced_util */

/**
 * @fn int deviced_get_cmdline_name(pid_t pid, char *cmdline, size_t cmdline_size)
 * @brief This API is used to get the file name of command line of the process from the proc fs.
 * 		Caller process MUST allocate enough memory for the cmdline parameter. \n
 * 		Its size should be assigned to cmdline_size. \n
 * 		Internally it reads the 1st argument of /proc/{pid}/cmdline and copies it to cmdline.
 * @param[in] pid pid of the process that you want to get the file name of command line
 * @param[out] cmdline command line of the process that you want to get the file name <br>
 * 			Callers should allocate memory to this parameter before calling this function.
 * 			The allocated memory size must be large enough to store the file name.
 * 			The result will include the terminating null byte('\0') at the end of the string.
 * @param[in] cmdline_size
 * @return 0 on success, negative value if failed.\n
 *         If the size of cmdline is smaller than the result, it will return -75 and  \n
 *         errno will be set as EOVERFLOW. \n
 *         If the function cannot open /proc/%d/cmdline file then it will return -3 and \n
 *         errno will be set as ESRCH.
 */
int deviced_get_cmdline_name(pid_t pid, char *cmdline,
				    size_t cmdline_size);

/**
 * @fn int deviced_get_apppath(pid_t pid, char *app_path, size_t app_path_size)
 * @brief This API is used to get the execution path of the process specified by the pid parameter.\n
 * 		Caller process MUST allocate enough memory for the app_path parameter. \n
 * 		Its size should be assigned to app_path_size. \n
 * 		Internally it reads a link of /proc/{pid}/exe and copies the path to app_path.
 * @param[in] pid pid of the process that you want to get the executed path
 * @param[out] app_path the executed file path of the process <br>
 * 			Callers should allocate memory to this parameter before calling this function.
 * 			The allocated memory size must be large enough to store the executed file path.
 * 			The result will include the terminating null byte('\0') at the end of the string.
 * @param[in] app_path_size allocated memory size of char *app_path
 * @return 0 on success, -1 if failed. \n
 *         If the size of app_path is smaller than the result, it will return -1 and \n
 *         errno will be set as EOVERFLOW.
 */
int deviced_get_apppath(pid_t pid, char *app_path, size_t app_path_size);

/* sysconf */

/**
 * @fn int deviced_conf_set_mempolicy_bypid(pid_t pid, enum mem_policy mempol)
 * @brief This API is used to set the policy of the given process when the phone has low available memory.
 * @param[in] pid process id which you want to set
 * @param[in] mempol oom adjust value which you want to set
 * @return 0 on success, -1 if failed.
 * @retval -1 operation error
 * @retval -EBADMSG - dbus error (in case of any error on the bus)
 * @retval -EINVAL no mandatory parameters
 * @retval -ESRCH incorrect sender process id
 */
int deviced_conf_set_mempolicy_bypid(pid_t pid, enum mem_policy mempol);

/**
 * @fn int deviced_call_predef_action(const char *type, int num, ...)
 * @brief This API calls predefined systemd action.
 * 		Internally it send message through SYSTEM_NOTI_SOCKET_PATH (/tmp/sn) UNIX socket to systemd.
 * @param[in] type systemd action type name
 * @param[in] num input parameters count. num cannot exceed SYSTEM_NOTI_MAXARG (16).
 * @param[in] ... action input parameters. List of the parameters depends on action type.
 * @return 0 on success, -1 if failed.
 *         If the action type is not set (is blank) or num > SYSTEM_NOTI_MAXARG it will return -1
 *         and errno will be set as EINVAL.
 */
int deviced_call_predef_action(const char *type, int num, ...);

/**
 * @fn int deviced_change_flightmode(int mode)
 * @brief This API call notifies about flight mode.
 * @return 0 or positive value on success and negative value if failed.
 * @retval -1 operation error
 * @retval -EBADMSG dbus error (in case of any error on the bus)
 * @retval -EINVAL no mandatory parameters
 * @retval -ESRCH incorrect sender process id
 */
int deviced_change_flightmode(int mode);

/**
 * @fn int deviced_inform_foregrd(void)
 * @brief This API call notifies that current process is running in foreground.
 * @return 0 or positive value on success and negative value if failed.
 * @retval -1 operation error
 * @retval -EBADMSG dbus error (in case of any error on the bus)
 * @retval -EINVAL no mandatory parameters
 * @retval -ESRCH incorrect sender process id
 */
int deviced_inform_foregrd(void);

/**
 * @fn int deviced_inform_backgrd(void)
 * @brief This API call notifies that current process is running in background.
 * @return 0 or positive value on success and negative value if failed.
 * @retval -1 operation error
 * @retval -EBADMSG dbus error (in case of any error on the bus)
 * @retval -EINVAL no mandatory parameters
 * @retval -ESRCH incorrect sender process id
 */
int deviced_inform_backgrd(void);

/**
 * @fn int deviced_inform_active(pid_t pid)
 * @brief This API call notifies deviced daemon that given process (pid) is active.
 * @param[in] pid process pid
 * @return 0 or positive value on success and negative value if failed.
 * @retval -1 operation error
 * @retval -EBADMSG dbus error (in case of any error on the bus)
 * @retval -EINVAL no mandatory parameters
 * @retval -ESRCH incorrect sender process id
 */
int deviced_inform_active(pid_t pid);

/**
 * @fn int deviced_inform_active(pid_t pid)
 * @brief This API call notifies deviced daemon that given process (pid) is inactive.
 * @param[in] pid process pid
 * @return 0 or positive value on success and negative value if failed.
 * @retval -1 operation error
 * @retval -EBADMSG dbus error (in case of any error on the bus)
 * @retval -EINVAL no mandatory parameters
 * @retval -ESRCH incorrect sender process id
 */
int deviced_inform_inactive(pid_t pid);

/**
 * @fn int deviced_request_poweroff(void)
 * @brief This API call requests deviced daemon to launch "system poweroff popup".
 * @return 0 or positive value on success and negative value if failed.
 * @retval -1 operation error
 * @retval -EBADMSG dbus error (in case of any error on the bus)
 * @retval -EINVAL no mandatory parameters
 * @retval -ESRCH incorrect sender process id
 */
int deviced_request_poweroff(void);

/**
 * @fn deviced_request_entersleep(void)
 * @brief This API call requests deviced daemon to enter system into sleep mode.
 * @return 0 or positive value on success and negative value if failed.
 * @retval -EBADMSG dbus error (in case of any error on the bus)
 * @retval -EINVAL no mandatory parameters
 * @retval -ESRCH incorrect sender process id
 */
int deviced_request_entersleep(void);

/**
 * @fn int deviced_request_leavesleep(void)
 * @brief This API calls requests deviced daemon to leave by system "sleep" mode and enter into "normal" mode.
 * @return 0 or positive value on success and negative value if failed.
 * @retval -EBADMSG dbus error (in case of any error on the bus)
 * @retval -EINVAL no mandatory parameters
 * @retval -ESRCH incorrect sender process id
 */
int deviced_request_leavesleep(void);

/**
 * @fn int deviced_request_reboot(void)
 * @brief This API call requests deviced daemon to initiate system reboot.
 * @return 0 on success and negative value if failed.
 * @retval -EBADMSG dbus error (in case of any error on the bus)
 * @retval -EINVAL no mandatory parameters
 * @retval -ESRCH incorrect sender process id
 */
int deviced_request_reboot(void);

/**
 * @fn int deviced_request_set_cpu_max_frequency(int val)
 * @brief This API call requests deviced daemon to set maximum CPU frequency.
 * @param[in] val maximum CPU frequency to be set.
 * @return 0 or positive value on success and negative value if failed.
 * @retval -1 operation error
 * @retval -EBADMSG - dbus error (in case of any error on the bus)
 */
int deviced_request_set_cpu_max_frequency(int val);

/**
 * @fn int deviced_request_set_cpu_min_frequency(int val)
 * @brief This API call requests deviced daemon to set minimum CPU frequency.
 * @param[in] val minimum CPU frequency to be set
 * @return 0 or positive value on success and negative value if failed.
 * @retval -1 operation error
 * @retval -EBADMSG dbus error (in case of any error on the bus)
 */
int deviced_request_set_cpu_min_frequency(int val);

/**
 * @fn int deviced_release_cpu_max_frequency(void)
 * @brief This API call releases limit of maximum CPU frequency set by deviced_request_set_cpu_max_frequency() API.
 * @return 0 or positive value on success and negative value if failed.
 * @retval -1 operation error
 * @retval -EBADMSG dbus error (in case of any error on the bus)
 */
int deviced_release_cpu_max_frequency(void);

/**
 * @fn int deviced_release_cpu_min_frequency(void)
 * @brief This API calls releases limit of minimum CPU frequency set by deviced_request_set_cpu_min_frequency() API.
 * @return 0 or positive value on success and negative value if failed.
 * @retval -1 operation error
 * @retval -EBADMSG dbus error (in case of any error on the bus)
 */
int deviced_release_cpu_min_frequency(void);

/**
 * @fn int deviced_request_set_factory_mode(int val)
 * @brief This API call requests systemd daemon to enter system into factory mode.
 * @param[in] val factory mode level.\n
 *        0 - "off" factory mode\n
 *        1 - "on" factory mode.
 * @return 0 or positive value on success and negative value if failed.
 * @retval -1 operation error
 * @retval -EBADMSG dbus error (in case of any error on the bus)
 * @retval -EINVAL no mandatory parameters
 * @retval -ESRCH incorrect sender process id
 */
int deviced_request_set_factory_mode(int val);

/**
 * @fn int deviced_request_dump_log(int type)
 * @brief This API call force dumps all application, modem or application + modem logs.
 *   - if type is equal AP_DUMP application log dump will be created.
 *   - if type is equal CP_DUMP modem logs dump  will be created
 *   - if dump type is equal ALL_DUMP application and modem logs dump  will be created
 * @param[in] type dump type.
 * @return 0 or positive value on success and negative value if failed.
 * @retval -1 operation error
 * @retval -EBADMSG dbus error (in case of any error on the bus)
 * @see dump_log_type
 */
int deviced_request_dump_log(int type);

/**
 * @fn int deviced_request_delete_dump(char *ticket)
 * @brief This API call removes all dump data for given ticket from /opt/usr/share/crash/ticket directory.
 * @param[in] ticket ticket name.
 * @return 1 on success and negative value if failed.
 * @retval -1 operation error
 * @retval -EBADMSG dbus error (in case of any error on the bus)
 */
int deviced_request_delete_dump(char *ticket);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif
#endif				/* __DD_DEVICED__ */
