#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  uint64 addr;
  int checknum,bitmask;
  if(argaddr(0,&addr)<0){
    return -1;
  };
  if(argint(1,&checknum)<0){
    return -1;
  };
  if(argint(2,&bitmask)<0){
    return -1;
  }
  //设定检测数目上限
  if(checknum>32){
    return -1;
  }
  pte_t* pte;
  struct proc* cur_proc=myproc();
  int resbuf=0;//0x 0000
  for(int i=0;i<checknum;++i){
    pte=walk(cur_proc->pagetable,addr+i*PGSIZE,0);//得到pte物理地址
    //进行判断
    if((*pte & PTE_A) != 0){
      //访问位为1
      *pte=*pte&(~PTE_A);//置0
      resbuf=resbuf|(1<<i);//相应位置1
    }
  }
  copyout(cur_proc->pagetable,bitmask,(char*)&resbuf,sizeof(resbuf));
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
