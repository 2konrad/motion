#!/bin/bash
sudo service motion stop
make -j4
sudo service motion start
 
