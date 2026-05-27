/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Caitoa release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "os-mm.h"
#include "syscall.h"
#include "libmem.h"
#include "queue.h"
#include <stdlib.h>
#include <pthread.h>

#ifdef MM64
#include "mm64.h"
#else
#include "mm.h"
#endif

//typedef char BYTE;
static pthread_mutex_t sys_mem_lock = PTHREAD_MUTEX_INITIALIZER;
int __sys_memmap(struct krnl_t *krnl, uint32_t pid, struct sc_regs* regs)
{
   int memop = regs->a1;
   BYTE value;
   
   /* TODO THIS DUMMY CREATE EMPTY PROC TO AVOID COMPILER NOTIFY 
    *      need to be eliminated
	*/
   int ret = 0;
   pthread_mutex_lock(&sys_mem_lock);
   struct pcb_t *caller = find_pcb_by_pid(krnl, pid);
   if (caller == NULL) {
       printf("__sys_memmap: Process %d not found\n", pid);
       pthread_mutex_unlock(&sys_mem_lock);
       return -1;
   }
   
   /* Set kernel pointer for the caller */
   caller->krnl = krnl;
   /*
    * @bksysnet: Please note in the dual spacing design
    *            syscall implementations are in kernel space.
    */

   /* TODO: Traverse proclist to terminate the proc
    *       stcmp to check the process match proc_name
    */
//	struct queue_t *running_list = krnl->running_list;

    /* TODO Maching and marking the process */
    /* user process are not allowed to access directly pcb in kernel space of syscall */
    //....
	
   switch (memop) {
   case SYSMEM_MAP_OP:
            /* Reserved process case*/
			ret = vmap_pgd_memset(caller, regs->a2, regs->a3);
            break;
   case SYSMEM_INC_OP:
            ret = inc_vma_limit(caller, regs->a2, regs->a3);
            break;
   case SYSMEM_SWP_OP:
            ret = __mm_swap_page(caller, regs->a2, regs->a3);
            break;
   case SYSMEM_IO_READ:
            ret = MEMPHY_read(caller->krnl->mram, regs->a2, &value);
            regs->a3 = value;
            break;
   case SYSMEM_IO_WRITE:
            ret = MEMPHY_write(caller->krnl->mram, regs->a2, (BYTE)regs->a3);
            break;
   default:
            printf("__sys_memmap: Unknown memop %d\n", memop);
            ret = -1;
            break;
   }
   pthread_mutex_unlock(&sys_mem_lock);
   return ret;
}


