#include "contiki.h"
#include "net/routing/routing.h"
#include "random.h"
#include "net/netstack.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ipv6/simple-udp.h"
#include "dev/serial-line.h"

#include "net/security/tinydtls/tinydtls.h"
#include "shell.h"
#include "project-conf.h"

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

#define SEND_INTERVAL		  (5 * CLOCK_SECOND)

static struct simple_udp_connection udp_conn;
uip_ipaddr_t dest_ipaddr;
static struct process *curr_network_process;
static struct pt cmd_handler_pt;

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client");
AUTOSTART_PROCESSES(&udp_client_process);
/*---------------------------------------------------------------------------*/

char crecvbuf[PAYLOAD];
int crecvbuf_len = 0;

static session_t dst_session;
static dtls_context_t *dtls_context;

#define FDTLS_STATE_NONE 0 // Nothing ready
#define FDTLS_STATE_UDPCONN 1 // ONLY UDP CONNECTION READY
#define FDTLS_STATE_INIT 2 // Ready to init fdtls
#define FDTLS_STATE_START 3 // Ready to start fdtls handshake
#define FDTLS_STATE_HANDSHAKE 4 // Handshake done

static int fdtls_state = FDTLS_STATE_NONE;

#define DTLS_ECC 0
#define DTLS_PSK 1

static int
client_recv_handler(char* recvbuf, uint16_t datalen){
  memset(crecvbuf, 0, sizeof(crecvbuf));
  memcpy(crecvbuf, recvbuf, datalen);
  crecvbuf_len = datalen;

  LOG_INFO("Client recv handler: %s(%d)\n", recvbuf, crecvbuf_len);
  LOG_INFO("curr_network_process: %p\n", curr_network_process);
  if (curr_network_process != NULL){
    process_poll(curr_network_process);
  }
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

  if (datalen > 0) {
    if (fdtls_state > FDTLS_STATE_INIT) {
      dtls_handle_message(dtls_context, &dst_session, (uint8_t*) data, datalen);
      if (fdtls_state < FDTLS_STATE_HANDSHAKE) {
        crecvbuf_len = 0;
      }
    } else {
      client_recv_handler((char *) data, datalen);
    }
  } else {
    // LOG_INFO_(" (empty)\n");
  }
}

static int
client_recv(char *buf)
{
  if (crecvbuf_len > 0) {
    memcpy(buf, crecvbuf, crecvbuf_len);
    int ret = crecvbuf_len;
    crecvbuf_len = 0;
    return ret;
  } else {
    return 0;
  }
}

static int
client_send_raw(const char *buf, int len)
{
  simple_udp_sendto(&udp_conn, buf, len, &dest_ipaddr);
  return len;
}

static int
client_send(const char *buf, int len)
{
  if (fdtls_state >= FDTLS_STATE_HANDSHAKE) {
    return dtls_write(dtls_context, &dst_session, (uint8_t*) buf, len);
  } else {
    return client_send_raw(buf, len);
  }
}

static void
shell_printf(const char *str)
{
  printf("%s", str);
}

/* -------------------- cmd functions -------------------- */

static
PT_THREAD(cmd_help(struct pt *pt, char* args))
{
  PT_BEGIN(pt);
  SHELL_OUTPUT("Available commands:\n");
  for(int i = 0; builtin_shell_commands[i].name != NULL; i++) {
    SHELL_OUTPUT("  %-10s %s\n", builtin_shell_commands[i].name, builtin_shell_commands[i].help);
  }
  PT_END(pt);
}

static
PT_THREAD(cmd_ping(struct pt *pt, char* args))
{
  // static char sendbuf[PAYLOAD];
  PT_BEGIN(pt);

  static char recvbuf[PAYLOAD];

  curr_network_process = PROCESS_CURRENT();
  client_send("ping\0", 10);
  
  PT_WAIT_UNTIL(pt, crecvbuf_len > 0);
  curr_network_process = NULL;

  int len = client_recv(recvbuf);
  if (len > 0) {
    SHELL_OUTPUT("Received response '%.*s'\n", len, recvbuf);
  }
  PT_END(pt);
}

static
PT_THREAD(cmd_reboot(struct pt *pt, char* args))
{
  
  PT_BEGIN(pt);

  SHELL_OUTPUT("Rebooting...\n");
  watchdog_reboot();

  PT_END(pt);
}

/* --------------- FDTLS ----------------- */

static int
read_from_peer(struct dtls_context_t *ctx, 
	       session_t *session, uint8_t *data, size_t len) {
  
  LOG_INFO("read_from_peer: %s(%llu)\n", (char*) data, len);
  client_recv_handler((char *) data, len);
  return 0;
}

static int
send_to_peer(struct dtls_context_t *ctx, 
	     session_t *session, uint8_t *data, size_t len) {
  // LOG_INFO("send_to_peer(");
  // LOG_INFO_6ADDR(&(session->addr));
  // LOG_INFO_("): %s(%u)\n", (char*) data, len);

  return client_send_raw((char *) data, len);
}

// static int
// dtls_handle_read(struct dtls_context_t *ctx) {
  
//   static uint8_t buf[RX_BUF_SIZE];
//   int len;

//   printf("dtls_handle_read\n");
//   if(uip_newdata()) {
//     printf("uip_newdata\n");
//     uip_ipaddr_copy(&session.addr, &UIP_IP_BUF->srcipaddr);
//     session.port = UIP_UDP_BUF->srcport;

//     len = uip_datalen();

//     if (len > sizeof(buf)) {
//       // dtls_warn("packet is too large");
//       return dtls_alert_fatal_create(DTLS_ALERT_INTERNAL_ERROR);
//     }

//     memcpy(buf, uip_appdata, len);
//     printf("buf: %s\n", buf);
//   }
//   return 
// }

static int
dtls_complete(struct dtls_context_t *ctx, session_t *session, dtls_alert_level_t level, unsigned short code){

  LOG_INFO("dtls_complete: alert %d, code %d in state %d\n", level, code, fdtls_state);
  if(code == DTLS_EVENT_CONNECTED) {
    // handshake_complete = 1;
    fdtls_state = FDTLS_STATE_HANDSHAKE;
    process_poll(PROCESS_CURRENT());
    //struct etimer et;
    // etimer_set(&handshake_timer,CLOCK_SECOND*5);

    //buflen = sizeof(buf);
    //dtls_write(ctx, session, (uint8 *)buf, buflen);
    //rtimer_count = rtimer_arch_now();
    //printf("send packet\n");
  }
  return 0;
}

#if DTLS_PSK
#define PSK_ID_MAXLEN 32
#define PSK_MAXLEN 32
#define PSK_DEFAULT_IDENTITY "Client_identity"
#define PSK_DEFAULT_KEY      "secretPSK"

static unsigned char psk_id[PSK_ID_MAXLEN] = PSK_DEFAULT_IDENTITY;
static size_t psk_id_length = sizeof(PSK_DEFAULT_IDENTITY) - 1;
static unsigned char psk_key[PSK_MAXLEN] = PSK_DEFAULT_KEY;
static size_t psk_key_length = sizeof(PSK_DEFAULT_KEY) - 1;

#ifdef __GNUC__
#define UNUSED_PARAM __attribute__((unused))
#else
#define UNUSED_PARAM
#endif /* __GNUC__ */

/* This function is the "key store" for tinyDTLS. It is called to
 * retrieve a key for the given identity within this particular
 * session. */
static int
get_psk_info(struct dtls_context_t *ctx UNUSED_PARAM,
            const session_t *session UNUSED_PARAM,
            dtls_credentials_type_t type,
            const unsigned char *id, size_t id_len,
            unsigned char *result, size_t result_length) {

  switch (type) {
  case DTLS_PSK_IDENTITY:
    if (result_length < psk_id_length) {
      LOG_INFO("cannot set psk_identity -- buffer too small\n");
      return dtls_alert_fatal_create(DTLS_ALERT_INTERNAL_ERROR);
    }

    memcpy(result, psk_id, psk_id_length);
    return psk_id_length;
  case DTLS_PSK_KEY:
    if (id_len != psk_id_length || memcmp(psk_id, id, id_len) != 0) {
      LOG_INFO("PSK for unknown id requested, exiting\n");
      return dtls_alert_fatal_create(DTLS_ALERT_ILLEGAL_PARAMETER);
    } else if (result_length < psk_key_length) {
      LOG_INFO("cannot set psk -- buffer too small\n");
      return dtls_alert_fatal_create(DTLS_ALERT_INTERNAL_ERROR);
    }

    memcpy(result, psk_key, psk_key_length);
    return psk_key_length;
  default:
    LOG_INFO("unsupported request type: %d\n", type);
  }
  return dtls_alert_fatal_create(DTLS_ALERT_INTERNAL_ERROR);
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


static
PT_THREAD(cmd_init_fdtls(struct pt *pt, char* args))
{
  static char recvbuf[PAYLOAD];
  
  PT_BEGIN(pt);

  if(fdtls_state >= FDTLS_STATE_HANDSHAKE){
    LOG_INFO("FDTLS already in handshake state\n");
  } else {

    LOG_INFO("Init FDTLS\n");
    fdtls_state = FDTLS_STATE_INIT;
    curr_network_process = PROCESS_CURRENT();

    
    dtls_init();
    dst_session.addr = dest_ipaddr;
    dst_session.port = UDP_SERVER_PORT;

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
    else {
      printf("cannot create context\n");
      PT_EXIT(pt);
    }

    client_send("fdtls_init\0", 12);

    LOG_INFO("Waiting for server FDTLS handshake ready\n");
    while (1)
    {
      memset(recvbuf, 0, sizeof(recvbuf));
      curr_network_process = PROCESS_CURRENT();
      PT_WAIT_UNTIL(pt, crecvbuf_len > 0);
      int len = client_recv(recvbuf);

      if (len > 0 && !strcmp(recvbuf, "fdtls_hs_r\0")) {
        curr_network_process = NULL;
        LOG_INFO("fdtls_handshake_ready\n");
        break;
      }
    }

    LOG_INFO("Start FDTLS handshake\n");
    fdtls_state = FDTLS_STATE_START;
    dtls_connect(dtls_context, &dst_session);

    // curr_network_process = PROCESS_CURRENT();
    PT_WAIT_UNTIL(pt, fdtls_state == FDTLS_STATE_HANDSHAKE);
    LOG_INFO("FDTLS handshake done\n");
  }
  PT_END(pt);
}


/* -------------------- cmd functions end -------------------- */

struct shell_command_t builtin_shell_commands[] = {
  { "help",           cmd_help, "'> help': Shows this help" },
  { "reboot",         cmd_reboot, "'> help': Reboot the node" },
  { "ping",           cmd_ping, "'> help': Ping the server" },
  { "init_fdtls",     cmd_init_fdtls, "'> help': init_fdtls"},
  
  // { "fdtls",                cmd_fdtls,                "'> help': cmd_fdtls"},
  { NULL, NULL, NULL }
};

struct shell_command_t*
handle_shell_input(const char *cmd)
{
  static char *args;

  /* Shave off any leading spaces. */
  while(*cmd == ' ') {
    cmd++;
  }

  /* Ignore empty lines */
  if(*cmd == '\0') {
    return NULL;
  }

  args = strchr(cmd, ' ');
  if(args != NULL) {
    *args = '\0';
    args++;
  }

  for(int i = 0; builtin_shell_commands[i].name != NULL; i++) {
    if (strcmp(builtin_shell_commands[i].name, cmd) == 0) {
      return &builtin_shell_commands[i];
    }
  }

  return NULL;
}

// int waiting_for_shell_input = 0;

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
  static struct etimer periodic_timer;
  static char sendbuf[PAYLOAD];
  static char recvbuf[PAYLOAD];
  static struct shell_command_t *cmd_descr = NULL;

  PROCESS_BEGIN();

  // // dest_ipaddr to my global ipv6 address
  // uip_ip6addr_copy(&dest_ipaddr, &uip_ds6_if.addr_list[1].ipaddr);

  // LOG_INFO("Destination address: ");
  // LOG_INFO_6ADDR(&dest_ipaddr);
  // LOG_INFO_("\n");

  LOG_INFO("Current process: %s\n", PROCESS_CURRENT()->name);

  /* Initialize UDP connection */
  if (!simple_udp_register(&udp_conn, UDP_CLIENT_PORT, &dest_ipaddr, UDP_SERVER_PORT, udp_rx_callback))
	{
		LOG_ERR("simple_udp_register error\n");
    goto exit;
	}

  while(1) {

    // if(waiting_for_shell_input){
    if(fdtls_state >= FDTLS_STATE_UDPCONN){
      snprintf(sendbuf, sizeof(sendbuf), "sh");
      simple_udp_send(&udp_conn, sendbuf, strlen(sendbuf));

      SHELL_OUTPUT("> ");
      PROCESS_WAIT_EVENT_UNTIL(ev == serial_line_event_message && data != NULL);

      cmd_descr = handle_shell_input(data);

      if (cmd_descr != NULL && cmd_descr->func != NULL) {
        PROCESS_PT_SPAWN(&cmd_handler_pt, cmd_descr->func(&cmd_handler_pt, NULL));

      } else {
        SHELL_OUTPUT("Command not found. Type 'help' for a list of commands\n");
      }
    }  else {
      if(NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
        LOG_INFO("Root is reachable now\n");
        LOG_INFO("Address: ");
        LOG_INFO_6ADDR(&dest_ipaddr);
        LOG_INFO_("\n");

        curr_network_process = PROCESS_CURRENT();
        client_send("syn\0", 10);
        PROCESS_WAIT_EVENT_UNTIL(crecvbuf_len > 0);
        curr_network_process = NULL;

        int len = client_recv(recvbuf);
        if (len > 0 && !strcmp(recvbuf, "ack\0")) {
          LOG_INFO("Connected to server\n");
          fdtls_state = FDTLS_STATE_UDPCONN;
        } else {
          goto exit;
        }
      } else {
        LOG_INFO("Not reachable yet\n");
        etimer_set(&periodic_timer, 5 * CLOCK_SECOND);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
      }
    }
  }

exit:
  dtls_free_context(dtls_context);
;

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
