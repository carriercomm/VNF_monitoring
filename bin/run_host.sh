#/bin/sh
export BPAPP_HOME=$HOME/BPAPP

test -x /root/BPAPP/bin/bpappHOST || exit 0

case "$1" in
  start)
    echo " "

    echo "     [HOST]"
    /root/BPAPP/bin/bpappHOST &

    echo " "
    ;;

  stop)
    echo " "

    killist=`/bin/ps -ef | egrep "bpappHOST" | egrep -v egrep | egrep -v vi | egrep -v sh | awk '{print $2}'`
    killist_name=`/bin/ps -ef | egrep "bpappHOST" | egrep -v egrep | egrep -v vi | egrep -v sh | awk '{print $8}'`
    echo '     ---> KILL-LIST ['$killist_name']'
    ( kill -TERM $killist ) > /dev/null

    sleep 3

    ;;

  *)
    echo "Usage : run_host.sh {start|stop}"
    exit 1
esac


exit 0
