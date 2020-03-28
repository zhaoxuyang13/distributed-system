/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <stdint.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_udp.h>
#include <rte_ip.h>
#include <rte_ether.h>
#include <rte_byteorder.h>

#define IPv4(a,b,c,d) (a | b << 8 | c << 16 | d << 24)
#define SRC_IP IPv4(192,168,152,1)
#define SRC_PORT 233
#define DST_IP IPv4(192,168,0,101)
#define DST_PORT 233
#define IPv4_SIZE sizeof(struct rte_ipv4_hdr)
#define ETHER_SIZE sizeof(struct rte_ether_hdr)
#define UDP_SIZE sizeof(struct rte_udp_hdr)

#define HEADER_SIZE (IPv4_SIZE + ETHER_SIZE + UDP_SIZE)
#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32


static const struct rte_eth_conf port_conf_default = {
	.rxmode = {
		.max_rx_pkt_len = RTE_ETHER_MAX_LEN,
	},
};

/* basicfwd.c: Basic DPDK skeleton forwarding example. */

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int
port_init(uint16_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	uint16_t nb_rxd = RX_RING_SIZE;
	uint16_t nb_txd = TX_RING_SIZE;
	int retval;
	uint16_t q;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf txconf;

	if (!rte_eth_dev_is_valid_port(port))
		return -1;

	retval = rte_eth_dev_info_get(port, &dev_info);
	if (retval != 0) {
		printf("Error during getting device (port %u) info: %s\n",
				port, strerror(-retval));
		return retval;
	}

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		port_conf.txmode.offloads |=
			DEV_TX_OFFLOAD_MBUF_FAST_FREE;

	/* Configure the Ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
	if (retval != 0)
		return retval;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
				rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (retval < 0)
			return retval;
	}

	txconf = dev_info.default_txconf;
	txconf.offloads = port_conf.txmode.offloads;
	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, nb_txd,
				rte_eth_dev_socket_id(port), &txconf);
		if (retval < 0)
			return retval;
	}

	/* Start the Ethernet port. */
	retval = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	/* Display the port MAC address. */
	struct rte_ether_addr addr;
	retval = rte_eth_macaddr_get(port, &addr);
	if (retval != 0)
		return retval;

	printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			port,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);

	/* Enable RX in promiscuous mode for the Ethernet device. */
	retval = rte_eth_promiscuous_enable(port);
	if (retval != 0)
		return retval;

	return 0;
}
// static struct rte_mbuf *udp_wrapper(struct rte_mbuf *mbuf, uint16_t dst_port){
// 	struct rte_udp_hdr hdr;
// 	char *pkt;

// 	memset(&hdr, 0, sizeof(hdr));
// 	hdr.src_port = rte_cpu_to_be_16(SRC_PORT);
// 	hdr.dst_port = rte_cpu_to_be_16(dst_port);
// 	hdr.dgram_len = mbuf->pkt_len + sizeof(struct rtudp_hdr);
// 	hdr.dgram_len = rte_cpu_to_be_16(hdr.dgram_len);

// 	/* Pre-add UDP header */
// 	pkt = rte_pktmbuf_prepend(mbuf, sizeof(hdr));
// 	rte_memcpy(pkt, &hdr, sizeof(hdr));
// 	return mbuf;
// }


static void construct_udp_packet(const char* data, uint32_t len,struct rte_mbuf *m){
	struct rte_ether_hdr *eth_header;
	struct rte_ipv4_hdr *ipv4_header;
	struct rte_udp_hdr *udp_header;
	void *payload;
	
	// printf("constructing\n");
	eth_header = rte_pktmbuf_mtod(m,struct rte_ether_hdr *);
	ipv4_header = rte_pktmbuf_mtod_offset(m,struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr ));
	udp_header = rte_pktmbuf_mtod_offset(m,struct rte_udp_hdr *,sizeof(struct rte_ipv4_hdr ) + sizeof(struct rte_ether_hdr ));
	payload = rte_pktmbuf_mtod_offset(m,void *,sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_ether_hdr) + sizeof(struct rte_udp_hdr));
	
	// printf("header init\n");
	struct rte_ether_addr src_addr,dst_addr;
	rte_eth_macaddr_get(0,&src_addr);
	rte_eth_macaddr_get(0,&dst_addr);
	eth_header->s_addr = src_addr;
	eth_header->d_addr = dst_addr;
	eth_header->ether_type = rte_be_to_cpu_16(RTE_ETHER_TYPE_IPV4); // big endian to CPU
	
	// printf("ether header \n");
	ipv4_header->version_ihl = 4 << 4 | 5; // ip version 4,  5 x4byte no option
	ipv4_header->type_of_service = 0;// DSCP / ECN  0=best effort
	ipv4_header->total_length = (len + sizeof(struct rte_ipv4_hdr ) + sizeof(struct rte_udp_hdr))<<8; // bit endian is shit. how to deal with it easily.
	ipv4_header->packet_id = 0;
	ipv4_header->fragment_offset = 0;
	ipv4_header->time_to_live = 20;
	ipv4_header->next_proto_id= 0x11; //udp
	ipv4_header->src_addr = SRC_IP;
	ipv4_header->dst_addr = DST_IP;
	ipv4_header->hdr_checksum = rte_ipv4_cksum(ipv4_header);

	// printf("ipv4 header\n");
	udp_header->src_port = SRC_PORT << 8;
	udp_header->dst_port = DST_PORT << 8;
	udp_header->dgram_len = (len + sizeof(struct rte_udp_hdr)) << 8; // bit endian
	udp_header->dgram_cksum = rte_raw_cksum(data,len); // check

	// printf("udp header\n");

	memcpy(payload,data,len);

	// printf("done packet\n");
}


/*
 * The lcore main. This is the main thread that does the work, reading from
 * an input port and writing to an output port.
 */
static __attribute__((noreturn)) void
lcore_main(struct rte_mempool *mbuf_pool, uint8_t nb_ports)
{
	uint16_t port;

	/*
	 * Check that the port is on the same NUMA node as the polling thread
	 * for best performance.
	 */
	RTE_ETH_FOREACH_DEV(port)
		if (rte_eth_dev_socket_id(port) > 0 &&
				rte_eth_dev_socket_id(port) !=
						(int)rte_socket_id())
			printf("WARNING, port %u is on remote NUMA node to "
					"polling thread.\n\tPerformance will "
					"not be optimal.\n", port);

	printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n",
			rte_lcore_id());

	/* Run until the application is quit or killed. */
	struct rte_mbuf *packets[1];
	packets[0] = rte_pktmbuf_alloc(mbuf_pool);
	// printf("alloc packet\n");
	char payload[22] = "hello from the inside";
	rte_pktmbuf_prepend(packets[0],HEADER_SIZE + strlen(payload));
	// printf("constructing packet\n");
	construct_udp_packet(payload,strlen(payload),packets[0]);
	// printf("constructed packet\n");
	for (;;) {
		const uint16_t nb_tx = rte_eth_tx_burst(0,0,packets,1);
		printf("Sending packet: %d\n",nb_tx);
		sleep(1);
		}
}
/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int
main(int argc, char *argv[])
{
	struct rte_mempool *mbuf_pool;
	unsigned nb_ports;
	uint16_t portid;

	/* Initialize the Environment Abstraction Layer (EAL). */
	int ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	argc -= ret;
	argv += ret;

	// /* Check that there is an even number of ports to send/receive on. */
	nb_ports = rte_eth_dev_count_avail();
	// if (nb_ports < 2 || (nb_ports & 1))
	// 	rte_exit(EXIT_FAILURE, "Error: number of ports must be even\n");

	/* Creates a new mempool in memory to hold the mbufs. */
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
		MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* Initialize all ports. */
	RTE_ETH_FOREACH_DEV(portid)
		if (port_init(portid, mbuf_pool) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu16 "\n",
					portid);

	if (rte_lcore_count() > 1)
		printf("\nWARNING: Too many lcores enabled. Only 1 used.\n");

	/* Call lcore_main on the master core only. */
	lcore_main(mbuf_pool,nb_ports);

	return 0;
}
