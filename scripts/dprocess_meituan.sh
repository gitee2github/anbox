#!/system/bin/sh 

# 日志文件最大为5MB
maxlogsize=5242880

function createLogdir(){
    if [ ! -d "/data/processlog" ];then
        mkdir -p /data/processlog
    fi
}

function rmLog(){
    if [  -e $logdir/meituan.log  ];then
        emptylogsize=`ls -l $logdir/meituan.log | xargs -n1 |sed -n 5p`
        if [ $emptylogsize -gt $maxlogsize ];then
            rm -r $logdir/meituan.log
        fi
    fi
}

function fix_meituan(){
#    echo "test_meituan in ------------------------------------>>>>>>>"
    if [ -f $logdir/testlog_meituan ];then
#        echo "rm -rf testlog_meituan"
        rm -rf $logdir/testlog_meituan
    fi
    if [ -f logcat_meituan ];then
#        echo "rm -rf logcat_meituan"
        rm -rf $logdir/logcat_meituan
    fi
    logcat -c
    logcat >> $logdir/testlog_meituan &
    sleep 10
    cat $logdir/testlog_meituan | grep "System.out: com.meituan.android.pt.homepage.activity.Welcome.onCreate"
    if [ $? = 0 ];then
        date >> $logdir/meituan.log
        echo "am force-stop com.sankuai.meituan  ===========================================" >> $logdir/meituan.log
        am force-stop com.sankuai.meituan
        sleep 3
        echo "am start com.sankuai.meituan  ================================================" >> $logdir/meituan.log
        am start com.sankuai.meituan/com.meituan.android.pt.homepage.activity.Welcome
    else
        date >> $logdir/meituan.log    
        echo "am do nothing  ===============================================================" >> $logdir/meituan.log
    fi


    if [ -f $logdir/testlog_meituan ];then
#       echo "rm -rf testlog_meituan"
        rm -rf $logdir/testlog_meituan
    fi    
    #kill logcat
    ppid=$$
#   echo "ppid:$ppid"
    ps | grep logcat | grep -v "/system/bin/logcat" >> $logdir/logcat_meituan
    if [ $? = 0 ];then
        cat $logdir/logcat_meituan
        process_num=`cat $logdir/logcat_meituan | wc -l`
#       echo "logcat process_num:$process_num"
        for i in $(seq 1 +1 $process_num)
        do
            process_logcat=`cat $logdir/logcat_meituan | sed -n ${i}p | xargs -n1 | sed -n 2p`
#           echo "process_logcat:$process_logcat"
            process_ppid=`cat $logdir/logcat_meituan | sed -n ${i}p | xargs -n1 | sed -n 3p`
#           echo "process_ppid:$process_ppid"
            if [ $ppid = $process_ppid ];then
#              echo "kill $process_logcat"
               kill -9 $process_logcat
            fi        
        done
    fi
    if [ -f $logdir/logcat_meituan ];then
#         echo "rm -rf logcat_meituan"
        rm -rf $logdir/logcat_meituan
    fi

    date
    echo "test_meituan out -----------------------------------<<<<<<<"
    echo ""
    echo ""
    echo ""
    echo ""
    echo ""
}

while(true)
do
    createLogdir
    logdir=/data/processlog
    rmLog
    fix_meituan
done
