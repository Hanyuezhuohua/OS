void enable_paging() {
    // After initializing the page table, write to register SATP register for kernel registers.
    // Flush the TLB to invalidate the existing TLB Entries
    // Suggested: 2 LoCs
    w_satp(MAKE_SATP(kernel_pagetable));
    flush_tlb();
}

// Return the address of the PTE in page table *pagetable*
// The Risc-v Sv48 scheme has four levels of page table.
// For VA:
//   47...63 zero
//   39...47 -- 9  bits of level-3 index
//   30...38 -- 9  bits of level-2 index
//   21...29 -- 9  bits of level-1 index
//   12...20 -- 9  bits of level-0 index
//   0...11  -- 12 bits of byte offset within the page
// Return the last-level page table entry (WITHOUT checking it's validity).
static pte_t* pt_query(pagetable_t pagetable, vaddr_t va, int alloc){
    if(va >= MAXVA) BUG_FMT("get va[0x%lx] >= MAXVA[0x%lx]", va, MAXVA);
    // Suggested: 18 LoCs
    pte_t *pte;
    for (int level = 3; level >= 1; --level) {
        pte = &pagetable[PX(level, va)];
        if (*pte & PTE_V) { // is valid
            pagetable = (pagetable_t) PTE2PA(*pte);
        } else { // not valid
            if (!alloc) return NULL;
            pagetable = (pde_t*) mm_kalloc();
            if (pagetable == NULL) return NULL;
            memset(pagetable, 0, BD_LEAF_SIZE);
            *pte = PA2PTE(pagetable) | PTE_V; // set valid
        }
    }
    return &pagetable[PX(0, va)];
}
int pt_map_pages(pagetable_t pagetable, vaddr_t va, paddr_t pa, uint64 size, int perm){
    // Suggested: 11 LoCs
    uint64 va_end = PGROUNDDOWN(va + size - 1);
    va = PGROUNDDOWN(va);
    while (true) {
        pt_map_addrs(pagetable, va, pa, perm);
        if (va == va_end) break;
        va += PGSIZE; pa += PGSIZE;
    }
    return 0; // Do not modify
}

paddr_t pt_query_address(pagetable_t pagetable, vaddr_t va){
    // Suggested: 3 LoCs
    pte_t *pte = pt_query(pagetable, va, 0);
    // not found or found invalid address: return addr = 0
    if (pte == NULL || !(*pte & PTE_V)) return 0;
    return (paddr_t)(PTE2PA(*pte) | (va & 0xFFF));
}

int pt_unmap_addrs(pagetable_t pagetable, vaddr_t va){
    // Suggested: 2 LoCs
    pte_t *last_level_pte = pt_query(pagetable, va, 0);
    if (last_level_pte == NULL) return 0;
    *last_level_pte = 0;
    return 0; // Do not modify
}

int pt_map_addrs(pagetable_t pagetable, vaddr_t va, paddr_t pa, int perm){
    // Suggested: 2 LoCs
    pte_t *last_level_pte = pt_query(pagetable, va, 1);
    if (last_level_pte == NULL) return 0;
    *last_level_pte = PA2PTE(pa) | perm | PTE_V;
    return 0; // Do not modify
}
