#--------------------------------------
#   power-manager
#--------------------------------------
/usr/bin/devicectl display savelog
PM_DEBUG=$1/power-manager
PM_KMG=sleep_wakeup.log
PM_KMG_SLEEP=/sys/kernel/debug/sleep_history
PM_KMG_WAKEUP=/sys/kernel/debug/wakeup_sources
/bin/mkdir -p ${PM_DEBUG}
/bin/cp -rf /opt/var/log/pm_state.log ${PM_DEBUG}

if [ -e $PM_KMG_SLEEP ];
then
/bin/cat ${PM_KMG_SLEEP} > ${PM_DEBUG}/${PM_KMG}
fi

if [ -e $PM_KMG_WAKEUP ];
then
/bin/cat ${PM_KMG_WAKEUP} >> ${PM_DEBUG}/${PM_KMG}
fi

