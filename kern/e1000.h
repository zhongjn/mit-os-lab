#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>
#include <kern/e1000_regs.h>

#define E1000_VENDOR 0x8086
#define E1000_PRODUCT 0x100e
#define PACKET_BUFFER_SIZE 2048

int e1000_attach(struct pci_func* pcif);
int e1000_try_send(void* addr, size_t size);
int e1000_recv(void* buf);

extern int e1000_irq;
void e1000_intr();

#endif  // SOL >= 6
