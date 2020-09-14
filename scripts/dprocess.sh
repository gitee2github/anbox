#!/system/bin/sh 

:<<!
1.脚本用于判断第三方应用是否存在父进程为1的孤儿进程，存在时将其关闭；
2.判断第三方应用进程与stack是否匹配，当应用stack不存在时，应用线程还存在时，将应用关闭；
3.判断存放日志大小，当日志大于5M时，将存放的日志删除；
!

# 日志文件最大为5MB
maxlogsize=5242880

function createProcessfile(){
    process_name=($(pm list packages -3 | sed 's/package://g'))
} 

function orphanProcess(){
    for process in ${process_name[*]}
    do
        ps | grep $process | grep " 1 "
        if [ $? = 0 ];then
            process_num=`ps | grep $process | grep " 1 " | wc -l`
            for i in $(seq 1 +1 $process_num)
            do
                process_id=`ps | grep $process | grep  " 1 " | sed -n ${i}p | xargs -n1 | sed -n 2p`
                date >> $logdir/orphanprocesslog.txt
                ps | grep $process | grep  " 1 " >> $logdir/orphanprocesslog.txt
                kill -9 $process_id
            done
        fi
    done
}

function createLogdir(){
    if [ ! -d "/data/processlog" ];then
        mkdir -p /data/processlog
        logdir=/data/processlog
    fi
}

function emptyProcess(){
    for process in ${process_name[*]}
    do
        process_stacknum=`am stack list | grep $process | wc -l`
        process_tasknum=`ps | grep $process |wc -l`
        if [ $process_tasknum -ne 0 ] && [ $process_stacknum -eq 0 ];then
            date >> $logdir/emptyprocesslog.txt
            ps | grep $process >> $logdir/emptyprocesslog.txt
            am force-stop $process
        fi
    done
}

function rmLog(){
    if [  -e $logdir/emptyprocesslog.txt ];then
        emptylogsize=`ls -l $logdir/emptyprocesslog.txt | xargs -n1 |sed -n 5p`
        if [ $emptylogsize -gt $maxlogsize ];then
            rm -r $logdir/emptyprocesslog.txt
        fi
    fi
    if [  -e /data/processlog/orphanprocesslog.txt ];then
        orphanlogsize=`ls -l $logdir/orphanprocesslog.txt | xargs -n1 |sed -n 5p`
        if [ $orphanlogsize -gt $maxlogsize ];then
            rm -r $logdir/orphanprocesslog.txt
        fi
    fi
}

while(true)
do
    createProcessfile
    createLogdir
    orphanProcess
    emptyProcess
    rmLog
    sleep 100
done
