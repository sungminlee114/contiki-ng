make -j 44 PORT=/dev/ttyUSB1 udp-client.upload WERROR=0 && \
# make -j 44 PORT=/dev/ttyUSB1 login
../../tools/serial-io/serialdump -b115200 /dev/ttyUSB1
