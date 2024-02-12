# Overview
This project is the result of reverse engineering the L+H2 Mac printer driver and porting it to linux. The driver consists of a .ppd file and an executable that takes a raster print stream from cups and pipes it to the printer using special escape codes every now and then. Reverse engineering was done with IDA and Ghidra. I also modified the ppd file to restrict printer size to specific labels I have. I then bought another printer that look the same (Jadens thermal printer) and tried out the driver. Surprisingly, it worked very well even on the rebranded Jadens printer. 

# Adding the printer on other computers
Surprisingly, adding the printer to Windows was super easy. Linux was a bit more difficult. This is largely because there are a lot of different options which are completely foreign to me (This project was my first handson experience with printer administration in this manner). In the end, using the deprecated raw mode was the best solution. 

# Static compilation for Olimex
docker ps -a | grep dru_filter
docker rm shas for any dru_fitlers in list
docker rmi dru_filter sha

./build_docker_image.sh to build up to date ubuntu docker container for compilation on aarch64 (arm64)
./run_docker_image.sh
cd src
./build_static.sh
exit

scp src/a.out olimex@192.168.xxx.xxx:/home/olimex/dru_filter
ssholimex
./update.sh
lp -d LeftHans2 print_test.png
