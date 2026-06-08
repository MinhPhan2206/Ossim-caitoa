/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

#include "os-cfg.h"
#include "mm64.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined(MM64)

int total_allocated_pagetables = 1;

int init_pte(addr_t *pte, int pre, addr_t fpn, int drt, int swp, int swptyp, addr_t swpoff) {
    if (pre != 0) {
        if (swp == 0) {
            if (fpn == 0) return -1;
            SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
            CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
            CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);
            SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
        } else {
            SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
            SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
            CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);
            SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
            SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
        }
    }
    return 0;
}

int get_pd_from_address(addr_t addr, addr_t* pgd, addr_t* p4d, addr_t* pud, addr_t* pmd, addr_t* pt) {
    *pgd = PAGING64_ADDR_PGD(addr);
    *p4d = PAGING64_ADDR_P4D(addr);
    *pud = PAGING64_ADDR_PUD(addr);
    *pmd = PAGING64_ADDR_PMD(addr);
    *pt  = PAGING64_ADDR_PT(addr);
    return 0;
}

int get_pd_from_pagenum(addr_t pgn, addr_t* pgd, addr_t* p4d, addr_t* pud, addr_t* pmd, addr_t* pt) {
    return get_pd_from_address(pgn << PAGING64_ADDR_PT_SHIFT, pgd, p4d, pud, pmd, pt);
}

/* Thuật toán Sparse Allocation để chỉ tạo bảng khi cần */
static addr_t* walk_page_table(struct pcb_t *caller, addr_t pgn) {
    addr_t pgd_idx, p4d_idx, pud_idx, pmd_idx, pt_idx;
    get_pd_from_pagenum(pgn, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx);

    addr_t *pgd = caller->krnl->mm->pgd;
    if (pgd[pgd_idx] == 0) {
        pgd[pgd_idx] = (addr_t)calloc(512, sizeof(addr_t));
        total_allocated_pagetables++; // Tăng biến đếm
    }
    addr_t *p4d = (addr_t *)pgd[pgd_idx];

    if (p4d[p4d_idx] == 0) {
        p4d[p4d_idx] = (addr_t)calloc(512, sizeof(addr_t));
        total_allocated_pagetables++; // Tăng biến đếm
    }
    addr_t *pud = (addr_t *)p4d[p4d_idx];

    if (pud[pud_idx] == 0) {
        pud[pud_idx] = (addr_t)calloc(512, sizeof(addr_t));
        total_allocated_pagetables++; // Tăng biến đếm
    }
    addr_t *pmd = (addr_t *)pud[pud_idx];

    if (pmd[pmd_idx] == 0) {
        pmd[pmd_idx] = (addr_t)calloc(512, sizeof(addr_t));
        total_allocated_pagetables++; // Tăng biến đếm
    }
    addr_t *pt = (addr_t *)pmd[pmd_idx];

    return &pt[pt_idx];
}

int pte_set_swap(struct pcb_t *caller, addr_t pgn, int swptyp, addr_t swpoff) {
    addr_t *pte = walk_page_table(caller, pgn);
    SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
    SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
    SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
    SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
    return 0;
}

int pte_set_fpn(struct pcb_t *caller, addr_t pgn, addr_t fpn) {
    addr_t *pte = walk_page_table(caller, pgn);
    SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
    CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
    SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
    return 0;
}

uint32_t pte_get_entry(struct pcb_t *caller, addr_t pgn) {
    return (uint32_t)*walk_page_table(caller, pgn);
}

int pte_set_entry(struct pcb_t *caller, addr_t pgn, uint32_t pte_val) {
    addr_t *pte = walk_page_table(caller, pgn);
    *pte = pte_val;
    return 0;
}

int vmap_pgd_memset(struct pcb_t *caller, addr_t addr, int pgnum) {
    uint64_t pattern = 0xdeadbeef;
    for (int pgit = 0; pgit < pgnum; pgit++) {
        /* FIXED MACRO WARNING bằng cách bọc () */
        addr_t pgn = PAGING_PGN((addr + pgit * PAGING64_PAGESZ));
        addr_t *pte = walk_page_table(caller, pgn);
        *pte = pattern;
    }
    return 0;
}

addr_t vmap_page_range(struct pcb_t *caller, addr_t addr, int pgnum, 
                    struct framephy_struct *frames, struct vm_rg_struct *ret_rg) {
    struct framephy_struct *fpit = frames;
    ret_rg->rg_start = addr;
    ret_rg->rg_end = addr + pgnum * PAGING64_PAGESZ;
    ret_rg->vmaid = 0; 
    for (int pgit = 0; pgit < pgnum; pgit++) {
        /* FIXED MACRO WARNING */
        addr_t pgn = PAGING_PGN((addr + pgit * PAGING64_PAGESZ));
        if (fpit != NULL) {
            pte_set_fpn(caller, pgn, fpit->fpn);
            enlist_pgn_node(&caller->krnl->mm->fifo_pgn, pgn);
            fpit = fpit->fp_next;
        }
    }
    return 0;
}

addr_t alloc_pages_range(struct pcb_t *caller, int req_pgnum, struct framephy_struct **frm_lst) {
    addr_t fpn;
    struct framephy_struct *head = NULL;
    struct framephy_struct *tail = NULL;
    for (int pgit = 0; pgit < req_pgnum; pgit++) {
        if (MEMPHY_get_freefp(caller->krnl->mram, &fpn) == 0) {
            struct framephy_struct *newfp_str = malloc(sizeof(struct framephy_struct));
            newfp_str->fpn = fpn;
            newfp_str->fp_next = NULL;
            if (head == NULL) {
                head = newfp_str;
                tail = newfp_str;
            } else {
                tail->fp_next = newfp_str;
                tail = newfp_str;
            }
        } else {
            return -3000;
        }
    }
    *frm_lst = head;
    return 0;
}

addr_t vm_map_ram(struct pcb_t *caller, addr_t astart, addr_t aend, addr_t mapstart, int incpgnum, struct vm_rg_struct *ret_rg) {
    struct framephy_struct *frm_lst = NULL;
    addr_t ret_alloc = alloc_pages_range(caller, incpgnum, &frm_lst);
    if (ret_alloc < 0 && ret_alloc != -3000) return -1;
    if (ret_alloc == -3000) return -1;
    vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg);
    return 0;
}

int __swap_cp_page(struct memphy_struct *mpsrc, addr_t srcfpn, struct memphy_struct *mpdst, addr_t dstfpn) {
    int cellidx;
    addr_t addrsrc, addrdst;
    for (cellidx = 0; cellidx < PAGING_PAGESZ; cellidx++) {
        addrsrc = srcfpn * PAGING_PAGESZ + cellidx;
        addrdst = dstfpn * PAGING_PAGESZ + cellidx;
        BYTE data;
        MEMPHY_read(mpsrc, addrsrc, &data);
        MEMPHY_write(mpdst, addrdst, data);
    }
    return 0;
}

int init_mm(struct mm_struct *mm, struct pcb_t *caller) {
    struct vm_area_struct *vma0 = malloc(sizeof(struct vm_area_struct));
    mm->pgd = calloc(512, sizeof(addr_t));
    vma0->vm_id = 0;
    vma0->vm_start = 0;
    vma0->vm_end = vma0->vm_start;
    vma0->sbrk = vma0->vm_start;
    struct vm_rg_struct *first_rg = init_vm_rg(vma0->vm_start, vma0->vm_end);
    enlist_vm_rg_node(&vma0->vm_freerg_list, first_rg);
    vma0->vm_next = NULL;
    vma0->vm_mm = mm; 
    mm->mmap = vma0;
    return 0;
}

/* Dummy Functions để thỏa mãn trình Linker và phục vụ in Debug */
int print_list_fp(struct framephy_struct *ifp) { return 0; }
int print_list_rg(struct vm_rg_struct *irg) { return 0; }
int print_list_vma(struct vm_area_struct *ivma) { return 0; }
int print_list_pgn(struct pgn_t *ip) { return 0; }

int print_pgtbl(struct pcb_t *caller, addr_t start, addr_t end) {
    if (caller == NULL || caller->krnl == NULL || caller->krnl->mm == NULL) return -1;
    
    printf("\n--- PAGE TABLE DUMP OF PROCESS %d ---\n", caller->pid);
    
    struct vm_area_struct *vma = caller->krnl->mm->mmap;
    if (vma == NULL) return -1;

    addr_t max_addr = vma->sbrk;
    int max_pgn = (max_addr / PAGING64_PAGESZ) + 1;
    
    for (int pgn = 0; pgn < max_pgn; pgn++) {
        uint32_t pte = pte_get_entry(caller, pgn);
        if (pte != 0) { // Có dữ liệu mapping
            int is_present = PAGING_PAGE_PRESENT(pte);
            int is_swapped = PAGING_PAGE_SWAPPED(pte);
            
            if (is_present && !is_swapped) {
                // Đang nằm trên RAM thực tế
                addr_t fpn = PAGING_FPN(pte);
                printf("PGN %03d: Present=1, Swapped=0, FPN=%lu (RAM)\n", pgn, (unsigned long)fpn);
            } 
            else if (is_present && is_swapped) {
                // Bị Thrashing đẩy xuống SWAP
                addr_t swp_fpn = PAGING_SWP(pte);
                printf("PGN %03d: Present=1, Swapped=1, FPN=%lu (SWAP)\n", pgn, (unsigned long)swp_fpn);
            }
        }
    }
    printf("-------------------------------------\n");
    return 0;
}

void print_paging_storage_stats() 
{
    printf("\n=========================================\n");
    printf("      MULTI-LEVEL PAGING STORAGE STATS   \n");
    printf("=========================================\n");
    
    long total_bytes = total_allocated_pagetables * 512 * sizeof(addr_t);
    
    printf("Total Allocated Tables: %d\n", total_allocated_pagetables);
    printf("Total Storage Size: %ld Bytes (%.2f KB)\n", total_bytes, total_bytes / 1024.0);
    printf("=========================================\n\n");
}

#endif
