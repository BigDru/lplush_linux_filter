#!/bin/bash
./build.sh
sudo chown root:lp a.out
sudo chmod 755 a.out
sudo cp a.out /usr/lib/cups/filter/dru_filter
sudo systemctl restart cups
