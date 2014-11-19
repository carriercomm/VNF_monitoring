#/bin/sh
export BPAPP_HOME=$HOME/BPAPP

test -x /root/BPAPP/bin/bpappVMCT || exit 0

case "$1" in
  start)
    echo " "

    echo "     [VMCT]"
    #./bpappVMCT &
    /root/BPAPP/bin/bpappVMCT &

    echo " "
    ;;

  stop)
    echo " "

    killist=`/bin/ps -ef | egrep "bpappVMCT" | egrep -v egrep | egrep -v vi | egrep -v sh | awk '{print $2}'`
    killist_name=`/bin/ps -ef | egrep "bpappVMCT" | egrep -v egrep | egrep -v vi | egrep -v sh | awk '{print $8}'`
    echo '     ---> KILL-LIST ['$killist_name']'
    ( kill -TERM $killist ) > /dev/null

    sleep 3

    ;;

  *)
    echo "Usage : run_vmct.sh {start|stop}"
    exit 1
esac


exit 0
