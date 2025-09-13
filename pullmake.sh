#!/bin/bash
cd /home/pi/motion
git pull

sudo service motion stop
make -j4
sudo service motion start
