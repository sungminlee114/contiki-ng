CONTIKI_PROJECT = udp-client udp-server
all: $(CONTIKI_PROJECT)


MODULES += os/net/security/tinydtls
MODULES += os/net/security/tinydtls/sha2
MODULES += os/net/security/tinydtls/aes
MODULES += os/net/security/tinydtls/ecc
MODULES += os/net/security/tinydtls/contiki-support
# MODULES += os/services/shell
CONTIKI=../..
include $(CONTIKI)/Makefile.include
