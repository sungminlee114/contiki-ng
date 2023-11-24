#include "contiki.h"
#include "net/routing/routing.h"
#include "random.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "dev/serial-line.h"

#include "shell.h"

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

#define SEND_INTERVAL		  (5 * CLOCK_SECOND)

static struct simple_udp_connection udp_conn;

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client");
AUTOSTART_PROCESSES(&udp_client_process);
/*---------------------------------------------------------------------------*/

char crecvbuf[32];
int crecvbuf_len = 0;

static void
udp_rx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{

  // LOG_INFO("Received response '%.*s' from ", datalen, (char *) data);
  // LOG_INFO_6ADDR(sender_addr);
  // LOG_INFO_("\n");

  if (datalen > 0) {
    memset(crecvbuf, 0, sizeof(crecvbuf));
    memcpy(crecvbuf, data, datalen);
    crecvbuf_len = datalen;
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
  static char sendbuf[32];

  PT_BEGIN(pt);

  snprintf(sendbuf, sizeof(sendbuf), "ping");
  simple_udp_send(&udp_conn, sendbuf, strlen(sendbuf));

  static char recvbuf[32];
  PT_WAIT_UNTIL(pt, crecvbuf_len > 0);
  int len = client_recv(recvbuf);
  if (len > 0) {
    SHELL_OUTPUT("Received response '%.*s'\n", len, recvbuf);
  }
  PT_END(pt);
}

/* -------------------- cmd functions end -------------------- */

struct shell_command_t builtin_shell_commands[] = {
  { "help",                 cmd_help,                 "'> help': Shows this help" },
  { "ping",                 cmd_ping,                 "'> help': Ping the server" },
  // { "prepare_data",         cmd_prepare_data,         "'> help': cmd_prepare_data"},
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

int waiting_for_shell_input = 0;
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
  static struct etimer periodic_timer;
  static char sendbuf[32];
  uip_ipaddr_t dest_ipaddr;


  PROCESS_BEGIN();

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, udp_rx_callback);

  while(1) {
    if(waiting_for_shell_input){
      snprintf(sendbuf, sizeof(sendbuf), "sh");
      simple_udp_sendto(&udp_conn, sendbuf, strlen(sendbuf), &dest_ipaddr);

      SHELL_OUTPUT("> ");
      PROCESS_WAIT_EVENT_UNTIL(ev == serial_line_event_message && data != NULL);

      struct shell_command_t *cmd_descr = handle_shell_input(data);

      if (cmd_descr != NULL && cmd_descr->func != NULL) {
        static struct pt cmd_handler_pt;
        PROCESS_PT_SPAWN(&cmd_handler_pt, cmd_descr->func(&cmd_handler_pt, NULL));
        LOG_INFO("DBG 2\n");
        // PT_WAIT_THREAD(&cmd_handler_pt);
      } else {
        SHELL_OUTPUT("Command not found. Type 'help' for a list of commands\n");
      }

    } else {
      if(NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
        LOG_INFO("Root is reachable now\n");
        LOG_INFO("Address: ");
        LOG_INFO_6ADDR(&dest_ipaddr);
        LOG_INFO_("\n");

        waiting_for_shell_input = 1;
      } else {
        LOG_INFO("Not reachable yet\n");
        etimer_set(&periodic_timer, 5 * CLOCK_SECOND);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
      }
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
