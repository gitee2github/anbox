#!/system/bin/sh 

# 日志文件最大为5MB
maxlogsize=5242880

function createLogdir(){
    if [ ! -d "/data/processlog" ];then
        mkdir -p /data/processlog
    fi
}

function rmLog(){
    if [  -e $logdir/toutiao.log ];then
        emptylogsize=`ls -l $logdir/toutiao.log | xargs -n1 |sed -n 5p`
        if [ $emptylogsize -gt $maxlogsize ];then
            rm -r $logdir/toutiao.log
        fi
    fi
}

function fix_toutiao(){
    #echo "test_toutiao in ------------------------------------>>>>>>>"
    date
    if [ -f $logdir/testlog_toutiao ];then
        #echo "rm -rf testlog_toutiao"
        rm -rf $logdir/testlog_toutiao
    fi
    if [ -f $logdir/logcat_toutiao ];then
#        echo "rm -rf logcat_toutiao"
        rm -rf $logdir/logcat_toutiao
    fi
    logcat -c
    logcat >> $logdir/testlog_toutiao &
    sleep 10
    # jinritoutiao
    cat $logdir/testlog_toutiao | grep "com.ss.android.article.news.activity.MainActivity.onCreate"
    if [ $? = 0 ];then
        #echo "com.ss.android.article.news start =============================================="
        cat $logdir/testlog_toutiao | grep "Launch timeout has expired"
        if [ $? = 0 ];then
            date >> $logdir/toutiao.log
            echo "am force-stop com.ss.android.article.news ==============================" >> $logdir/toutiao.log
            am force-stop com.ss.android.article.news
            sleep 3
            echo "am start com.ss.android.article.news ===================================" >> $logdir/toutiao.log
            am start com.ss.android.article.news/com.ss.android.article.news.activity.MainActivity
        else
            sleep 10
            cat $logdir/testlog_toutiao | grep "Launch timeout has expired"
            if [ $? = 0 ];then
                echo "am force-stop com.ss.android.article.news 2==============================" >> $logdir/toutiao.log
                am force-stop com.ss.android.article.news
                sleep 3
                echo "am start com.ss.android.article.news 2===================================" >> $logdir/toutiao.log
                am start com.ss.android.article.news/com.ss.android.article.news.activity.MainActivity
            else
                echo "am do nothing(com.ss.android.article.news)  2====================================" >> $logdir/toutiao.log
            fi
        fi
    else
        date >> $logdir/toutiao.log
        echo "com.ss.android.article.news not start  ==========================================" >> $logdir/toutiao.log
    fi

    if [ -f $logdir/testlog_toutiao ];then
        echo "rm -rf testlog_toutiao"
        rm -rf $logdir/testlog_toutiao
    fi    
    #kill logcat
    ppid=$$
#   echo "ppid:$ppid"
    ps | grep logcat | grep -v "/system/bin/logcat" >> $logdir/logcat_toutiao
    if [ $? = 0 ];then
        cat $logdir/logcat_toutiao
        process_num=`cat $logdir/logcat_toutiao | wc -l`
#       echo "logcat process_num:$process_num"
        for i in $(seq 1 +1 $process_num)
        do
            process_logcat=`cat $logdir/logcat_toutiao | sed -n ${i}p | xargs -n1 | sed -n 2p`
#           echo "process_logcat:$process_logcat"
            process_ppid=`cat $logdir/logcat_toutiao | sed -n ${i}p | xargs -n1 | sed -n 3p`
#           echo "process_ppid:$process_ppid"
            if [ $ppid = $process_ppid ];then
#               echo "kill $process_logcat"
                kill -9 $process_logcat
            fi        
        done
    fi
    if [ -f $logdir/logcat_toutiao ];then
#        echo "rm -rf logcat_toutiao"
        rm -rf $logdir/logcat_toutiao
    fi


    date
    echo "test_toutiao out -----------------------------------<<<<<<<"
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
    fix_toutiao
done
