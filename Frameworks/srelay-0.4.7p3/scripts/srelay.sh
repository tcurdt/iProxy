#!/bin/sh
#   startup script for srelay
#   	srelay_enable="Yes" in /etc/rc.conf needed for enable.
#   $Id: srelay.sh,v 1.1 2009/09/02 13:41:40 bulkstream Exp $
#							Tomo.M

# PROVIDE: srelay
# REQUIRE: DAEMON jail
# BEFORE: LOGIN
# KEYWORD: FreeBSD NetBSD shutdown

. /etc/rc.subr

name="srelay"
rcvar=`set_rcvar`
command="/usr/local/sbin/srelay"
pid_file="/var/run/srelay.pid"
start_cmd="start_${name}"
stop_cmd="stop_${name}"

start_srelay ()
{
    #echo -n "Starting ${name}"
    if [ -x ${command} ]; then
        ${command}
    fi
    echo "."
}

stop_srelay ()
{
    #echo -n "Stopping ${name}"
    if [ -f ${pid_file} ]; then
        /bin/kill `cat ${pid_file}`
    fi
    echo "."
}

load_rc_config $name
run_rc_command "$1"
