#!/bin/sh

if [ "$(id -u)" -ne 0 ]; then
        echo 'This script must be run by root' >&2
        exit 1
fi

##### LED
cat << EOF >> /boot/firmware/config.txt
dtparam=pwr_led_activelow=off
dtparam=pwr_led_trigger=default-on
EOF

#### SSH
cat << EOF >> /etc/ssh/sshd_config
KbdInteractiveAuthentication yes
PasswordAuthentication yes
AllowAgentForwarding yes
AllowTcpForwarding yes
EOF

##install
sudo apt install -y autoconf automake autopoint build-essential pkgconf libtool libzip-dev libjpeg-dev git libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libavdevice-dev 
sudo apt install -y libopencv-dev libwebp-dev gettext libmicrohttpd-dev libmariadb-dev libcamera-dev libcamera-tools libcamera-v4l2 libasound2-dev libpulse-dev libfftw3-dev
sudo apt install -y apache2


##### apache2.conf
cat << EOF >> /etc/aoache2/apache2.conf
ServerName localhost
<Directory /home/pi/motion/web>
        Options Indexes FollowSymLinks
        AllowOverride All
        Require all granted
</Directory>
<VirtualHost *:80>
        ServerAdmin webmaster@localhost
        DocumentRoot /home/pi/motion/web
        ErrorLog ${APACHE_LOG_DIR}/error.log
        CustomLog ${APACHE_LOG_DIR}/access.log combined
</VirtualHost>
LoadModule rewrite_module /usr/lib/apache2/modules/mod_rewrite.so
EOF
cat << EOF >> /etc/apache2/envvars
export APACHE_RUN_USER=pi
export APACHE_RUN_GROUP=pi
EOF


#### log
cat << EOF > /etc/logrotate.d/motion.conf
/home/pi/motion/log/lotion.log {
        rotate 14
        daily
        nocompress
}

EOF

sudo cp /home/pi/motion/conf/motion.service /etc/systemd/motion.service 
sudo systemctl enable motion

cd /home/pi/motion
autoreconf -fiv
./configure
make


sudo reboot
