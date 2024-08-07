/**
 * ideal_indirection
 * CS 341 - Spring 2024
 */
#include "mmu.h"
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

mmu *mmu_create()
{
    mmu *my_mmu = calloc(1, sizeof(mmu));
    my_mmu->tlb = tlb_create();
    return my_mmu;
}

// Note: All page directories will exist, but not all page tables will exist.

void mmu_read_from_virtual_address(mmu *this, addr32 virtual_address,
                                   size_t pid, void *buffer, size_t num_bytes)
{
    assert(this);
    assert(pid < MAX_PROCESS_ID);
    assert(num_bytes + (virtual_address % PAGE_SIZE) <= PAGE_SIZE);

    // use pid to check for context switch
    if (pid != this->curr_pid)
    {
        tlb_flush(&this->tlb);
        this->curr_pid = pid;
    }

    // check if address in segmentation
    vm_segmentations *segments = this->segmentations[pid];
    if (!address_in_segmentations(segments, virtual_address))
    {
        // if not: raise segfault and return
        mmu_raise_segmentation_fault(this);
        return;
    }

    // check TLB for page table entry
    page_table_entry *pte = tlb_get_pte(&this->tlb, virtual_address & ~0xFFF);

    if (!pte)
    {
        // raise tlb miss
        mmu_tlb_miss(this);

        // get pde:
        page_directory *pd = this->page_directories[pid];

        page_directory_entry pde = pd->entries[virtual_address >> 22];

        if (pde.present == 0)
        {

            // raise page fault
            mmu_raise_page_fault(this);

            // ask kernel for frame
            addr32 pde_base = ask_kernel_for_frame(NULL);

            pde.base_addr = (pde_base >> NUM_OFFSET_BITS);
            pde.user_supervisor = 1;
            pde.read_write = 1;
            pde.present = 1;
        }

        // get page table using pde
        page_table *pt = (page_table *)get_system_pointer_from_pde(&pde);

        // get page_table_entry from page_table using 2nd 10 bits
        uint32_t second_ten = (virtual_address & 0x003FF000) >> NUM_OFFSET_BITS;

        pte = &pt->entries[second_ten];

        // add entry to tlb
        tlb_add_pte(&this->tlb, virtual_address & ~0xFFF, pte);
    }

    // if pte's 'present' flag is 0
    if (pte->present == 0)
    {
        // raise page fault
        mmu_raise_page_fault(this);

        // ask kernel for frame
        addr32 pte_base = ask_kernel_for_frame(NULL);

        pte->base_addr = (pte_base >> NUM_OFFSET_BITS);
        pte->user_supervisor = 1;
        pte->read_write = 1;
        pte->present = 1;

        // read page from disk
        read_page_from_disk(pte);
    }

    // check permissions for READING
    vm_segmentation *seg = find_segment(segments, virtual_address);
    if ((seg->permissions & READ) == 0)
    {
        mmu_raise_segmentation_fault(this);
        return;
    }

    // get 'physical' memory address
    uint32_t sim_addr = (pte->base_addr << 12) | (virtual_address & 0xFFF);
    void *physical_addr = get_system_pointer_from_address(sim_addr);

    // read operation
    memcpy(buffer, physical_addr, num_bytes);

    // set 'accessed' flag
    pte->accessed = 1;
}

void mmu_write_to_virtual_address(mmu *this, addr32 virtual_address, size_t pid,
                                  const void *buffer, size_t num_bytes)
{
    assert(this);
    assert(pid < MAX_PROCESS_ID);
    assert(num_bytes + (virtual_address % PAGE_SIZE) <= PAGE_SIZE);

    // use pid to check for context switch
    if (pid != this->curr_pid)
    {
        tlb_flush(&this->tlb);
        this->curr_pid = pid;
    }

    // check if address in segmentation
    vm_segmentations *segments = this->segmentations[pid];
    if (!address_in_segmentations(segments, virtual_address))
    {
        // if not: raise segfault and return
        mmu_raise_segmentation_fault(this);
        return;
    }

    // check TLB for page table entry
    page_table_entry *pte = tlb_get_pte(&this->tlb, virtual_address & ~0xFFF);

    if (!pte)
    {
        // raise tlb miss
        mmu_tlb_miss(this);

        // get pde:
        page_directory *pd = this->page_directories[pid];
        page_directory_entry *pde = &pd->entries[virtual_address >> 22];

        if (!pde->present)
        {
            // raise page fault
            mmu_raise_page_fault(this);

            // ask kernel for frame
            addr32 pde_base = ask_kernel_for_frame(NULL);

            pde->base_addr = pde_base >> 12;
            pde->user_supervisor = 1;
            pde->read_write = 1;
            pde->present = 1;
        }

        // get page table using pde
        page_table *pt = (page_table *)get_system_pointer_from_pde(pde);

        // get page_table_entry from page_table using 2nd 10 bits
        uint32_t second_ten = (virtual_address & 0x003FF000) >> NUM_OFFSET_BITS;
        pte = &pt->entries[second_ten];

        // add entry to tlb
        tlb_add_pte(&this->tlb, virtual_address & ~0xFFF, pte);
    }

    // if pte's 'present' flag is 0
    if (!pte->present)
    {
        // raise page fault
        mmu_raise_page_fault(this);

        // ask kernel for frame
        addr32 pte_base = ask_kernel_for_frame(NULL);

        pte->base_addr = pte_base >> NUM_OFFSET_BITS;
        pte->user_supervisor = 1;
        pte->read_write = 1;
        pte->present = 1;

        // read page from disk
        read_page_from_disk(pte);
    }

    // check permissions for WRITING
    vm_segmentation *seg = find_segment(segments, virtual_address);
    if ((seg->permissions & WRITE) == 0)
    {
        mmu_raise_segmentation_fault(this);
        return;
    }

    // get 'physical' memory address
    uint32_t sim_addr = (pte->base_addr << 12) | (virtual_address & 0xFFF);

    void *physical_addr = get_system_pointer_from_address(sim_addr);

    // read operation
    memcpy(physical_addr, buffer, num_bytes);

    // set 'dirty' and 'accessed' flags
    pte->dirty = 1;
    pte->accessed = 1;
}

void mmu_tlb_miss(mmu *this)
{
    this->num_tlb_misses++;
}

void mmu_raise_page_fault(mmu *this)
{
    this->num_page_faults++;
}

void mmu_raise_segmentation_fault(mmu *this)
{
    this->num_segmentation_faults++;
}

void mmu_add_process(mmu *this, size_t pid)
{
    assert(pid < MAX_PROCESS_ID);
    addr32 page_directory_address = ask_kernel_for_frame(NULL);
    this->page_directories[pid] =
        (page_directory *)get_system_pointer_from_address(
            page_directory_address);
    page_directory *pd = this->page_directories[pid];
    this->segmentations[pid] = calloc(1, sizeof(vm_segmentations));
    vm_segmentations *segmentations = this->segmentations[pid];

    // Note you can see this information in a memory map by using
    // cat /proc/self/maps
    segmentations->segments[STACK] =
        (vm_segmentation){.start = 0xBFFFE000,
                          .end = 0xC07FE000, // 8mb stack
                          .permissions = READ | WRITE,
                          .grows_down = true};

    segmentations->segments[MMAP] =
        (vm_segmentation){.start = 0xC07FE000,
                          .end = 0xC07FE000,
                          // making this writeable to simplify the next lab.
                          // todo make this not writeable by default
                          .permissions = READ | EXEC | WRITE,
                          .grows_down = true};

    segmentations->segments[HEAP] =
        (vm_segmentation){.start = 0x08072000,
                          .end = 0x08072000,
                          .permissions = READ | WRITE,
                          .grows_down = false};

    segmentations->segments[BSS] =
        (vm_segmentation){.start = 0x0805A000,
                          .end = 0x08072000,
                          .permissions = READ | WRITE,
                          .grows_down = false};

    segmentations->segments[DATA] =
        (vm_segmentation){.start = 0x08052000,
                          .end = 0x0805A000,
                          .permissions = READ | WRITE,
                          .grows_down = false};

    segmentations->segments[TEXT] =
        (vm_segmentation){.start = 0x08048000,
                          .end = 0x08052000,
                          .permissions = READ | EXEC,
                          .grows_down = false};

    // creating a few mappings so we have something to play with (made up)
    // this segment is made up for testing purposes
    segmentations->segments[TESTING] =
        (vm_segmentation){.start = PAGE_SIZE,
                          .end = 3 * PAGE_SIZE,
                          .permissions = READ | WRITE,
                          .grows_down = false};
    // first 4 mb is bookkept by the first page directory entry
    page_directory_entry *pde = &(pd->entries[0]);
    // assigning it a page table and some basic permissions
    pde->base_addr = (ask_kernel_for_frame(NULL) >> NUM_OFFSET_BITS);
    pde->present = true;
    pde->read_write = true;
    pde->user_supervisor = true;

    // setting entries 1 and 2 (since each entry points to a 4kb page)
    // of the page table to point to our 8kb of testing memory defined earlier
    for (int i = 1; i < 3; i++)
    {
        page_table *pt = (page_table *)get_system_pointer_from_pde(pde);
        page_table_entry *pte = &(pt->entries[i]);
        pte->base_addr = (ask_kernel_for_frame(pte) >> NUM_OFFSET_BITS);
        pte->present = true;
        pte->read_write = true;
        pte->user_supervisor = true;
    }
}

void mmu_remove_process(mmu *this, size_t pid)
{
    assert(pid < MAX_PROCESS_ID);
    // example of how to BFS through page table tree for those to read code.
    page_directory *pd = this->page_directories[pid];
    if (pd)
    {
        for (size_t vpn1 = 0; vpn1 < NUM_ENTRIES; vpn1++)
        {
            page_directory_entry *pde = &(pd->entries[vpn1]);
            if (pde->present)
            {
                page_table *pt = (page_table *)get_system_pointer_from_pde(pde);
                for (size_t vpn2 = 0; vpn2 < NUM_ENTRIES; vpn2++)
                {
                    page_table_entry *pte = &(pt->entries[vpn2]);
                    if (pte->present)
                    {
                        void *frame = (void *)get_system_pointer_from_pte(pte);
                        return_frame_to_kernel(frame);
                    }
                    remove_swap_file(pte);
                }
                return_frame_to_kernel(pt);
            }
        }
        return_frame_to_kernel(pd);
    }

    this->page_directories[pid] = NULL;
    free(this->segmentations[pid]);
    this->segmentations[pid] = NULL;

    if (this->curr_pid == pid)
    {
        tlb_flush(&(this->tlb));
    }
}

void mmu_delete(mmu *this)
{
    for (size_t pid = 0; pid < MAX_PROCESS_ID; pid++)
    {
        mmu_remove_process(this, pid);
    }

    tlb_delete(this->tlb);
    free(this);
    remove_swap_files();
}
