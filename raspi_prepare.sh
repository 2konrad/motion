#!/bin/sh

if [ "$(id -u)" -ne 0 ]; then
        echo 'This script must be run by root' >&2
        exit 1
fi

if true; then

if  [ ! -d "/home/pi/motion" ] ; then
cd ~
git clone https://github.com/2konrad/motion.git
fi

git remote add origin git@github.com/2konrad/motion.git
git remote set-url origin git@github.com/2konrad/motion.git

git config --global user.name "K Meyer"
git config --global user.email "km@web.de"


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
echo "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAACAQDLhr5xe/PWHILgxfYpIkeP8qOc/oIgsKdKclqGkoNvVaOQDhVeNbUqR8LMJ9REjCu82ztw08DQdW2PBhej5IoKrx9yDEny+uCM+tsxrKyN8P/5LcRB2R0rXFWOTnYetc7+7WJ0watsB52gO5q+Zy1pqKWavwbwlxcR/e/HL3y8Z53UW150CCkK8R0h+bAORdVBdpwnfNktc8Qhu7RV0MaE/dmrUmTGWlJeIp4uzOgF7yEkcYa2GhSdSLZNnUiHLKdUNbDPr9jVD+2S0MJw80Jn77h8yQHZgm0gre8IZoGyloLcI1Isy6O1iBaN0tv9UibpZ4y091kQhL6V+ThKVModmz7Lm1jw5bmEn5VTq2s3IYDnyXln5Uoe3G7bMaT2BT94tzp8VFDOB53LFkGY0KPCe9QPjIX+kt1Dd3UQ3tdr58yxeCpkY5h42V9WliUwUUdSZ+PO9djDKJjPQfJSaax5nKikZjVkqoQqMSR2+EqRaGhcyXebF9tbk51bsB4gMq9h/UW+xst/bURlBxs8/m9RkNVfq7LFqACwRZJssp7cxZR3hNlfmuBV1YSxforNuySgYvqtArc36n0hV1Dz1VKn9kf3la1DXGYOpzZD5cCVf758GA3IAzHK1fhZY5XVBdHTBnJadgv0b3T39gyMoXj1tnaHg/hHOw6NM+b5OhzCbQ== konrad@Air-von-Konrad.fritz.box" >> /home/pi/.ssh/authorized_keys


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
        ErrorLog \${APACHE_LOG_DIR}/error.log
        CustomLog \${APACHE_LOG_DIR}/access.log combined
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

fi
