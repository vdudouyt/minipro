#
# Regular cron jobs for the minipro package
#
0 4	* * *	root	[ -x /usr/bin/minipro_maintenance ] && /usr/bin/minipro_maintenance
