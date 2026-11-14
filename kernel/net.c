#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

// xv6's ethernet MAC and IP addresses.
// these are determined by qemu.
static uint8 local_mac[ETHADDR_LEN] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static uint32 local_ip = MAKE_IP_ADDR(10, 0, 2, 15);

// qemu host's ethernet address.
static uint8 host_mac[ETHADDR_LEN] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };

static struct spinlock netlock;
#define NPORT 128
#define NPACKET 16
// Metadata occupies discarded headers, not the UDP payload.
struct packet {
  struct packet *next;
  uint32 src;
  uint16 sport, len, offset;
};
static struct port {
  int bound, number, count;
  uint64 generation;
  struct packet *head, *tail;
} ports[NPORT];

static struct port *
findport(int number)
{
  for(int i = 0; i < NPORT; i++)
    if(ports[i].bound && ports[i].number == number) return &ports[i];
  return 0;
}

void
netinit(void)
{
  initlock(&netlock, "netlock");
}


//
// bind(int port)
// prepare to receive UDP packets address to the port,
// i.e. allocate any queues &c needed.
//
uint64
sys_bind(void)
{
  int number;
  argint(0, &number);
  if(number < 0 || number > 65535) return -1;
  acquire(&netlock);
  if(findport(number)){
    release(&netlock);
    return -1;
  }
  for(int i = 0; i < NPORT; i++){
    if(!ports[i].bound){
      struct port *q = &ports[i];
      q->bound = 1;
      q->number = number;
      q->generation++;
      q->head = q->tail = 0;
      q->count = 0;
      release(&netlock);
      return 0;
    }
  }
  release(&netlock);
  return -1;
}

//
// unbind(int port)
// release any resources previously created by bind(port);
// from now on UDP packets addressed to port should be dropped.
//
uint64
sys_unbind(void)
{
  int number;
  argint(0, &number);
  acquire(&netlock);
  struct port *q = findport(number);
  if(!q){ release(&netlock); return -1; }
  struct packet *pkt = q->head;
  while(pkt){
    struct packet *next = pkt->next;
    kfree((char *)pkt);
    pkt = next;
  }
  q->head = q->tail = 0;
  q->count = 0;
  q->bound = 0;
  q->generation++;
  wakeup(q);
  release(&netlock);
  return 0;
}

//
// recv(int dport, int *src, short *sport, char *buf, int maxlen)
// if there's a received UDP packet already queued that was
// addressed to dport, then return it.
// otherwise wait for such a packet.
//
// sets *src to the IP source address.
// sets *sport to the UDP source port.
// copies up to maxlen bytes of UDP payload to buf.
// returns the number of bytes copied,
// and -1 if there was an error.
//
// dport, *src, and *sport are host byte order.
// bind(dport) must previously have been called.
//
uint64
sys_recv(void)
{
  int number, maxlen;
  uint64 srcaddr, sportaddr, bufaddr;
  argint(0, &number);
  argaddr(1, &srcaddr);
  argaddr(2, &sportaddr);
  argaddr(3, &bufaddr);
  argint(4, &maxlen);
  if(maxlen < 0) return -1;
  struct proc *p = myproc();
  acquire(&netlock);
  struct port *q = findport(number);
  if(!q){ release(&netlock); return -1; }
  uint64 generation = q->generation;
  while(q->head == 0){
    if(killed(p) || !q->bound || q->generation != generation){
      release(&netlock);
      return -1;
    }
    sleep_prepare(q);
    release(&netlock);
    sleep();
    acquire(&netlock);
  }
  // Unbind/rebind may have happened while this receiver was asleep.
  if(!q->bound || q->generation != generation){
    release(&netlock);
    return -1;
  }
  struct packet *pkt = q->head;
  q->head = pkt->next;
  if(!q->head) q->tail = 0;
  q->count--;
  release(&netlock);
  int n = pkt->len < maxlen ? pkt->len : maxlen;
  int failed = copyout(p->pagetable, p->sz, srcaddr, (char *)&pkt->src, sizeof(pkt->src)) < 0 ||
    copyout(p->pagetable, p->sz, sportaddr, (char *)&pkt->sport, sizeof(pkt->sport)) < 0 ||
    copyout(p->pagetable, p->sz, bufaddr, (char *)pkt + pkt->offset, n) < 0;
  kfree((char *)pkt);
  return failed ? -1 : n;
}

// This code is lifted from FreeBSD's ping.c, and is copyright by the Regents
// of the University of California.
static unsigned short
in_cksum(const unsigned char *addr, int len)
{
  int nleft = len;
  const unsigned short *w = (const unsigned short *)addr;
  unsigned int sum = 0;
  unsigned short answer = 0;

  /*
   * Our algorithm is simple, using a 32 bit accumulator (sum), we add
   * sequential 16 bit words to it, and at the end, fold back all the
   * carry bits from the top 16 bits into the lower 16 bits.
   */
  while (nleft > 1)  {
    sum += *w++;
    nleft -= 2;
  }

  /* mop up an odd byte, if necessary */
  if (nleft == 1) {
    *(unsigned char *)(&answer) = *(const unsigned char *)w;
    sum += answer;
  }

  /* add back carry outs from top 16 bits to low 16 bits */
  sum = (sum & 0xffff) + (sum >> 16);
  sum += (sum >> 16);
  /* guaranteed now that the lower 16 bits of sum are correct */

  answer = ~sum; /* truncate to 16 bits */
  return answer;
}

//
// send(int sport, int dst, int dport, char *buf, int len)
//
uint64
sys_send(void)
{
  struct proc *p = myproc();
  int sport;
  int dst;
  int dport;
  uint64 bufaddr;
  int len;

  argint(0, &sport);
  argint(1, &dst);
  argint(2, &dport);
  argaddr(3, &bufaddr);
  argint(4, &len);

  if(len < 0 || len > PGSIZE - sizeof(struct eth) - sizeof(struct ip) - sizeof(struct udp)) return -1;
  int total = len + sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp);
  if(total > PGSIZE)
    return -1;

  char *buf = kalloc();
  if(buf == 0){
    printk("sys_send: kalloc failed\n");
    return -1;
  }
  memset(buf, 0, PGSIZE);

  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, host_mac, ETHADDR_LEN);
  memmove(eth->shost, local_mac, ETHADDR_LEN);
  eth->type = htons(ETHTYPE_IP);

  struct ip *ip = (struct ip *)(eth + 1);
  ip->ip_vhl = 0x45; // version 4, header length 4*5
  ip->ip_tos = 0;
  ip->ip_len = htons(sizeof(struct ip) + sizeof(struct udp) + len);
  ip->ip_id = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 100;
  ip->ip_p = IPPROTO_UDP;
  ip->ip_src = htonl(local_ip);
  ip->ip_dst = htonl(dst);
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(*ip));

  struct udp *udp = (struct udp *)(ip + 1);
  udp->sport = htons(sport);
  udp->dport = htons(dport);
  udp->ulen = htons(len + sizeof(struct udp));

  char *payload = (char *)(udp + 1);
  if(copyin(p->pagetable, p->sz, payload, bufaddr, len) < 0){
    kfree(buf);
    printk("send: copyin failed\n");
    return -1;
  }

  tulip_xmit(buf, total);

  return 0;
}

void
ip_rx(char *buf, int len)
{
  // don't delete this printk; make grade depends on it.
  static int seen_ip = 0;
  if(seen_ip == 0)
    printk("ip_rx: received an IP packet\n");
  seen_ip = 1;

  printk("."); // help Tulip receive debugging; delete if you want to.

  struct ip *ip = (struct ip *)(buf + sizeof(struct eth));
  int ihl = (ip->ip_vhl & 15) * 4;
  int iplen = ntohs(ip->ip_len);
  if((ip->ip_vhl >> 4) != 4 || ihl < sizeof(*ip) ||
     iplen < ihl + sizeof(struct udp) || iplen > len - sizeof(struct eth) ||
     ip->ip_p != IPPROTO_UDP || (ntohs(ip->ip_off) & 0x3fff) ||
     ntohl(ip->ip_dst) != local_ip){
    kfree(buf);
    return;
  }
  struct udp *udp = (struct udp *)((char *)ip + ihl);
  int ulen = ntohs(udp->ulen);
  if(ulen < sizeof(*udp) || ulen > iplen - ihl){ kfree(buf); return; }
  uint32 src = ntohl(ip->ip_src);
  uint16 sport = ntohs(udp->sport);
  int dport = ntohs(udp->dport);
  int offset = (char *)(udp+1) - buf;
  acquire(&netlock);
  struct port *q = findport(dport);
  if(!q || q->count == NPACKET){
    release(&netlock);
    kfree(buf);
    return;
  }
  struct packet *pkt = (struct packet *)buf;
  pkt->next = 0;
  pkt->src = src;
  pkt->sport = sport;
  pkt->len = ulen - sizeof(*udp);
  pkt->offset = offset;
  if(q->tail) q->tail->next = pkt;
  else q->head = pkt;
  q->tail = pkt;
  q->count++;
  wakeup(q);
  release(&netlock);
}

//
// send an ARP reply packet to tell qemu to map
// xv6's ip address to its ethernet address.
// this is the bare minimum needed to persuade
// qemu to send IP packets to xv6; the real ARP
// protocol is more complex.
//
void
arp_rx(char *inbuf)
{
  static int seen_arp = 0;

  if(seen_arp){
    kfree(inbuf);
    return;
  }
  printk("arp_rx: received an ARP packet\n");
  seen_arp = 1;

  struct eth *ineth = (struct eth *) inbuf;
  struct arp *inarp = (struct arp *) (ineth + 1);

  char *buf = kalloc();
  if(buf == 0)
    panic("send_arp_reply");
  
  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, ineth->shost, ETHADDR_LEN); // ethernet destination = query source
  memmove(eth->shost, local_mac, ETHADDR_LEN); // ethernet source = xv6's ethernet address
  eth->type = htons(ETHTYPE_ARP);

  struct arp *arp = (struct arp *)(eth + 1);
  arp->hrd = htons(ARP_HRD_ETHER);
  arp->pro = htons(ETHTYPE_IP);
  arp->hln = ETHADDR_LEN;
  arp->pln = sizeof(uint32);
  arp->op = htons(ARP_OP_REPLY);

  memmove(arp->sha, local_mac, ETHADDR_LEN);
  arp->sip = htonl(local_ip);
  memmove(arp->tha, ineth->shost, ETHADDR_LEN);
  arp->tip = inarp->sip;

  tulip_xmit(buf, sizeof(*eth) + sizeof(*arp));

  kfree(inbuf);
}

//
// the tulip driver should call net_rx() for each
// packet received. net_rx() always calls kfree(buf).
//
void
net_rx(char *buf, int len)
{
  if(len < sizeof(struct eth)){ kfree(buf); return; }
  struct eth *eth = (struct eth *) buf;

  if(len >= sizeof(struct eth) + sizeof(struct arp) &&
     ntohs(eth->type) == ETHTYPE_ARP){
    arp_rx(buf);
  } else if(len >= sizeof(struct eth) + sizeof(struct ip) &&
     ntohs(eth->type) == ETHTYPE_IP){
    ip_rx(buf, len);
  } else {
    printk("net_rx: unknown ether type %x\n", ntohs(eth->type));
    kfree(buf);
  }
}
