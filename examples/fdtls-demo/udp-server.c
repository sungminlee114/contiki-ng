/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"

#include "net/security/tinydtls/tinydtls.h"
#include "net/security/tinydtls/dtls.h"
#include "project-conf.h"
#include "cfs-coffee.h"

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

static struct simple_udp_connection udp_conn;
static uip_ipaddr_t src_ipaddr;


static session_t src_session;
static dtls_context_t *dtls_context;

#define FDTLS_STATE_INIT 0
#define FDTLS_STATE_START 1
#define FDTLS_STATE_HANDSHAKE 2
static int fdtls_state = FDTLS_STATE_INIT;

#define DTLS_ECC 0
#define DTLS_PSK 1

session_t *vir_sess;
#define PSK_DEFAULT_IDENTITY "Client_identity"
char to_send[20][32];
int num_send = 0;

static struct etimer p_timer;

PROCESS(udp_server_process, "UDP server");
AUTOSTART_PROCESSES(&udp_server_process);
/*---------------------------------------------------------------------------*/

// static int
// server_recv(uint8_t *buf) {
//   return simple_udp_recvfrom(&udp_conn, data, len, &src_ipaddr);
// }


static int
server_send_raw(void *data, size_t len) {
  simple_udp_sendto(&udp_conn, data, len, &src_ipaddr);
  return len;
}

static int
server_send(void *data, size_t len) {
  // LOG_INFO("server_send: %s(%lu) to ", (char*) data, len);
  // LOG_INFO_6ADDR(&src_ipaddr);
  // LOG_INFO_("\n");
  
  if (fdtls_state >= FDTLS_STATE_HANDSHAKE) {
    dtls_write(dtls_context, &src_session, (uint8_t *)data, len);
  } else {
    server_send_raw(data, len);
  }
  return len;
}

static int
server_recv_handler(char *recvbuf, uint16_t datalen) {
  if (datalen > 0) {
    if (!strcmp(recvbuf, "sh\0")) {
      // Waiting for client shell input. Do nothing.
    } else if (!strcmp(recvbuf, "ping\0")) {
      // Ping from client.
      server_send("pong\0", 10);
    
    // Check recvbuf starts with "pd "
    } else if (recvbuf[0] == 'p' && recvbuf[1] == 'd' && recvbuf[2] == ' ') {
      // prepare data with specific string
      char *str = recvbuf + 3;
      LOG_INFO("Received prepare data request: %s\n", str);
      cfs_prepare_data(dtls_context,vir_sess, str);

      server_send("done\0", 10);
    } else if (recvbuf[0] == 'q' && recvbuf[1] == 'd'){
      LOG_INFO("Received query data request\n");

      for (int i = 0; i < num_send; i++) {
        server_send(to_send[i], strlen(to_send[i]));
      }
      server_send("done\0", 10);
    } else {
      // Strange input from client.
      LOG_INFO("Which is a strange request\n");
      for (int i = 0; i < datalen; i++) {
        LOG_INFO("%c, %d\n", recvbuf[i], recvbuf[i] == 'p');
      }
    }
  }
  return 0;
}

static int
read_from_peer(struct dtls_context_t *ctx, 
	       session_t *session, uint8_t *data, size_t len) {
  
  char cdata[PAYLOAD];
  snprintf(cdata, len+1, "%s", data);
  // LOG_INFO("read_from_peer: %s(%u)\n", cdata, len);
  server_recv_handler(cdata, len);
  return 0;
}

static int
send_to_peer(struct dtls_context_t *ctx, 
	     session_t *session, uint8_t *data, size_t len) {
  return server_send_raw(data, len);
}

#if DTLS_PSK
/* This function is the "key store" for tinyDTLS. It is called to
 * retrieve a key for the given identity within this particular
 * session. */
static int
get_psk_info(struct dtls_context_t *ctx, const session_t *session,
             dtls_credentials_type_t type,
             const unsigned char *id, size_t id_len,
             unsigned char *result, size_t result_length) {

  struct keymap_t {
    unsigned char *id;
    size_t id_length;
    unsigned char *key;
    size_t key_length;
  } psk[3] = {
    { (unsigned char *)"Client_identity", 15,
      (unsigned char *)"secretPSK", 9 },
    { (unsigned char *)"default identity", 16,
      (unsigned char *)"\x11\x22\x33", 3 },
    { (unsigned char *)"\0", 2,
      (unsigned char *)"", 1 }
  };

  if (type != DTLS_PSK_KEY) {
    return 0;
  }

  if (id) {
    int i;
    for (i = 0; i < sizeof(psk)/sizeof(struct keymap_t); i++) {
      if (id_len == psk[i].id_length && memcmp(id, psk[i].id, id_len) == 0) {
        if (result_length < psk[i].key_length) {
          return dtls_alert_fatal_create(DTLS_ALERT_INTERNAL_ERROR);
        }

        memcpy(result, psk[i].key, psk[i].key_length);
        return psk[i].key_length;
      }
    }
  }
  return dtls_alert_fatal_create(DTLS_ALERT_DECRYPT_ERROR);
}
#endif /* DTLS_PSK */

#if DTLS_ECC
static const unsigned char ecdsa_priv_key[] = {
			0xD9, 0xE2, 0x70, 0x7A, 0x72, 0xDA, 0x6A, 0x05,
			0x04, 0x99, 0x5C, 0x86, 0xED, 0xDB, 0xE3, 0xEF,
			0xC7, 0xF1, 0xCD, 0x74, 0x83, 0x8F, 0x75, 0x70,
			0xC8, 0x07, 0x2D, 0x0A, 0x76, 0x26, 0x1B, 0xD4};

static const unsigned char ecdsa_pub_key_x[] = {
			0xD0, 0x55, 0xEE, 0x14, 0x08, 0x4D, 0x6E, 0x06,
			0x15, 0x59, 0x9D, 0xB5, 0x83, 0x91, 0x3E, 0x4A,
			0x3E, 0x45, 0x26, 0xA2, 0x70, 0x4D, 0x61, 0xF2,
			0x7A, 0x4C, 0xCF, 0xBA, 0x97, 0x58, 0xEF, 0x9A};

static const unsigned char ecdsa_pub_key_y[] = {
			0xB4, 0x18, 0xB6, 0x4A, 0xFE, 0x80, 0x30, 0xDA,
			0x1D, 0xDC, 0xF4, 0xF4, 0x2E, 0x2F, 0x26, 0x31,
			0xD0, 0x43, 0xB1, 0xFB, 0x03, 0xE2, 0x2F, 0x4D,
			0x17, 0xDE, 0x43, 0xF9, 0xF9, 0xAD, 0xEE, 0x70};

static int
get_ecdsa_key(struct dtls_context_t *ctx,
              const session_t *session,
              const dtls_ecdsa_key_t **result) {
  static const dtls_ecdsa_key_t ecdsa_key = {
    .curve = DTLS_ECDH_CURVE_SECP256R1,
    .priv_key = ecdsa_priv_key,
    .pub_key_x = ecdsa_pub_key_x,
    .pub_key_y = ecdsa_pub_key_y
  };

  *result = &ecdsa_key;
  return 0;
}

static int
verify_ecdsa_key(struct dtls_context_t *ctx,
                 const session_t *session,
                 const unsigned char *other_pub_x,
                 const unsigned char *other_pub_y,
                 size_t key_size) {
  return 0;
}
#endif /* DTLS_ECC */

static int
dtls_complete(struct dtls_context_t *ctx, session_t *session, dtls_alert_level_t level, unsigned short code){

  if(code == DTLS_EVENT_CONNECTED) {
    fdtls_state = FDTLS_STATE_HANDSHAKE;
    //struct etimer et;
    // timer_set(&handshake_timer,CLOCK_SECOND*5);

    //buflen = sizeof(buf);
    //dtls_write(ctx, session, (uint8 *)buf, buflen);
    //rtimer_count = rtimer_arch_now();
    //printf("send packet\n");
  }
  return 0;
}

#define FILENAME "fdtls"
cfs_prepare_data(struct dtls_context_t *ctx, session_t *session, char* msg){

  char sendbuf[PAYLOAD];
  int i;

  // cfs_coffee_reserve(FILENAME,4096);
  // int fd = cfs_open(FILENAME,CFS_WRITE);
  
  LOG_INFO("Batching msg by ' ' as delimiter\n");
  msg = strtok(msg, " ");
  for(i=0; ; i++){
    if (msg == NULL) {
      break;
    }
    LOG_INFO("iteration: %d: encrypted %s\n", i, msg);
    // memset(sendbuf, 0, sizeof(sendbuf));
    
    // dtls_encrypt_data(ctx,session,msg,sizeof(msg),sendbuf,sizeof(sendbuf));

    // if(fd >= 0){
    //     int res = cfs_write(fd,sendbuf+21,PAYLOAD-21);

    //     if(res < 0){
    //       printf("maximum size: BUF_SIZE*i = %d\n",PAYLOAD*i);
    //       printf("iteration: %d\n", i);
    //       return -1;
    //     }
    // }

    memset(to_send[i], 0, sizeof(to_send[i]));
    snprintf(to_send[i], sizeof(to_send[i]), "%s", msg);
    num_send = i+1;

    if (i == 0) {
      LOG_INFO("First batch. Writing headers too..\n");
    }

    msg = strtok(NULL, " ");
  }
  // cfs_close(fd);
}


static void
udp_rx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  LOG_INFO("Received response '%.*s'(%d) from ", datalen, (char *) data, datalen);
  LOG_INFO_6ADDR(sender_addr);
  LOG_INFO_("\n");
  // simple_udp_sendto(&udp_conn, data, datalen, sender_addr);

  char recvbuf[PAYLOAD];
  memset(recvbuf, 0, sizeof(recvbuf));
  memcpy(recvbuf, data, datalen);

  if (!strcmp(recvbuf, "syn\0")) {
    // Hello from client.
    src_ipaddr = *sender_addr;
    server_send("ack\0", 10);
    LOG_INFO("Connected to client\n");
  } else if (!strcmp(recvbuf, "fdtls_init\0")) {
    // Initialize DTLS context.
    dtls_init();

    static dtls_handler_t cb = {
      .write = send_to_peer,
      .read  = read_from_peer,
      .event = dtls_complete,
#if DTLS_PSK
        .get_psk_info = get_psk_info,
#endif /* DTLS_PSK */
#if DTLS_ECC
        .get_ecdsa_key = get_ecdsa_key,
        .verify_ecdsa_key = verify_ecdsa_key
#endif /* DTLS_ECC */
    };
    dtls_context = dtls_new_context(&udp_conn);
    if (dtls_context)
        dtls_set_handler(dtls_context, &cb);
    
    LOG_INFO("Creating virtual peer..\n");

    // if(create_virtual_peer(dtls_context,vir_sess, PSK_DEFAULT_IDENTITY ,15) != 0){
    //   LOG_ERR("create virtual peer error\n");
    // }
    
    LOG_INFO("Creating self key block using virtual peer.\n");
    // calculate_key_block_self(dtls_context,vir_sess);
    

    src_session.addr = src_ipaddr;
    fdtls_state = FDTLS_STATE_START;
    LOG_INFO("FDTLS handshake done\n");
    server_send("fdtls_hs_r\0", 12);



  } else if (fdtls_state > FDTLS_STATE_INIT) {
    dtls_handle_message(dtls_context, &src_session, (uint8_t*) recvbuf, datalen);
  } else {
    server_recv_handler(recvbuf, datalen);
  }

  process_poll(&udp_server_process);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_server_process, ev, data)
{
  PROCESS_BEGIN();

  LOG_INFO("UDP server started\n");
  /* Initialize DAG root */
  NETSTACK_ROUTING.root_start();

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT, udp_rx_callback);

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
