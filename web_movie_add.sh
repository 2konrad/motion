#!/bin/bash
d=`date +%Y%m%d`
t=`date +%H:%M`

cd /home/pi/motion/web/ 
if [ ! -d $d ]
 then 
 mkdir $d
 cp /home/pi/motion/web_movie_index.htm /home/pi/motion/web/$d/index.htm
### fi

cd /home/pi/motion/web/$d

if [[ "$1" == "lightswitch" ]] ; then 
    echo "<p> $t - &#128161; $3 </p>" >> index.htm
else
    if [ -e /home/pi/motion/web/$d/$1.jpg ]
    then # echo $1 exit >> status.txt
    exit
    fi
    if [[ "$2" == "PERSON" ]] ; then
        class="personcard"
    else
        class="card"
    fi
    mv /home/pi/motion/output/$1.jpg /home/pi/motion/web/$d/$1.jpg || exit # if jpg not there anymore leave here
    mv /home/pi/motion/output/$1m.mp4 /home/pi/motion/web/$d/
    mv /home/pi/motion/output/$1.mp4 /home/pi/motion/web/$d/

    echo "
    <div class='$class' id='$1'>
    <h2> ${1:8:4} $2</h2> 
    <img src='$1.jpg' onclick='changeSource(\"$1\")' >
    </div>
    " >> index.htm
fi
fi


minfree_gb=10     # min 10 GB free
minfree=$(( 1000000*minfree_gb ))
# echo "Min free: $minfree"
avail=$(df | grep 0p2 | awk '{print $4}')
# echo "Available: $avail"
cd /home/pi/motion/web
oldestdir=$(ls -ltcd */ --time=birth | tail -1 | awk '{print $9}')

while [[ $avail -lt $minfree ]] ; do 
    echo "delete mp4 in $oldestdir"
    rm $oldestdir/*.mp4 
    break
done 

find /home/pi/motion/web/  -maxdepth 1 -mindepth 1 -type d -mtime +33 -exec rm -r {} \;

