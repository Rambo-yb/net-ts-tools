#!/bin/sh

BASE_PATH=`pwd`

export PATH="/home/smb/RV1126_RV1109_LINUX_SDK_V2.2.5.1_20230530/prebuilts/gcc/linux-x86/arm/gcc-arm-8.3-2019.03-x86_64-arm-linux-gnueabihf/bin/:$PATH"
HOST=arm-linux-gnueabihf

CC=$HOST-gcc

$CC serial/serial_transparent.c serial/serial.c tcp/tcp_server.c -o serial_ts -I$BASE_PATH/serial -I$BASE_PATH/tcp -lpthread
$CC i2c/i2c_transport.c i2c/i2c.c tcp/tcp_server.c -o i2c_ts -I$BASE_PATH/i2c -I$BASE_PATH/tcp -lpthread
