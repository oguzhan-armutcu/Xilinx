/*
 * Copyright (C) 2009 - 2018 Xilinx, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 */

#include <stdio.h>

#include "xparameters.h"

#include "netif/xadapter.h"

#include "platform.h"
#include "platform_config.h"
#if defined (__arm__) || defined(__aarch64__)
#include "xil_printf.h"
#endif

#include "lwip/tcp.h"
#include "lwip/err.h"
#include "lwip/udp.h"
#include "xil_cache.h"


/* defined by each RAW mode application */
void print_app_header();
int start_application_UDP();
int start_application();
int transfer_data();
void tcp_fasttmr(void);
void tcp_slowtmr(void);

/* missing declaration in lwIP */
void lwip_init();

#if LWIP_IPV6==0
#if LWIP_DHCP==1
extern volatile int dhcp_timoutcntr;
err_t dhcp_start(struct netif *netif);
#endif
#endif

extern volatile int TcpFastTmrFlag;
extern volatile int TcpSlowTmrFlag;
static struct netif server_netif;
struct netif *echo_netif;

unsigned char data[30];
extern struct udp_pcb *MyPcb;
volatile u8		SendResults;
void
print_ip(char *msg, ip_addr_t *ip)
{
	print(msg);
	xil_printf("%d.%d.%d.%d\n\r", ip4_addr1(ip), ip4_addr2(ip),
			ip4_addr3(ip), ip4_addr4(ip));
}

void
print_ip_settings(ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw)
{

	print_ip("Board IP: ", ip);
	print_ip("Netmask : ", mask);
	print_ip("Gateway : ", gw);
}


#if defined (__arm__) && !defined (ARMR5)
#if XPAR_GIGE_PCS_PMA_SGMII_CORE_PRESENT == 1 || XPAR_GIGE_PCS_PMA_1000BASEX_CORE_PRESENT == 1
int ProgramSi5324(void);
int ProgramSfpPhy(void);
#endif
#endif

#ifdef XPS_BOARD_ZCU102
#ifdef XPAR_XIICPS_0_DEVICE_ID
int IicPhyReset(void);
#endif
#endif

void PayloadID(unsigned char *data)
//void framecounter(char data,int payloadcount)
{
	char text[16] = "Hello UDP "; // Adding Payload to Data
// static int digit[]=0;
	int i = 0, j = 0, k = 0;

	for (k = 0; text[k] != '\0'; ++k, ++j) {
		data[j] = text[k];
	}

}
int main()
{

	ip_addr_t ipaddr, netmask, gw;
	ip_addr_t pc_ipaddr;
	err_t status;
	err_t error;
	// Creation of a basic UDP Packet
	struct pbuf *MyP;
	int i = 0;
	int buflen = 30;
	int count = 0;
	u16_t Port = 4040;

	/* the mac address of the board. this should be unique per board */
	unsigned char mac_ethernet_address[] =
	{ 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };

	/* initliaze IP addresses to be used */
	IP4_ADDR(&ipaddr,  192, 168,   1, 10);
	IP4_ADDR(&netmask, 255, 255, 255,  0);
	IP4_ADDR(&gw,      192, 168,   1,  1);
	IP4_ADDR(&pc_ipaddr,  192, 168,   1, 5);

	echo_netif = &server_netif;

	init_platform();

	print_ip_settings(&ipaddr, &netmask, &gw);


	lwip_init();


	/* Add network interface to the netif_list, and set it as default */
	if (!xemac_add(echo_netif, &ipaddr, &netmask,
						&gw, mac_ethernet_address,
						PLATFORM_EMAC_BASEADDR)) {
		xil_printf("Error adding N/W interface\n\r");
		return -1;
	}

	netif_set_default(echo_netif);

	/* specify that the network if is up */
	netif_set_up(echo_netif);

	/* now enable interrupts */
	platform_enable_interrupts();

	/* start the application (web server, rxtest, txtest, etc.) */
	xil_printf("Setup Done");


	xil_printf("UDP starting...\n");

	if ( start_application_UDP() != ERR_OK ){
		return 0;
	}
	while (1) {
		if (TcpFastTmrFlag) {
			tcp_fasttmr();
			TcpFastTmrFlag = 0;
			//SendResults = 1;
		}
		if (TcpSlowTmrFlag) {
			tcp_slowtmr();
			TcpSlowTmrFlag = 0;
			SendResults = 1;

		}
		xemacif_input(echo_netif);

		if(SendResults == 1){

			SendResults = 0;
			error = udp_connect(MyPcb, &pc_ipaddr, Port);
			if (error != 0) {
				xil_printf("Failed %d\r\n", error);
				return 0;
			}
			usleep(1000);

			MyP = pbuf_alloc(PBUF_TRANSPORT, buflen, PBUF_REF);

			if (!MyP) {
				xil_printf("error allocating pbuf \r\n");
				return ERR_MEM;
			}

			PayloadID(data);

			memcpy(MyP->payload, data, buflen);

			status = udp_send(MyPcb, MyP);
			if ( status != ERR_OK ){
				xil_printf("\n ERROR SEND UDP = %d \r\n", status);
			}
			udp_disconnect(MyPcb);

			usleep(50000);

			pbuf_free(MyP);
		}

	}


	return 0;
}
