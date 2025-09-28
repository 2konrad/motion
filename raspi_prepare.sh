#!/bin/sh

#git clone https://github.com/2konrad/motion.git && motion/raspi_prepare.sh

if [ "$(id -u)" -ne 1000 ]; then
        echo 'This script must be run by pi' >&2
        exit 1
fi


if  [ ! -d "/home/pi/motion" ] ; then
cd ~
git clone https://github.com/2konrad/motion.git 
fi
sudo service motion stop 

ssh-keygen -t rsa -q -f "/home/pi/.ssh/id_rsa" -N ""
chown pi:pi /home/pi/.ssh/id_rsa
echo "add this to https://github.com/settings/keys"

cat /home/pi/.ssh/id_rsa.pub
cd motion
git remote add origin git@github.com:2konrad/motion.git
git remote set-url origin git@github.com:2konrad/motion.git
git config --global user.name "K Meyer"
git config --global user.email "km@web.de"

### swap
sudo bash -c 'echo "CONF_SWAPSIZE=1024" >> /etc/dphys-swapfile'
sudo service dphys-swapfile restart


##### LED
if [ "$(tail -n 1 /boot/firmware/config.txt )" != "dtparam=pwr_led_trigger=default-on" ] ; then
sudo cat << EOF >> /boot/firmware/config.txt
dtparam=pwr_led_activelow=off
dtparam=pwr_led_trigger=default-on
EOF
echo "config updated"
fi

#### SSH
if [ "$(tail -n 1 /etc/ssh/sshd_config )" != "ClientAliveCountMax 20" ] ; then
sudo sed -i -e 's/PasswordAuthentication no/PasswordAuthentication yes/g' /etc/ssh/sshd_config
sudo sed -i -e 's/KbdInteractiveAuthentication no/KbdInteractiveAuthentication yes/g' /etc/ssh/sshd_config
sudo cat << EOF >> /etc/ssh/sshd_config
KbdInteractiveAuthentication yes
PasswordAuthentication yes
AllowAgentForwarding yes
AllowTcpForwarding yes
ClientAliveInterval 7200
ClientAliveCountMax 20
EOF
echo "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAACAQDLhr5xe/PWHILgxfYpIkeP8qOc/oIgsKdKclqGkoNvVaOQDhVeNbUqR8LMJ9REjCu82ztw08DQdW2PBhej5IoKrx9yDEny+uCM+tsxrKyN8P/5LcRB2R0rXFWOTnYetc7+7WJ0watsB52gO5q+Zy1pqKWavwbwlxcR/e/HL3y8Z53UW150CCkK8R0h+bAORdVBdpwnfNktc8Qhu7RV0MaE/dmrUmTGWlJeIp4uzOgF7yEkcYa2GhSdSLZNnUiHLKdUNbDPr9jVD+2S0MJw80Jn77h8yQHZgm0gre8IZoGyloLcI1Isy6O1iBaN0tv9UibpZ4y091kQhL6V+ThKVModmz7Lm1jw5bmEn5VTq2s3IYDnyXln5Uoe3G7bMaT2BT94tzp8VFDOB53LFkGY0KPCe9QPjIX+kt1Dd3UQ3tdr58yxeCpkY5h42V9WliUwUUdSZ+PO9djDKJjPQfJSaax5nKikZjVkqoQqMSR2+EqRaGhcyXebF9tbk51bsB4gMq9h/UW+xst/bURlBxs8/m9RkNVfq7LFqACwRZJssp7cxZR3hNlfmuBV1YSxforNuySgYvqtArc36n0hV1Dz1VKn9kf3la1DXGYOpzZD5cCVf758GA3IAzHK1fhZY5XVBdHTBnJadgv0b3T39gyMoXj1tnaHg/hHOw6NM+b5OhzCbQ== konrad@Air-von-Konrad.fritz.box" >> /home/pi/.ssh/authorized_keys
echo "ssh updated"
fi

##install
sudo apt purge -y linux-image-rpi-2712
sudo apt purge -y linux-image-*+rpt-rpi-2712
sudo apt purge -y linux-headers-*+rpt-rpi-2712
sudo apt purge chromium firefox cups -y
sudo apt autoremove -y
sudo apt update && apt upgrade -y    
sudo apt install -y autoconf automake autopoint build-essential pkgconf libtool libzip-dev libjpeg-dev git libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libavdevice-dev 
sudo apt install -y libopencv-dev libwebp-dev gettext libmicrohttpd-dev libmariadb-dev libcamera-dev libcamera-tools libcamera-v4l2 libasound2-dev libpulse-dev libfftw3-dev
sudo apt install -y apache2
sudo apt autoremove -y

#sudo apt install -y realvnc-vnc-server realvnc-vnc-viewer
sudo apt install -y wayvnc
sudo systemctl enable wayvnc.service


##### apache2.conf
if [ "$(tail -n 1 /etc/apache2/envvars )" != "export APACHE_RUN_GROUP=pi" ] ; then
sudo cat << EOF >> /etc/apache2/envvars
export APACHE_RUN_USER=pi
export APACHE_RUN_GROUP=pi
EOF
sudo rm /etc/apache2/sites-enabled/000-default.conf
sudo cp /home/pi/motion/conf/localweb.conf /etc/apache2/sites-abailable/
sudo a2siteenable localweb.conf

mkdir /home/pi/motion/web
cd /home/pi/motion/data
cp .header.htm ../web
cp .htaccess ../web
echo "<pre> $(hostname) </pre>" >> ../web/.header.htm
echo "apache updted"
fi

#### log
cp /home/pi/motion/conf/motion.logrotate /etc/logrotate.d/


cd /home/pi/motion
autoreconf -fiv
./configure
make -j4

cp /home/pi/motion/conf/motion.service /etc/systemd/system/motion.service 
sudo systemctl daemon-reload
sudo systemctl enable motion


cat /home/pi/.ssh/id_rsa.pub
echo "add this to https://github.com/settings/keys"

echo "Press enter to reboot"
read _

sudo reboot


