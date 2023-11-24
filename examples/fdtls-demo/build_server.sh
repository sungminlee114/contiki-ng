make -j 44 PORT=/dev/ttyUSB0 udp-server.upload && \
# make -j 44 PORT=/dev/ttyUSB0 login
../../tools/serial-io/serialdump -b115200 /dev/ttyUSB0
