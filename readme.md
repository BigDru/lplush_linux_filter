# Static compilation
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
