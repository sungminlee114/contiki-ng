make -j 44 PORT=/dev/ttyUSB0 udp-client.upload WERROR=0 && \
../../tools/serial-io/serialdump -b115200 /dev/ttyUSB0

# make -j 44 PORT=/dev/ttyUSB1 login