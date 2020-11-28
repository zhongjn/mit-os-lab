#include <kern/e1000.h>
#include <kern/pmap.h>
#include <kern/pci.h>
#include <inc/error.h>
#include <inc/string.h>

// LAB 6: Your driver code here

#define LOG_PREFIX "e1000 driver: "
#define E1000_REG(addr) e1000_bar0[addr/4]

#define TX_RING_SIZE 32
#define RX_RING_SIZE 128
#define BITMASK_ACTUAL(mask,value) (value * ((mask) & ~((mask) << 1)))

volatile uint32_t* e1000_bar0;
struct e1000_tx_desc tx_ring[TX_RING_SIZE] __attribute__ ((aligned (16)));
struct e1000_rx_desc rx_ring[RX_RING_SIZE] __attribute__ ((aligned (16)));
uint8_t tx_buffer[TX_RING_SIZE][PACKET_BUFFER_SIZE];
uint8_t rx_buffer[RX_RING_SIZE][PACKET_BUFFER_SIZE];

int tdt;
int rdt, rdh;

static void init_tx_ring();
static void init_rx_ring();

int e1000_attach(struct pci_func* pcif) {
    pci_func_enable(pcif);
    e1000_bar0 = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
    cprintf(LOG_PREFIX "status=0x%08x\n", E1000_REG(E1000_STATUS));

    // init transmit descriptor array
    E1000_REG(E1000_TDBAH) = 0;
    E1000_REG(E1000_TDBAL) = PADDR(tx_ring);
    E1000_REG(E1000_TDLEN) = sizeof(tx_ring);

    // init transmit descriptor head & tail
    E1000_REG(E1000_TDH) = 0;
    tdt = E1000_REG(E1000_TDT) = 0;

    // init transmit inter packet grap
    E1000_REG(E1000_TIPG) = 10;

    init_tx_ring();

    // init transmit control register
    E1000_REG(E1000_TCTL) = E1000_TCTL_EN | E1000_TCTL_PSP | BITMASK_ACTUAL(E1000_TCTL_COLD, 0x40);

    // multicast table array
    E1000_REG(E1000_MTA) = 0;

    // init receive descriptor array
    E1000_REG(E1000_RDBAH) = 0;
    E1000_REG(E1000_RDBAL) = PADDR(rx_ring);
    E1000_REG(E1000_RDLEN) = sizeof(rx_ring);

    // init receive address
    volatile uint32_t* ra_reg = &E1000_REG(E1000_RA);
    uint8_t ra[] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56, 0x00, 0x00 };
    ra_reg[0] = ((uint32_t*)ra)[0];
    ra_reg[1] = ((uint32_t*)ra)[1] | E1000_RAH_AV;

    // init receive descriptor head & tail
    rdh = E1000_REG(E1000_RDH) = 0;
    rdt = E1000_REG(E1000_RDT) = RX_RING_SIZE - 1;

    init_rx_ring();

    // init receive control register
    E1000_REG(E1000_RCTL) = E1000_RCTL_EN | E1000_RCTL_SECRC | E1000_RCTL_SZ_2048 | E1000_RCTL_BAM;

    return 1;
}

static void init_tx_ring() {
    for (int i = 0; i < TX_RING_SIZE; i++) {
        tx_ring[i].buffer_addr = PADDR(tx_buffer[i]);
        tx_ring[i].lower.data = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;
        tx_ring[i].upper.data = E1000_TXD_STAT_DD;
    }
}

// Try send a packet
// Return 0 on success, -E_NET_TX_FULL on failure
int e1000_try_send(void* addr, size_t size) {
    assert(size <= PACKET_BUFFER_SIZE);

    struct e1000_tx_desc* desc = &tx_ring[tdt];
    if (desc->upper.data & E1000_TXD_STAT_DD) {
        memcpy(tx_buffer[tdt], addr, size);
        desc->lower.flags.length = size;
        desc->upper.fields.status = 0;

        tdt = (tdt + 1) % TX_RING_SIZE;
        E1000_REG(E1000_TDT) = tdt;
        return 0;
    }
    else {
        return -E_NET_TX_FULL;
    }
}

static void init_rx_ring() {
    for (int i = 0; i < RX_RING_SIZE; i++) {
        rx_ring[i].buffer_addr = PADDR(rx_buffer[i]);
    }
}

// Receive a packet, buf's size should be at least PACKET_BUFFER_SIZE
// Return packet size (>0) on success, -E_NET_RX_EMPTY on failure
int e1000_recv(void* buf) {
    struct e1000_rx_desc* desc = &rx_ring[rdh];
    if (desc->status & E1000_RXD_STAT_DD) {
        assert(desc->status & E1000_RXD_STAT_EOP);
        assert(desc->length <= PACKET_BUFFER_SIZE);

        // copy out data
        int sz = desc->length;
        memcpy(buf, rx_buffer[rdh], sz);

        // restore descriptor & update tail
        desc->status = 0;
        rdt = rdh;
        E1000_REG(E1000_RDT) = rdt;

        // update head (sync with hw)
        rdh = (rdh + 1) % RX_RING_SIZE;
        return sz;
    }
    else {
        return -E_NET_RX_EMPTY;
    }
}