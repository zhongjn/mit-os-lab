#include "ns.h"

extern union Nsipc nsipcbuf;

static struct jif_pkt* pkt = (struct jif_pkt*)0x0ffff000;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
	while (1) {
		envid_t from_env;
		int req = ipc_recv(&from_env, pkt, NULL);
		assert(from_env == ns_envid);
		assert(req == NSREQ_OUTPUT);

		int r;
		while (1) {
			r = sys_net_try_send(pkt->jp_data, pkt->jp_len);
			if (r == 0)
				break;
			else if (r == -E_NET_TX_FULL)
				continue;
			else
				panic("network output error: %e", r);
		}
	}
}
