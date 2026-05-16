/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

#include "os-cfg.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>

/* Tìm VMA dựa vào ID */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid) {
    struct vm_area_struct *pvma = mm->mmap;
    if (pvma == NULL) return NULL;
    while (pvma != NULL) {
        if (pvma->vm_id == vmaid) return pvma;
        pvma = pvma->vm_next;
    }
    return NULL;
}

/* Lấy node vùng nhớ mới tại vị trí break pointer */
struct vm_rg_struct *get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, addr_t size, addr_t alignedsz) {
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);
    if (cur_vma == NULL) return NULL;
    struct vm_rg_struct *newrg = malloc(sizeof(struct vm_rg_struct));
    newrg->rg_start = cur_vma->sbrk;
    newrg->rg_end = newrg->rg_start + size;
    newrg->vmaid = vmaid;
    newrg->rg_next = NULL;
    return newrg;
}

/* Kiểm tra xem vùng nhớ dự kiến có bị chồng lấp với VMA hiện có không */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, addr_t vmastart, addr_t vmaend) {
    if (vmastart >= vmaend) return -1;
    struct vm_area_struct *vma = caller->krnl->mm->mmap;
    struct vm_area_struct *cur_area = get_vma_by_num(caller->krnl->mm, vmaid);
    if (cur_area == NULL) return -1;
    while (vma != NULL) {
        if (vma != cur_area) {
            // Điều kiện giao thoa
            if (vmastart < vma->vm_end && vmaend > vma->vm_start) return -1;
        }
        vma = vma->vm_next;
    }
    return 0;
}

/* Tăng giới hạn VMA và ánh xạ thêm RAM vật lý */
int inc_vma_limit(struct pcb_t *caller, int vmaid, addr_t inc_sz) {
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);
    if (cur_vma == NULL) return -1;
    addr_t inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
    int incnumpage = inc_amt / PAGING_PAGESZ;
    addr_t old_end = cur_vma->vm_end;
    addr_t new_end = old_end + inc_amt;
    if (validate_overlap_vm_area(caller, vmaid, cur_vma->vm_start, new_end) < 0) return -1;
    struct vm_rg_struct *newrg = malloc(sizeof(struct vm_rg_struct));
    if (vm_map_ram(caller, cur_vma->vm_start, new_end, old_end, incnumpage, newrg) < 0) {
        free(newrg);
        return -1;
    }
    cur_vma->vm_end = new_end;
    cur_vma->sbrk = new_end;
    enlist_vm_rg_node(&cur_vma->vm_freerg_list, newrg);
    return 0;
}

/* Thực thi việc chuyển (Swap) giữa Frame RAM và SWAP */
int __mm_swap_page(struct pcb_t *caller, addr_t vicfpn , addr_t swpfpn) {
    __swap_cp_page(caller->krnl->mram, vicfpn, caller->krnl->active_mswp, swpfpn);
    return 0;
}

/* Các hàm hỗ trợ quản lý node danh sách liên kết */
struct vm_rg_struct *init_vm_rg(addr_t rg_start, addr_t rg_end) {
    struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));
    rgnode->rg_start = rg_start;
    rgnode->rg_end = rg_end;
    rgnode->vmaid = 0;
    rgnode->rg_next = NULL;
    return rgnode;
}

int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct *rgnode) {
    rgnode->rg_next = *rglist;
    *rglist = rgnode;
    return 0;
}

int enlist_pgn_node(struct pgn_t **plist, addr_t pgn) {
    struct pgn_t *pnode = malloc(sizeof(struct pgn_t));
    pnode->pgn = pgn;
    pnode->pg_next = *plist;
    *plist = pnode;
    return 0;
}