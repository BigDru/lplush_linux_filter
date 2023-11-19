#!/bin/bash
./build.sh
sudo chown root:lp a.out
sudo chmod 755 a.out
sudo cp a.out /usr/lib/cups/filter/dru_filter

# remove logs so it's easier to debug
sudo rm /var/log/cups/*

sudo systemctl restart cups
