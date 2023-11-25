make -j 44 PORT=/dev/ttyUSB1 udp-server.upload WERROR=0 && \
# make -j 44 PORT=/dev/ttyUSB0 login
../../tools/serial-io/serialdump -b115200 /dev/ttyUSB1
