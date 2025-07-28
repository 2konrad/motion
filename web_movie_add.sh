#!/bin/bash
d=`date +%Y%m%d`
t=`date +%H:%M`

cd /home/pi/motion/web
if [ ! -d $d ]
 then mkdir $d
 cd /home/pi/motion/web/$d
 echo "
<html> 
<head>
 
<meta name='viewport' content='width=640' /> 
<style>
	body  {position: relative;}
	div {min-height: 400px; }
	img {position: absolute;height:320px;z-index: 10;}
    .container {
        display: grid;
        grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
        background-color: beige;
        padding: 10px;
    }
</style>
<head>
<body onload='document.body.scrollIntoView(false);'> 
<script type='text/javascript'>
	window.onscroll = scroll;
	function scroll() {
  		var els = document.querySelectorAll('img');
		for (var i=0; i < els.length; i++) {
   			els[i].setAttribute('style', 'display:block');
		}
    }
    function c(id){
        document.getElementById('i'+id).style.display = 'none';
        if (document.getElementById('i'+id).nextElementSibling === null){
            document.getElementById('i'+id).insertAdjacentHTML('afterend',  \`
            <video width=320  controls autoplay> <source src='\${id}.mp4' type=video/mp4 ></video >
            <br><video width=320  controls onerror='hide(this)'> <source src='\${id}m.mp4' type=video/mp4 ></video> 
            \`);
        }
    } 
    function hide(){
        this.style.display = 'none';
        }
</script>
<a href='javascript: document.body.scrollIntoView(false);' 
   style='width:100%;font-size:24pt;margin: 20px auto 20px auto; display:block;'>
   Scroll to bottom
</a><br>
 " > index.htm 
fi

cd /home/pi/motion/web/$d

if [[ "$1" == "lightswitch" ]] ; then 
    echo "<p> $t - &#128161; $3 </p>" >> index.htm
else

    if [ -e /home/pi/motion/web/$d/$1.jpg ]
    then # echo $1 exit >> status.txt
    exit
    fi
    mv /home/pi/motion/output/$1.jpg /home/pi/motion/web/$d/$1.jpg
    mv /home/pi/motion/output/$1m.mp4 /home/pi/motion/web/$d 
    mv /home/pi/motion/output/$1.mp4 /home/pi/motion/web/$d 

    echo "
    <h2 onclick='t(\"d$1\")'> ${1:8:4} $2</h2> 
    <div id='d$1' >
    <img id='i$1' src='$1.jpg' onclick='c(\"$1\")' ></div>
    " >> index.htm
fi

find /home/pi/motion/web/  -maxdepth 1 -mindepth 1 -type d -mtime +14 -exec rm -r {} \;

