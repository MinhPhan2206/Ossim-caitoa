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
#include <stdio.h>
#include <pthread.h>

#ifdef MM64
#include "mm64.h"
#else
#include "mm.h"
#endif

static pthread_mutex_t sys_mem_lock = PTHREAD_MUTEX_INITIALIZER;

int __sys_memmap(struct krnl_t *krnl, uint32_t pid, struct sc_regs* regs)
{
   int memop = regs->a1;
   BYTE value;
   struct pcb_t *caller = NULL;
   int ret = 0;

   pthread_mutex_lock(&sys_mem_lock);

   /* * Bảo mật không gian nhớ: User process không được truy cập trực tiếp PCB.
    * Kernel phải tự quét trong running_list để tìm ra tiến trình gọi system call (caller).
    */
   struct queue_t *running_list = krnl->running_list;
   if (running_list != NULL) {
       for (int i = 0; i < running_list->size; i++) {
           if (running_list->proc[i] != NULL && running_list->proc[i]->pid == pid) {
               caller = running_list->proc[i];
               break;
           }
       }
   }

   /* Nếu không tìm thấy tiến trình hợp lệ, chặn thực thi để tránh crash (Segfault) */
   if (caller == NULL) {
       printf("[ERROR] sys_memmap: PID %d not found in running_list!\n", pid);
	   pthread_mutex_unlock(&sys_mem_lock);
       return -1;
   }
	
   switch (memop) {
   case SYSMEM_MAP_OP:
            /* Reserved process case*/
			vmap_pgd_memset(caller, regs->a2, regs->a3);
            break;
   case SYSMEM_INC_OP:
            inc_vma_limit(caller, regs->a2, regs->a3);
            break;
   case SYSMEM_SWP_OP:
            __mm_swap_page(caller, regs->a2, regs->a3);
            break;
   case SYSMEM_IO_READ:
            MEMPHY_read(caller->krnl->mram, regs->a2, &value);
            regs->a3 = value;
            break;
   case SYSMEM_IO_WRITE:
            MEMPHY_write(caller->krnl->mram, regs->a2, regs->a3);
            break;
   default:
            printf("Memop code: %d\n", memop);
            break;
   }

   pthread_mutex_unlock(&sys_mem_lock);
   return 0;
}
