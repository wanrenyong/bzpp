#include <errno.h>
#include <memory.h>
#include <stdarg.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <linux/socket.h>
#include <arpa/inet.h>
#include <pthread.h>


#include <rte_cycles.h>
#include <rte_common.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_eventdev.h>
#include <rte_bus_vdev.h>

#include <rte_event_eth_rx_adapter.h>
#include <rte_event_eth_tx_adapter.h>


#include "bzpp.h"
#include "bzpp_log.h"
#include "bzpp_cli.h"

#define PP_MBUF_PRIV_SZ   128
#define PP_DEF_MBUF_SZ     RTE_MBUF_DEFAULT_BUF_SIZE
#define PP_ATOMIC_QID 1
#define PP_ORDERED_QID 0






typedef struct bzpp_init_config
{
	uint8_t event_dev;
	uint32_t nb_rx_core;      
	uint32_t mbuf_cache_size;
	uint32_t nb_port_rxq;
	uint32_t nb_port_txq;
	uint32_t port_rxq_sz;
	uint32_t port_txq_sz;
}bzpp_init_config_t;


#define PP_DBG_FLAG_RX    0x1
#define PP_DBG_FLAG_TX    0x2
#define PP_DBG_FLAG_RX_SAMPLE 0x4
#define PP_DBG_FLAG_RX_COUNT 0x8

uint64_t pp_dbg_flag = 0;

bzpp_init_config_t pp_init_config;

struct bzpp_lcore_data pp_lcore_data[RTE_MAX_LCORE];
struct bzpp_global_data pp_global_data;


struct bzpp_port_data pp_port_data[RTE_MAX_ETHPORTS];


static const struct rte_eth_conf port_conf_default = {
		.rxmode = {
			.mq_mode = ETH_MQ_RX_NONE,
		},
		.intr_conf = {
			.rxq = 1,
		},
	};



int bzpp_ports_init(void)
{
	 uint16_t portid;
	 int q;
	 int ret;
	 char s[32];

	 bzpp_init_config_t *pp_init_cfg = &pp_init_config;
#if 0
	 pp_global_data.pkt_mempool = rte_pktmbuf_pool_create(s,
						   pp_init_cfg->nb_port_rxq * pp_init_cfg->port_rxq_sz,
						   pp_init_cfg->mbuf_cache_size,
						   PP_MBUF_PRIV_SZ,
						   PP_DEF_MBUF_SZ,
						   rte_eth_dev_socket_id(portid));
	if(pp_global_data.pkt_mempool == NULL)
	{
		BZPP_ERR_LOG("Could not alloc pkt  mempool");
		return BZPP_E_NOMEM;
	}
#endif
	 RTE_ETH_FOREACH_DEV(portid){

	 	printf("Initing ethdev %u\n", portid);

		rte_eth_dev_stop(portid);
#if 1
		if(pp_port_data[portid].pkt_mempool != NULL)
		{
			rte_mempool_free(pp_port_data[portid].pkt_mempool);
			pp_port_data[portid].pkt_mempool = NULL;
		}
		sprintf(s, "port_%d-packet_pool", portid);
	 	pp_port_data[portid].pkt_mempool = rte_pktmbuf_pool_create(s,
						   pp_init_cfg->nb_port_rxq * pp_init_cfg->port_rxq_sz,
						   pp_init_cfg->mbuf_cache_size,
						   PP_MBUF_PRIV_SZ,
						   PP_DEF_MBUF_SZ,
						   rte_eth_dev_socket_id(portid));
		if(pp_port_data[portid].pkt_mempool == NULL)
		{
			BZPP_ERR_LOG("Could not alloc mbuf for port %u", portid);
			return BZPP_E_NOMEM;
		}
#else
		pp_port_data[portid].pkt_mempool = pp_global_data.pkt_mempool;
#endif

		printf("Configuring ethdev %u\n", portid);
		ret = rte_eth_dev_configure(portid,  pp_init_cfg->nb_port_rxq,
				 pp_init_cfg->nb_port_txq, &port_conf_default);
		if(ret)
		{
			BZPP_ERR_LOG("Could not configure port %u", portid);
			return BZPP_E_FAIL;
		}

		
		
		for(q = 0; q < pp_init_cfg->nb_port_rxq; q++){

			printf("Setting up ethdev %u rxq %u\n", portid, q);
			ret = rte_eth_rx_queue_setup(portid, q, pp_init_cfg->port_rxq_sz,
					rte_eth_dev_socket_id(portid), NULL, pp_port_data[portid].pkt_mempool);
			if(ret)
			{
				BZPP_ERR_LOG("Could not setup port %u rxq %u", portid, q);
				return BZPP_E_FAIL;
			}
/*
			ret = rte_eth_dev_rx_queue_start(portid, q);
			if(ret)
			{
				BZPP_ERR_LOG("Could not start port %u rxq %u", portid, q);
				return BZPP_E_FAIL;
			}
*/
		}
		for(q = 0; q < pp_init_cfg->nb_port_txq; q++){
			
			printf("Setting up ethdev %u txq %u\n", portid, q);
			
			ret = rte_eth_tx_queue_setup(portid, q, pp_init_cfg->port_txq_sz,
					rte_eth_dev_socket_id(portid), NULL);
			if(ret)
			{
				BZPP_ERR_LOG("Could not setup port %u txq %u", portid, q);
				return BZPP_E_FAIL;
			}
/*
			ret = rte_eth_dev_tx_queue_start(portid, q);
			if(ret)
			{
				BZPP_ERR_LOG("Could not start port %u txq %u", portid, q);
				return BZPP_E_FAIL;
			}
*/			
		}
		
		ret = rte_eth_promiscuous_enable(portid);
		if (ret != 0)
		{
			BZPP_ERR_LOG("Could not enable port %u promiscuous", portid);
			return BZPP_E_FAIL;
		}
		
	 }
	 return 0;
}


int bzpp_rxtx_init()
{
	int ret;
	uint8_t evdev;
	uint8_t rx_adapter = 0;
	uint8_t tx_adapter = 0;
	uint16_t evdev_port;
	unsigned int core;
	uint16_t ethdev;
	uint16_t eth_rxq, eth_txq;
	uint8_t evdev_q;

	bzpp_init_config_t *init_cfg = &pp_init_config;
	
	struct rte_event_port_conf evport_cfg={0};
	struct rte_event_dev_info evdev_info;
	struct rte_event_dev_config evdev_cfg = {0};
	struct rte_event_port_conf rx_p_conf = {0};
	struct rte_event_queue_conf evdev_q_cfg={0};
	struct rte_event_eth_rx_adapter_queue_conf rx_q_conf = {
		.ev = {
#if 0
			.queue_id = PP_ORDERED_QID,
			.sched_type = RTE_SCHED_TYPE_ATOMIC,
#endif
			.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
		},
	};
	
	evdev = init_cfg->event_dev;
	rx_adapter = 0;
	
	ret = rte_event_dev_info_get(evdev, &evdev_info);
	if(ret)
	{
		BZPP_ERR_LOG("Could not get event %u info", evdev);
		return BZPP_E_FAIL;
	}

	/*one event port per core except master core*/
	evdev_cfg.nb_event_ports = 0;
	RTE_LCORE_FOREACH_SLAVE(core)
		evdev_cfg.nb_event_ports++;

	if(evdev_cfg.nb_event_ports > evdev_info.max_event_ports)
	{
		BZPP_ERR("nb_event_ports=%u > max_event_ports=%u", 
			evdev_cfg.nb_event_ports, evdev_info.max_event_ports);
		return BZPP_E_FAIL;
	}
	
	evdev_cfg.nb_event_queues = 2;	
	evdev_cfg.nb_event_port_dequeue_depth = evdev_info.max_event_port_dequeue_depth;
	evdev_cfg.nb_event_port_enqueue_depth = evdev_info.max_event_port_enqueue_depth;
	evdev_cfg.nb_events_limit = evdev_info.max_num_events;
	evdev_cfg.nb_event_queue_flows = evdev_info.max_event_queue_flows;
	evdev_cfg.dequeue_timeout_ns = evdev_info.max_dequeue_timeout_ns;

	ret = rte_event_dev_configure(evdev, &evdev_cfg);
	if(ret)
	{
		BZPP_ERR_LOG("Could not configure event dev %u", evdev);
		return BZPP_E_FAIL;
	}
	BZPP_INFO_LOG("Configured event dev: id=%u, nb_event_ports=%u, "
		"nb_event_queues=%u, nb_event_port_dequeue_depth=%u, "
		"nb_event_port_enqueue_depth=%u, nb_events_limit=%u, "
		"nb_event_queue_flows=%u, dequeue_timeout_ns=%u", evdev, evdev_cfg.nb_event_ports, evdev_cfg.nb_event_queues,
		evdev_cfg.nb_event_port_dequeue_depth, 
		evdev_cfg.nb_event_port_enqueue_depth, 
		evdev_cfg.nb_events_limit, evdev_cfg.nb_event_queue_flows,
		evdev_cfg.dequeue_timeout_ns);


	/*setup evdev aotmic queue*/
	evdev_q_cfg.nb_atomic_flows = evdev_info.max_event_queue_flows;
	evdev_q_cfg.schedule_type = RTE_SCHED_TYPE_ATOMIC;
	evdev_q_cfg.priority = RTE_EVENT_DEV_PRIORITY_NORMAL;
	evdev_q_cfg.nb_atomic_order_sequences = evdev_info.max_event_queue_flows;
	evdev_q_cfg.event_queue_cfg = RTE_EVENT_QUEUE_CFG_ALL_TYPES;
	ret = rte_event_queue_setup(evdev, PP_ATOMIC_QID, &evdev_q_cfg);
	if(ret)
	{
		BZPP_ERR("Could not setup eventdev %u queue %u", evdev, PP_ATOMIC_QID);
		return BZPP_E_FAIL;
	}

	/*setup evdev ordered queue*/
	evdev_q_cfg.schedule_type = RTE_SCHED_TYPE_ORDERED;
	ret = rte_event_queue_setup(evdev, PP_ORDERED_QID, &evdev_q_cfg);
	if(ret)
	{
		BZPP_ERR("Could not setup eventdev %u queue %u", evdev, PP_ORDERED_QID);
		return BZPP_E_FAIL;
	}
	
	/*setup evdev port*/
	evport_cfg.dequeue_depth = evdev_info.max_event_port_dequeue_depth;
	evport_cfg.enqueue_depth = evdev_info.max_event_port_enqueue_depth;
	evport_cfg.new_event_threshold = evdev_info.max_num_events;
#if 1
	if(evdev_info.event_dev_cap & RTE_EVENT_DEV_CAP_IMPLICIT_RELEASE_DISABLE)
		evport_cfg.disable_implicit_release = 0;
#endif

	evdev_port = 0;
	
	RTE_LCORE_FOREACH_SLAVE(core)
	{
		BZPP_INFO("Setting up event port %d, core %d", evdev_port, core);
		ret = rte_event_port_setup(evdev, evdev_port, &evport_cfg);
		if(ret)
		{
			BZPP_ERR("Colud not set up event %u port %u setup", 
				evdev, evdev_port);
			return BZPP_E_FAIL;
		}

		pp_lcore_data[core].evdev_id = evdev;
		pp_lcore_data[core].evdev_port = evdev_port;
		if(evdev_port < init_cfg->nb_rx_core)
		{
			pp_lcore_data[core].is_rx_core = 1;  /*the little numble of core is rx core*/
			evdev_q = PP_ORDERED_QID;     
		}
		else
			evdev_q = PP_ATOMIC_QID;
#if 1
		/*link  core to evdev_port*/
		ret = rte_event_port_link(evdev, evdev_port, &evdev_q, NULL, 1);
		if(ret < 1)
		{
			BZPP_ERR("Could not link event port, event port=%u, event queue=%u",
					evdev_port, evdev_q);
			return BZPP_E_FAIL;
		}
#endif		
		evdev_port++;
	}

	ret = rte_event_eth_rx_adapter_create(rx_adapter, evdev, &evport_cfg);
	if(ret)
	{
		BZPP_ERR_LOG("Could not create event eth rx adpater, event devid=%u", evdev);
		return BZPP_E_FAIL;
	}
#if 0
	ret = rte_event_eth_tx_adapter_create(tx_adapter, evdev, &evport_cfg);
	if(ret)
	{
		BZPP_ERR_LOG("Could not create event eth tx adpater, event devid=%u", evdev);
		return BZPP_E_FAIL;
	}
#endif
	/*link eth rxq to event order queue*/
	RTE_ETH_FOREACH_DEV(ethdev)
	{
		for(eth_rxq = 0; eth_rxq < init_cfg->nb_port_rxq; eth_rxq++)
		{
			ret = rte_event_eth_rx_adapter_queue_add(rx_adapter, ethdev, 
				eth_rxq, &rx_q_conf);
			if(ret)
			{
				BZPP_ERR("Could not add rx adapter queue, ethdev=%u, ethdev_rxq=%u",
					ethdev, eth_rxq);
				return BZPP_E_FAIL;
			}
		}
#if 0
		for(eth_txq = 0; eth_txq < init_cfg->nb_port_txq; eth_txq++)
		{
			ret = rte_event_eth_tx_adapter_queue_add(tx_adapter, ethdev, eth_txq);
			if(ret)
			{
				BZPP_ERR("Could not add tx adapter queue, ethdev=%u, ethdev_txq=%u",
					ethdev, eth_txq);
				return BZPP_E_FAIL;
			}
		}
#endif
	}

	BZPP_INFO("Starting event eth rx adapter %u", evdev);
	ret = rte_event_eth_rx_adapter_start(rx_adapter);
	if(ret)
	{
		BZPP_ERR("Could not start event eth rx adpater %u", rx_adapter);
		return BZPP_E_FAIL;
	}
#if 0
	BZPP_INFO("Starting event eth tx adapter %u", evdev);
	ret = rte_event_eth_tx_adapter_start(rx_adapter);
	if(ret)
	{
		BZPP_ERR("Could not start event eth tx adpater %u", tx_adapter);
		return BZPP_E_FAIL;
	}
#endif
#if 1
	BZPP_INFO("Starting event dev %u", evdev);
	ret = rte_event_dev_start(evdev);
	if(ret)
	{
		BZPP_INFO("Could not start event dev %u", evdev);
		return BZPP_E_FAIL;
	}
#endif
	return 0;
	
}

static inline void dump_pp_event(struct bzpp_lcore_data *lcore_data, struct rte_event *ev)
{
	
}

#define BZPP_RX_BURST 8
int bzpp_worker(void *arg)
{
	struct bzpp_lcore_data *lcore_data;
	int ret;
	int count;
	int i;
	uint64_t now, last_sample;
	struct rte_mbuf *mbuf;
	struct rte_event ev[BZPP_RX_BURST];
	struct rte_event *e = ev;

	lcore_data = &pp_lcore_data[rte_lcore_id()];
	last_sample = 0;

	
	
	while(1)
	{
		if(unlikely(!lcore_data->start))
			continue;
		count = rte_event_dequeue_burst(lcore_data->evdev_id, 
			lcore_data->evdev_port, ev, BZPP_RX_BURST, 1000);
			
		if(count > 0)
			lcore_data->rx_pkts += count;
		else
		{
			lcore_data->rx_timeouts++;
			continue;
		}

		if(unlikely(pp_dbg_flag & PP_DBG_FLAG_RX_COUNT))
			printf("[%u] rx %u events\n", rte_lcore_id(), count);
		
		for(i = 0; i < count; i++)
		{
			e = &ev[i];
			mbuf = e->mbuf;
			if(unlikely(pp_dbg_flag & PP_DBG_FLAG_RX))
			{
				if(pp_dbg_flag & PP_DBG_FLAG_RX_SAMPLE)
				{
					now = rte_get_timer_cycles();
					if(likely((now - last_sample)/rte_get_timer_hz() < 5))
					{
						ret = 0;
						goto skip_dump;
					}
					else
					{
						last_sample = now;
						ret = 1;
					}
				}
				else
					ret = 1;
				if(ret)
				{
					printf("%s[%u] Rx event: flow_id=%u, event_type=%u, op=%u, sched_type=%u,"
						" rx_port=%u, pkt_len=%u\n",
						lcore_data->is_rx_core?"*":"",
						rte_lcore_id(), 
						e->flow_id, e->event_type, e->op, e->sched_type,
						mbuf->port, mbuf->pkt_len);
				}
			}
skip_dump:			
			if(lcore_data->is_rx_core)
			{
				//rte_delay_us(5);
				e->flow_id++;
				e->sched_type = RTE_SCHED_TYPE_ATOMIC;
				e->queue_id = PP_ATOMIC_QID;
				e->event_type = RTE_EVENT_TYPE_CPU;
				e->op = RTE_EVENT_OP_FORWARD;
				ret = rte_event_enqueue_forward_burst(lcore_data->evdev_id, 
					lcore_data->evdev_port, e, 1); /*forward to processing core*/
				if(ret > 0) 
				{
					lcore_data->tx_pkts++;
					continue;
				}
				else
				{
					lcore_data->tx_fails++;
				}
			}
#if 0
			else
			{
				e->op = RTE_EVENT_OP_FORWARD;
				ret = rte_event_eth_tx_adapter_enqueue(lcore_data->evdev_id, 
						lcore_data->evdev_port, e, 1, 0);
					
			}
#endif
			
			
			e->op = RTE_EVENT_OP_RELEASE;
			ret = rte_event_enqueue_burst(lcore_data->evdev_id, 
				lcore_data->evdev_port, e, 1);	
			if(ret <= 0)
			{
				if(pp_dbg_flag & PP_DBG_FLAG_TX)
				{
					printf("[%u] Tx event fail: flow_id=%u, event_type=%u, op=%u, sched_type=%u\n",
						rte_lcore_id(), e->flow_id, e->event_type, e->op, e->sched_type);
				}
			}
#if 0			
			if(ret > 0) 
			{
				if(pp_dbg_flag & PP_DBG_FLAG_TX)
				{
					printf("[%u] Tx event: flow_id=%u, event_type=%u, op=%u, sched_type=%u\n",
						rte_lcore_id(), e->flow_id, e->event_type, e->op, e->sched_type);
				}
				if(lcore_data->is_rx_core)
				{
					lcore_data->tx_pkts++;
					break;
				}
			}
			else
			{
				BZPP_ERR("[%u] Tx event error: flow_id=%u, event_type=%u, op=%u, sched_type=%u\n",
						rte_lcore_id(), e->flow_id, e->event_type, e->op, e->sched_type);
			}
#endif				
#if 1	
			rte_pktmbuf_free(mbuf);
#endif
		}
	}
	return 0;
}




static int  bzpp_init_config_parse(int argc, char *argv[])
{
	int n, opt;
	char **argvopt;
	int opt_idx;
	
	static struct option lgopts[] = {
		{ "rx-cores",			1, 0, 0 },
		{ "nb-rxq",			1, 0, 0 },
		{ "rxq-sz",			1, 0, 0 },
	};


	pp_init_config.event_dev = 0;
	pp_init_config.mbuf_cache_size = 512;
	pp_init_config.nb_port_rxq = 1;
	pp_init_config.nb_port_txq = 1;
	pp_init_config.port_rxq_sz = 8192;
	pp_init_config.port_txq_sz = 8192;
	pp_init_config.nb_rx_core = 4;

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, "R:",
				 lgopts, &opt_idx)) != EOF) {
				 
		switch (opt) {
			case 'R':
				pp_init_config.nb_rx_core = strtoul(optarg, NULL, 10);
				break;
			case 0:
				if (!strcmp(lgopts[opt_idx].name, "rx-cores")) {
					pp_init_config.nb_rx_core = strtoul(optarg, NULL, 10);
				}
				else if (!strcmp(lgopts[opt_idx].name, "nb-rxq"))
				{
					pp_init_config.nb_port_rxq = strtoul(optarg, NULL, 10);
				}
				else if (!strcmp(lgopts[opt_idx].name, "rxq-sz"))
				{
					pp_init_config.port_rxq_sz = strtoul(optarg, NULL, 10);
				}
				break;
		}
	}

	printf("PP config:\n");
	printf("event_dev: %u\n", pp_init_config.event_dev);
	printf("mbuf_cache_size: %u\n", pp_init_config.mbuf_cache_size);
	printf("nb_port_rxq: %u\n", pp_init_config.nb_port_rxq);
	printf("nb_port_txq: %u\n", pp_init_config.nb_port_txq);
	printf("port_rxq_sz: %u\n", pp_init_config.port_rxq_sz);
	printf("port_txq_sz: %u\n", pp_init_config.port_txq_sz);
	printf("nb_rx_core: %u\n", pp_init_config.nb_rx_core);
				 
	return 0;
}


int bzpp_start(int start)
{
	int ret;
	uint16_t portid;
	uint8_t core;

	if(start)
	{
		RTE_LCORE_FOREACH_SLAVE(core)
			pp_lcore_data[core].start = start;
		rte_delay_ms(30);
	}
	
	RTE_ETH_FOREACH_DEV(portid){
		BZPP_INFO("%s ethdev %u", start ? "Starting":"stopping", portid);
		if(start)
		{
			ret = rte_eth_dev_start(portid);
			if(ret)
			{
				BZPP_ERR("Could not %s port %u",  portid);
				return BZPP_E_FAIL;
			}
		}
		else
			rte_eth_dev_stop(portid);
	}

	if(!start)
	{
		RTE_LCORE_FOREACH_SLAVE(core)
			pp_lcore_data[core].start = start;
	}
	
	return 0;	
}


int bzpp_init(int argc, char *argv[])
{
	int ret;
	uint8_t portid;

	ret = rte_eal_init(argc, argv);
	if(ret < 0)
	{
		BZPP_ERR("Failed to init rte eal");
		return BZPP_E_FAIL;
	}

	argc -= ret;
	argv += ret;
	
	 bzpp_init_config_parse(argc, argv);

	ret = rte_event_dev_count();
	printf("Event device count=%d\n", ret);
	if(ret < 1)
		return -1;	

	
	ret = bzpp_ports_init();
	if(ret)
	{
		BZPP_ERR("Failed to init pp ports");
		return BZPP_E_FAIL;
	}

	ret = bzpp_rxtx_init();
	if(ret)
	{
		BZPP_ERR("Could not init rx pp");
		return ret;
	}
#if 0
	RTE_ETH_FOREACH_DEV(portid){
		printf("Starting ethdev %u\n", portid);
		ret = rte_eth_dev_start(portid);
		if(ret)
		{
			BZPP_ERR_LOG("Could not start port %u", portid);
			return BZPP_E_FAIL;
		}
	}
#endif
	return 0;
}





int main(int argc, char *argv[])
{

	printf("BZPP: %s\n",  BZPP_VERSION);
	
	if(bzpp_init(argc, argv))
	{
		printf("Could not init bzpp\n");
		return -1;
	}
	
	rte_eal_mp_remote_launch(bzpp_worker, NULL, 0);


	bzpp_cli_run();
	
	rte_eal_mp_wait_lcore();

	

	return 0;
	
}
