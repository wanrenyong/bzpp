#ifndef _BZPP_H_
#define _BZPP_H_


typedef enum
{
	BZPP_E_NONE,
	BZPP_E_FAIL,
	BZPP_E_INVAL,
	BZPP_E_BADPARAM,
	BZPP_E_NOMEM,
}bzpp_errno_t;


struct bzpp_lcore_data
{
	uint8_t evdev_id;
	uint8_t evdev_port;
	uint8_t is_rx_core;
	uint8_t start;
	uint8_t rx_burst;
	struct rte_event *rx_buf;
	struct{
		uint64_t rx_pkts;
		uint64_t rx_timeouts;
		uint64_t tx_pkts;
		uint64_t tx_fails;
	}__rte_cache_min_aligned;
}__rte_cache_min_aligned;


struct bzpp_global_data{
	struct rte_mempool *pkt_mempool;
}__rte_cache_min_aligned;

struct bzpp_port_data{
	struct rte_mempool *pkt_mempool;
};

int bzpp_start(int start);


#define BZPP_VERSION  "V.0.1 "__TIME__

#endif

