#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <rte_common.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_eventdev.h>
#include <rte_bus_vdev.h>
#include <rte_cycles.h>


#include <rte_event_eth_rx_adapter.h>

#include "bzpp.h"
#include "bzpp_log.h"

#include "libcli.h"

#define BZPP_CLI_PORT 12121
#define BZPP_CLI_BANNER "Welcome to BZPP"
#define BZPP_CLI_PROMPT "bzpp"


static struct
{
	uint64_t rx_pkts;
	uint64_t rx_bytes;
	uint64_t rx_missed;
	uint64_t tx_pkts;
	uint64_t tx_bytes;
	uint64_t time;
}last_port_stats[RTE_MAX_ETHPORTS];

static struct
{
	uint64_t rx_pkts;
	uint64_t tx_pkts;
	uint64_t tx_fails;
	uint64_t rx_timeouts;
	uint64_t time;
}last_lcore_stats[RTE_MAX_LCORE];



struct bzpp_port_stats
{
	uint64_t rx_pkts;
	uint64_t rx_pkt_rate;
	uint64_t rx_bytes;
	uint64_t rx_bit_rate;
	uint64_t rx_misses;
	uint64_t rx_miss_rate;
	uint64_t rx_errors;
	uint64_t rx_error_rate;
	uint64_t rx_nobuf;
	uint64_t tx_pkts;
	uint64_t tx_pkts_rate;
	uint64_t tx_bytes;
	uint64_t tx_bit_rate;
	uint64_t tx_errors;
	uint64_t tx_error_rate;
};

struct bzpp_lcore_stats
{
	uint64_t rx_pkts;
	uint64_t rx_pkt_rate;
	uint64_t rx_timeouts;
	uint64_t rx_to_rate;
	uint64_t tx_pkts;
	uint64_t tx_pkt_rate;
	uint64_t tx_fails;
	uint64_t tx_fail_rate;
};




static int bzpp_cli_timeout(struct cli_def *cli)
{
	cli_print(cli, "Byebye!");
	return CLI_QUIT;
}

static void _bzpp_cli_get_lcore_stats(uint8_t core, struct bzpp_lcore_stats *s)
{
	uint64_t cycles, diff_cycles;
	extern struct bzpp_lcore_data pp_lcore_data[];
	memset(s, 0, sizeof(*s));

	cycles = rte_get_timer_cycles();
	
	s->rx_pkts = pp_lcore_data[core].rx_pkts;
	s->rx_timeouts = pp_lcore_data[core].rx_timeouts;
	s->tx_pkts = pp_lcore_data[core].tx_pkts;
	s->tx_fails =  pp_lcore_data[core].tx_fails;

	if(last_lcore_stats[core].time > 0)
	{
		diff_cycles = cycles - last_lcore_stats[core].time;
		
		if(diff_cycles > 0)
		{	
			if(last_lcore_stats[core].rx_pkts > 0)
				s->rx_pkt_rate = (s->rx_pkts - last_lcore_stats[core].rx_pkts)*rte_get_timer_hz()/diff_cycles;
			if(last_lcore_stats[core].tx_pkts > 0)
				s->tx_pkt_rate = ((s->tx_pkts - last_lcore_stats[core].tx_pkts)*rte_get_timer_hz()/diff_cycles);
			if(last_lcore_stats[core].rx_timeouts > 0)
				s->rx_to_rate = (s->rx_timeouts - last_lcore_stats[core].rx_timeouts)*rte_get_timer_hz()/diff_cycles;
			if(last_lcore_stats[core].tx_fails > 0)
				s->tx_fail_rate = (s->tx_fails - last_lcore_stats[core].tx_fails)*rte_get_timer_hz()/diff_cycles;
		}
	}

	last_lcore_stats[core].rx_pkts = s->rx_pkts;
	last_lcore_stats[core].rx_timeouts = s->rx_timeouts;
	last_lcore_stats[core].tx_pkts = s->tx_pkts;
	last_lcore_stats[core].tx_fails = s->tx_fails;
	last_lcore_stats[core].time = cycles;
	
}

static void _bzpp_cli_get_port_stats(uint8_t portid, struct bzpp_port_stats *s)
{
	uint64_t cycles, diff_cycles;
	struct rte_eth_stats stats;

	memset(s, 0, sizeof(*s));

	cycles = rte_get_timer_cycles();
	rte_eth_stats_get(portid, &stats);

	s->rx_pkts = stats.ipackets;
	s->rx_bytes = stats.ibytes;
	s->rx_misses = stats.imissed;
	s->rx_errors = stats.ierrors;
	s->rx_nobuf = stats.rx_nombuf;
	s->tx_pkts = stats.opackets;
	s->tx_bytes = stats.obytes;
	s->tx_errors = stats.oerrors;
	

	if(last_port_stats[portid].time > 0)
	{
		diff_cycles = cycles - last_port_stats[portid].time;
		if(diff_cycles > 0)
		{
			if(last_port_stats[portid].rx_pkts > 0)
				s->rx_pkt_rate = (s->rx_pkts - last_port_stats[portid].rx_pkts)*rte_get_timer_hz()/diff_cycles;
			if(last_port_stats[portid].rx_bytes > 0)
				s->rx_bit_rate = ((s->rx_bytes - last_port_stats[portid].rx_bytes)*rte_get_timer_hz()/diff_cycles)*8;
			if(last_port_stats[portid].rx_missed > 0)
				s->rx_miss_rate = (s->rx_misses - last_port_stats[portid].rx_missed)*rte_get_timer_hz()/diff_cycles;
			if(last_port_stats[portid].tx_pkts > 0)
				s->tx_pkts_rate = (s->rx_pkts - last_port_stats[portid].tx_pkts)*rte_get_timer_hz()/diff_cycles;
			if(last_port_stats[portid].tx_bytes > 0)
				s->tx_bit_rate = ((s->tx_bytes - last_port_stats[portid].tx_bytes)*rte_get_timer_hz()/diff_cycles)*8;
		}
	}

	last_port_stats[portid].rx_pkts = s->rx_pkts;
	last_port_stats[portid].rx_bytes = s->rx_bytes;
	last_port_stats[portid].rx_missed = s->rx_misses;
	last_port_stats[portid].tx_bytes = s->tx_bytes;
	last_port_stats[portid].tx_pkts = s->tx_pkts;
	last_port_stats[portid].time = cycles;
	
}

static int bzpp_cli_show_stats(struct cli_def *cli, const char *command, char *argv[], int argc)
{
	int core;
	uint16_t portid;
	struct bzpp_port_stats stats;
	struct bzpp_lcore_stats stats2;
	struct rte_eth_link link;

	cli_print(cli, "%5s %12s %12s %12s %12s %12s %12s %5s", "port", "rx_pkts", "rx_misses", "rx_err", "rx_nobuf", "tx_pkts",
			  "tx_err", "link");

	RTE_ETH_FOREACH_DEV(portid)
	{
		_bzpp_cli_get_port_stats(portid, &stats);
		rte_eth_link_get_nowait(portid, &link);
		cli_print(cli, "%5u %12llu %12llu %12llu %12llu %12llu %12llu %5s(%uGbps)", portid, 
			stats.rx_pkts, stats.rx_misses, stats.rx_errors, stats.rx_nobuf, stats.tx_pkts, stats.tx_errors,
			link.link_status ? "up":"down", link.link_speed/1000);
	}

	cli_print(cli, " ");

	cli_print(cli, "%5s %12s %12s %12s %12s", "core", "rx_pkts", "rx_timeouts", 
		"tx_pkts", "tx_fails");
	RTE_LCORE_FOREACH_SLAVE(core)
	{
		_bzpp_cli_get_lcore_stats(core, &stats2);
		cli_print(cli, "%5u %12llu %12llu %12llu %12llu", core, stats2.rx_pkts,
			stats2.rx_timeouts, stats2.tx_pkts, stats2.tx_fails);
	}
	
	return 0;
}

static int bzpp_cli_show_rate(struct cli_def *cli, const char *command, char *argv[], int argc)
{
	int core;
	uint16_t portid;
	struct bzpp_port_stats stats;
	struct bzpp_lcore_stats stats2;

	cli_print(cli, "%5s %12s %12s %12s %12s %12s", "port", "rx_pps", "rx_bps", "rx_misses_pps", "tx_pps", "tx_bps");

	RTE_ETH_FOREACH_DEV(portid)
	{
		_bzpp_cli_get_port_stats(portid, &stats);
		cli_print(cli, "%5u %12llu %12llu %12llu %12llu %12llu", portid, 
			stats.rx_pkt_rate, stats.rx_bit_rate, stats.rx_miss_rate, stats.tx_pkts_rate, stats.tx_bit_rate);
	}

	cli_print(cli, " ");

	cli_print(cli, "%5s %12s %12s %12s %12s", "core", "rx_pps", "rx_to_pps", 
		"tx_pps", "tx_fail_pps");
	RTE_LCORE_FOREACH_SLAVE(core)
	{
		_bzpp_cli_get_lcore_stats(core, &stats2);
		cli_print(cli, "%5u %12llu %12llu %12llu %12llu", core, stats2.rx_pkt_rate,
			stats2.rx_to_rate, stats2.tx_pkt_rate, stats2.tx_fail_rate);
	}

#if 0
	cli_print(cli, " ");

	cli_print(cli, "%5s %12s %12s %12s %12s", "core", "rx_pkts", "rx_timeouts", 
		"tx_pkts", "tx_fails");
	RTE_LCORE_FOREACH_SLAVE(core)
	{
		cli_print(cli, "%5u %12llu %12llu %12llu %12llu", core, pp_lcore_data[core].rx_pkts,
			pp_lcore_data[core].rx_timeouts, pp_lcore_data[core].tx_pkts, pp_lcore_data[core].tx_fails);
	}
#endif
	
	return 0;
}


static int bzpp_cli_clear_stats(struct cli_def *cli, const char *command, char *argv[], int argc)
{
	int core;
	uint16_t portid;
	extern struct bzpp_lcore_data pp_lcore_data[];

	memset(&last_port_stats, 0, sizeof(last_port_stats));
	RTE_ETH_FOREACH_DEV(portid)
	{
		rte_eth_stats_reset(portid);
	}

	RTE_LCORE_FOREACH_SLAVE(core)
	{
		pp_lcore_data[core].rx_pkts = 0;
		pp_lcore_data[core].rx_timeouts = 0;
		pp_lcore_data[core].tx_pkts = 0;
		pp_lcore_data[core].tx_fails = 0;
	}
	return 0;
}


static int bzpp_cli_stop(struct cli_def *cli, const char *command, char *argv[], int argc)
{
	bzpp_start(0);
}

static int bzpp_cli_start(struct cli_def *cli, const char *command, char *argv[], int argc)
{
	if(bzpp_start(1))
		cli_print(cli, "Could not start");
	return 0;
}


static int bzpp_cli_pp_debug(struct cli_def *cli, const char *command, char *argv[], int argc)
{
	extern uint64_t pp_dbg_flag;
	char *s;

	s = cli_get_optarg_value(cli, "value", NULL);
	if(s != NULL)
		pp_dbg_flag = strtol(s, NULL, 16);
	
	return 0;
}

static int bzpp_cli_timeout_set(struct cli_def *cli, const char *command, char *argv[], int argc)
{
	int timeout;
	char *s;

	s = cli_get_optarg_value(cli, "value", NULL);
	if(s != NULL)
	{
		timeout = atoi(s);
		cli_set_idle_timeout_callback(cli, timeout, bzpp_cli_timeout);
	}
	return 0;
}

static int bzpp_cli_timeout_show(struct cli_def *cli, const char *command, char *argv[], int argc)
{
	cli_print(cli, "CLI idle timeout: %d second", cli->idle_timeout);
	return 0;
}

static int bzpp_cli_debug_show(struct cli_def *cli, const char *command, char *argv[], int argc)
{
	extern uint64_t pp_dbg_flag;
	cli_print(cli, "bzpp debug flags: %x",  pp_dbg_flag);
	return 0;
}

static int bzpp_cli_version_show(struct cli_def *cli, const char *command, char *argv[], int argc)
{
	cli_print(cli, "%s",  BZPP_VERSION);
	return 0;
}



static void init_bzpp_cmds(struct cli_def *cli)
{
	struct cli_command *c, *s;
	struct cli_optarg *o;

	c = cli_register_command(cli, NULL, "show", NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Show commands");
	s = cli_register_command(cli, c, "stats", bzpp_cli_show_stats, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Show stats");
	s = cli_register_command(cli, c, "version", bzpp_cli_version_show, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Show version");
	s = cli_register_command(cli, c, "rate", bzpp_cli_show_rate, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Show  rate");
	s = cli_register_command(cli, c, "timeout", bzpp_cli_timeout_show, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Show CLI idle timeout");
	s = cli_register_command(cli, c, "debug", bzpp_cli_debug_show, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Show BZPP debug flags");

	c = cli_register_command(cli, NULL, "clear", NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Clear commands");
	s = cli_register_command(cli, c, "stats", bzpp_cli_clear_stats, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Clear stats");

	c = cli_register_command(cli, NULL, "stop", bzpp_cli_stop, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Clear commands");
	c = cli_register_command(cli, NULL, "start", bzpp_cli_start, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Clear commands");

	c = cli_register_command(cli, NULL, "debug", bzpp_cli_pp_debug, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Set debug flags");
	cli_register_optarg(c, "value", CLI_CMD_ARGUMENT, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, 
		"Hex value", NULL, NULL, NULL);

	c = cli_register_command(cli, NULL, "timeout", bzpp_cli_timeout_set, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Set CLI idle timeout");
	cli_register_optarg(c, "value", CLI_CMD_ARGUMENT, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, 
		"Seconds", NULL, NULL, NULL);

#if 0
	//port add command
	s = cli_register_command(cli, c, "add", tofino_port_add_cmd, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Add port");
	cli_register_optarg(s, "id", CLI_CMD_ARGUMENT, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, 
		"Port id", NULL, NULL, NULL);
	cli_register_optarg(s, "connector", CLI_CMD_ARGUMENT, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, 
		"front port connector id", NULL, NULL, NULL);
	cli_register_optarg(s, "channel", CLI_CMD_ARGUMENT, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, 
		"front port connector id", NULL, NULL, NULL);
	cli_register_optarg(s, "speed", CLI_CMD_ARGUMENT, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, 
		"Port speed", NULL, NULL, NULL);
	o = cli_register_optarg(s, "class", CLI_CMD_ARGUMENT, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, 
		"port class", NULL, NULL, NULL);
	cli_optarg_addhelp(o, "fp", NULL);
	cli_optarg_addhelp(o, "np", NULL);
	cli_optarg_addhelp(o, "lb", NULL);
		
	o = cli_register_optarg(s, "fec", CLI_CMD_OPTIONAL_ARGUMENT, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, 
		"Fec set", NULL, NULL, NULL);
	cli_optarg_addhelp(o, "rs", NULL);
	cli_optarg_addhelp(o, "fc", NULL);
	

	//port del cmd
	s = cli_register_command(cli, c, "del", tofino_port_del_cmd, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Delete port");
	cli_register_optarg(s, "id", CLI_CMD_ARGUMENT, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, 
		"Port id", NULL, NULL, NULL);

	//port info cmd
	s = cli_register_command(cli, c, "info", tofino_port_info_cmd, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Show port info");
	cli_register_optarg(s, "id", CLI_CMD_ARGUMENT, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, 
		"Port id", NULL, NULL, NULL);

	s = cli_register_command(cli, c, "stats", tofino_port_stats_cmd, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Show port stats");
	cli_register_optarg(s, "id", CLI_CMD_ARGUMENT, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, 
		"Port id", NULL, NULL, NULL);

	s = cli_register_command(cli, c, "speed", tofino_port_speed_cmd, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "set port speed");
	 cli_register_optarg(s, "id", CLI_CMD_ARGUMENT, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, 
		"Port id", NULL, NULL, NULL);
	o = cli_register_optarg(s, "speed", CLI_CMD_ARGUMENT, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, 
		"Speed", NULL, NULL, NULL);
	cli_optarg_addhelp(o, "10G", NULL);
	cli_optarg_addhelp(o, "25G", NULL);
	cli_optarg_addhelp(o, "100G", NULL);
	cli_optarg_addhelp(o, "auto", NULL);
#endif
}




static void *bzpp_cli_thread(void *arg)
{
	int x = (long)arg;
  	struct cli_def *cli;
 // struct cli_optarg *o;

  	cli = cli_init();
  	cli_set_banner(cli, BZPP_CLI_BANNER);
  	cli_set_hostname(cli, BZPP_CLI_PROMPT);
  	cli_telnet_protocol(cli, 1);
 	cli_set_idle_timeout_callback(cli, 300, bzpp_cli_timeout);
	init_bzpp_cmds(cli); 	
  	cli_loop(cli, x);
  	cli_done(cli);
	return NULL;
}


static int new_bzpp_cli_connection(int x)
{
	pthread_t th;
	long fd = x;
	if(pthread_create(&th, NULL, bzpp_cli_thread, (void *)fd))
		return BZPP_E_FAIL;
	return 0;
}


int bzpp_cli_run()
{
	int s, x;
	struct sockaddr_in addr;
	int on = 1;
	unsigned short port;

	port = BZPP_CLI_PORT;
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return 1;
	}

	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) {
		perror("setsockopt");
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(port);
	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			perror("bind");
		return 1;
	}

	if (listen(s, 50) < 0) {
		perror("listen");
		return 1;
	}

	BZPP_INFO("CLI listening on port %d\n", port);
	while ((x = accept(s, NULL, 0))) {
		  socklen_t len = sizeof(addr);
	  if (getpeername(x, (struct sockaddr *)&addr, &len) >= 0)
	    	BZPP_INFO(" accepted connection from %s\n", inet_ntoa(addr.sin_addr));
		 new_bzpp_cli_connection(x);
	}
	close(s);
	return 0;
}

