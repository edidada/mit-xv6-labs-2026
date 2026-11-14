// DEC 21143 Tulip: four-word descriptors, CSR registers eight bytes apart.
// Reference: EC-QWC4D-TE sections 3.2, 4.2 and 4.3.
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

#define NRING 16
#define OWN (1U << 31)
#define END (1U << 25)
#define RXBUF 1020
struct descriptor { volatile uint32 status, control, address, second; };
static struct descriptor tx[NRING] __attribute__((aligned(16)));
static struct descriptor rx[NRING] __attribute__((aligned(16)));
static char *txbuf[NRING], *rxbuf[NRING];
static volatile uint64 *csr;
static struct spinlock txlock, rxlock;
static int txnext, rxnext;

// Caller holds txlock. An idle ring must not retain transmitted pages.
static void
reclaim(void)
{
  for(int i = 0; i < NRING; i++){
    if(txbuf[i] && !(tx[i].status & OWN)){
      __sync_synchronize();
      kfree(txbuf[i]);
      txbuf[i] = 0;
    }
  }
}

void
tulip_init(volatile uint64 *csrs)
{
  csr = csrs;
  initlock(&txlock, "tulip tx");
  initlock(&rxlock, "tulip rx");
  csr[6] = 0;
  csr[7] = 0;
  for(int i = 0; i < NRING; i++){
    rxbuf[i] = kalloc();
    if(!rxbuf[i]) panic("tulip buffers");
    rx[i].address = (uint64)rxbuf[i];
    rx[i].control = RXBUF | (i == NRING-1 ? END : 0);
    rx[i].status = OWN;
  }
  csr[3] = (uint64)rx;
  csr[4] = (uint64)tx;
  csr[5] = 0xffffffff;
  __sync_synchronize();
  // Normal/abnormal summaries, receive, receive unavailable, transmit.
  csr[7] = (1<<16) | (1<<15) | (1<<7) | (1<<6) | 1;
  csr[6] = (1<<25) | (1<<13) | (1<<6) | (1<<1);
  csr[2] = 1;
}

void
tulip_xmit(char *buf, int len)
{
  acquire(&txlock);
  reclaim();
  int needed = (len + 2046) / 2047;
  if(len <= 0 || len > PGSIZE){
    release(&txlock);
    kfree(buf);
    return;
  }
  for(int j = 0; j < needed; j++){
    if(tx[(txnext+j)%NRING].status & OWN){
      release(&txlock);
      kfree(buf);
      return;
    }
  }
  int offset = 0;
  for(int j = 0; j < needed; j++){
    int i = (txnext+j)%NRING;
    int n = len-offset > 2047 ? 2047 : len-offset;
    // Only the final descriptor owns the allocation. Completion is in order.
    txbuf[i] = j == needed-1 ? buf : 0;
    tx[i].address = (uint64)(buf+offset);
    tx[i].second = 0;
    tx[i].control = n | (i == NRING-1 ? END : 0) |
      (j == 0 ? (1<<29) : 0) |
      (j == needed-1 ? (1U<<31) | (1<<30) : 0);
    offset += n;
  }
  __sync_synchronize();
  // Publish the first descriptor last: hardware cannot see a partial chain.
  for(int j = needed-1; j >= 0; j--)
    tx[(txnext+j)%NRING].status = OWN;
  __sync_synchronize();
  txnext = (txnext+needed)%NRING;
  csr[1] = 1;
  release(&txlock);
}

void
tulip_intr(void)
{
  // Serialize receive delivery across harts. net_rx can transmit ARP replies;
  // it acquires txlock, never rxlock, so there is no recursive driver lock.
  acquire(&rxlock);
  csr[5] = csr[5]; // write-one-to-clear before draining; new arrivals can reassert
  acquire(&txlock);
  reclaim();
  release(&txlock);
  while(!(rx[rxnext].status & OWN)){
    int i = rxnext;
    uint32 status = rx[i].status;
    __sync_synchronize();
    char *buf = rxbuf[i];
    char *replacement = kalloc();
    int len = (status >> 16) & 0x3fff; // includes four CRC bytes
    if(replacement){
      rxbuf[i] = replacement;
      rx[i].address = (uint64)replacement;
    }
    __sync_synchronize();
    rx[i].status = OWN;
    __sync_synchronize();
    rxnext = (i+1)%NRING;
    if(replacement){
      if(!(status & (1<<15)) && (status & 0x300) == 0x300 && len >= 4 && len <= RXBUF)
        net_rx(buf, len-4);
      else
        kfree(buf);
    } // OOM: drop contents and return the original buffer to DMA.
  }
  csr[2] = 1;
  release(&rxlock);
}
