#include "vmm.h"
#include "pmm.h"
#include "klibc/lock.h"
#include "klibc/string.h"
#include "drivers/serial.h"
#include <stddef.h>

lock_t vmm_spinlock = 0; // Spinlock for the VMM

uint64_t vmm_get_pml4t() {
    uint64_t ret;
    asm volatile("movq %%cr3, %0;" : "=r"(ret));
    return ret;
}

void vmm_set_pml4t(uint64_t new) {
    asm volatile("movq %0, %%cr3;" ::"r"(new) : "memory");
}

void vmm_invlpg(uint64_t new) {
    asm volatile("invlpg (%0);" ::"r"(new) : "memory");
}

void vmm_flush_tlb() {
    vmm_set_pml4t(vmm_get_pml4t());
}

pt_off_t vmm_virt_to_offs(void *virt) {
    uintptr_t addr = (uintptr_t)virt;

    pt_off_t off = {
        .p4_off = (addr & ((size_t)0x1ff << 39)) >> 39,
        .p3_off = (addr & ((size_t)0x1ff << 30)) >> 30,
        .p2_off = (addr & ((size_t)0x1ff << 21)) >> 21,
        .p1_off = (addr & ((size_t)0x1ff << 12)) >> 12,
    };

    return off;
}

void *vmm_offs_to_virt(pt_off_t offs) {
    uintptr_t addr = 0;

    addr |= offs.p4_off << 39;
    addr |= offs.p3_off << 30;
    addr |= offs.p2_off << 21;
    addr |= offs.p1_off << 12;

    return (void *)addr;
}

// pt_ptr_t vmm_get_table(pt_off_t offs, void *p4) {
//     pt_ptr_t ret;

//     pt_t *pml4 = (pt_t *) (((uint64_t) p4) + NORMAL_VMA_OFFSET);
//     ret.p4 = pml4;
//     pt_t *cur_table;
//     sprintf("\nP4: %lx", pml4);
//     /* Check if the p4 entry we want is present */
//     if (pml4->table[offs.p4_off] & VMM_PRESENT) {
//         cur_table = (pt_t *) (pml4->table[offs.p4_off] & ~(0xfff));
//         ret.p3 = (pt_t *) ((uint64_t) cur_table + NORMAL_VMA_OFFSET);
//         cur_table = ret.p3; // use the new and offset address
//     } else {
//         cur_table = (pt_t *) pmm_alloc(0x1000);
//         ret.p4->table[offs.p4_off] = (uint64_t) cur_table;
//         ret.p3 = (pt_t *) ((uint64_t) cur_table + NORMAL_VMA_OFFSET);
//         cur_table = ret.p3;
//         memset((uint8_t *) ret.p3, 0, sizeof(pt_t));
//     }

//     sprintf("\nP3: %lx", cur_table);
//     /* Check if the p3 entry we want is present */
    
//     if (cur_table->table[offs.p3_off] & VMM_PRESENT) {
//         cur_table = (pt_t *) (cur_table->table[offs.p3_off] & ~(0xfff));
//         ret.p2 = (pt_t *) ((uint64_t) cur_table + NORMAL_VMA_OFFSET);
//         cur_table = ret.p2; // use the new and offset address
//     } else {
//         cur_table = (pt_t *) pmm_alloc(0x1000);
//         ret.p3->table[offs.p3_off] = (uint64_t) cur_table;
//         ret.p2 = (pt_t *) ((uint64_t) cur_table + NORMAL_VMA_OFFSET);
//         cur_table = ret.p2;
//         memset((uint8_t *) ret.p2, 0, sizeof(pt_t));
//     }

//     sprintf("\nP2: %lx", cur_table);
//     /* Check if the p2 entry we want is present */
//     if (cur_table->table[offs.p2_off] & VMM_PRESENT) {
//         if (cur_table->table[offs.p2_off] & VMM_HUGE) {
//             sprintf("\nFound huge page noob");
//             /* Page is huge, and likely from bootstrap, remap to 4 KiB pages */
//             uint64_t old_p2 = cur_table->table[offs.p2_off];
//             uint64_t old_perms = (old_p2 & 0xfff) & ~(VMM_HUGE | VMM_HUGE | VMM_ACCESS); // Old permissions, minus the huge flag
//             uint64_t old_addr = old_p2 & ~(0xfff); // Old phys address

//             /* Code to remap the entry, need to do some ordering so that if the new table is in the entry, it doesn't
//                 give me a juicy page fault */
//             cur_table = (pt_t *) pmm_alloc(0x1000);
//             ret.p1 = (pt_t *) ((uint64_t) cur_table + NORMAL_VMA_OFFSET);
//             cur_table = ret.p1;
//             memset((uint8_t *) ret.p1, 0, sizeof(pt_t));
//             for (uint16_t i = 0; i < 512; i++) {
//                 ret.p1->table[i] = old_addr;
//                 old_addr += 0x1000;
//             }
//             sprintf("\nNew data: %lx", ((uint64_t) cur_table - NORMAL_VMA_OFFSET) | old_perms);
//             // Add perms for good measure lol
//             ret.p2->table[offs.p2_off] = ((uint64_t) cur_table - NORMAL_VMA_OFFSET) | old_perms;
//             old_addr -= 0x200000;
//             for (uint16_t i = 0; i < 512; i++) {
//                 vmm_invlpg(old_addr);
//                 old_addr += 0x1000;
//             }
//         } else {
//             cur_table = (pt_t *) (cur_table->table[offs.p2_off] & ~(0xfff));
//             ret.p1 = (pt_t *) ((uint64_t) cur_table + NORMAL_VMA_OFFSET);
//             cur_table = ret.p1; // use the new and offset address
//         }
//     } else {
//         /* Allocate new entry */
//         cur_table = (pt_t *) pmm_alloc(0x1000);
//         ret.p2->table[offs.p2_off] = (uint64_t) cur_table;
//         ret.p1 = (pt_t *) ((uint64_t) cur_table + NORMAL_VMA_OFFSET);
//         cur_table = ret.p1;
//         memset((uint8_t *) ret.p1, 0, sizeof(pt_t));
//     }

//     sprintf("\nP1: %lx", cur_table);

//     return ret; // Return our pointers
// }

pt_ptr_t vmm_get_table(pt_off_t *offs, pt_t *p4) {
    pt_ptr_t ret;
    p4 = (pt_t *)((uint64_t)p4 + NORMAL_VMA_OFFSET);
    // Is the p3 present?
    if (p4->table[offs->p4_off] & VMM_PRESENT) {
        ret.p3 = (pt_t *)((p4->table[offs->p4_off] & ~(0xfff)) + NORMAL_VMA_OFFSET);
    } else {
        size_t pmm_ret = (size_t)pmm_alloc(0x1000);
        ret.p3 = (pt_t *)((pmm_ret + NORMAL_VMA_OFFSET));
        p4->table[offs->p4_off] =
            ((uint64_t)ret.p3 - NORMAL_VMA_OFFSET) | VMM_PRESENT | VMM_WRITE;
        // Store the new entry in the table, as W/S/P (Writeable, supervisor, present)
        memset((uint8_t *)ret.p3, 0, 0x1000); // Clear the table
        vmm_invlpg((uint64_t)vmm_offs_to_virt(*offs));
    }

    // Is the p2 present?
    if (ret.p3->table[offs->p3_off] & VMM_PRESENT) {
        ret.p2 = (pt_t *)((ret.p3->table[offs->p3_off] & ~(0xfff)) + NORMAL_VMA_OFFSET);
    } else {
        ret.p2 = (pt_t *)(((size_t)pmm_alloc(0x1000)) + NORMAL_VMA_OFFSET);
        ret.p3->table[offs->p3_off] =
            ((uint64_t)ret.p2 - NORMAL_VMA_OFFSET) | VMM_PRESENT | VMM_WRITE;
        // Store the new entry in the table, as W/S/P (Writeable, supervisor, present)
        memset((uint8_t *)ret.p2, 0, 0x1000); // Clear the table
        vmm_invlpg((uint64_t)vmm_offs_to_virt(*offs));
    }

    // Is the p1 present and not huge?
    if ((ret.p2->table[offs->p2_off] & VMM_PRESENT) &&
        !((ret.p2->table[offs->p2_off] & VMM_HUGE))) {
        ret.p1 = (pt_t *)((ret.p2->table[offs->p2_off] & ~(0xfff)) + NORMAL_VMA_OFFSET);
    } else {
        uint64_t old_p2 = ret.p2->table[offs->p2_off] & ~(0xfff);
        old_p2 &= ~(VMM_HUGE); // Remove the huge flag
        uint8_t is_huge = 0;

        if ((ret.p2->table[offs->p2_off] & VMM_HUGE)) {
            is_huge = 1;
        }

        ret.p1 = (pt_t *)(((size_t)pmm_alloc(0x1000)) + NORMAL_VMA_OFFSET);
        memset((uint8_t *)ret.p1, 0, 0x1000); // Clear the table
        // Map the old mappings but in 4 KiB if the table is huge, and invalidate the old
        // table.
        if (is_huge) {
            for (uint16_t i = 0; i < 512; i++) {
                ret.p1->table[i] = (old_p2 + (i * 0x1000)) |
                    (ret.p2->table[offs->p2_off] & 0xfff & ~(VMM_HUGE));
                vmm_invlpg(((uint64_t)vmm_offs_to_virt(*offs)) + (i * 0x1000));
            }
        }
        // Store the new entry in the table, as W/S/P (Writeable, supervisor, present)
        ret.p2->table[offs->p2_off] =
            ((uint64_t)ret.p1 - NORMAL_VMA_OFFSET) | VMM_PRESENT | VMM_WRITE;
    }

    return ret;
}

int vmm_map_pages(void *phys, void *virt, void *p4, uint64_t count, uint16_t perms) {
    spinlock_lock(&vmm_spinlock);

    int ret = 0;

    uint8_t *cur_virt = (uint8_t *) virt;
    uint64_t cur_phys = ((uint64_t) phys) & ~(0xfff);

    for (uint64_t page = 0; page < count; page++) {
        sprintf("\nCount: %lu", page);
        pt_off_t offs = vmm_virt_to_offs((void *) cur_virt);
        pt_ptr_t ptrs = vmm_get_table(&offs, p4);
        sprintf("\nP4: %lu P3: %lu P2: %lu P1: %lu", offs.p4_off, offs.p3_off, offs.p2_off, offs.p1_off);

        /* Set the addresses */
        if (!(ptrs.p1->table[offs.p1_off] & VMM_PRESENT)) {
            ptrs.p1->table[offs.p1_off] = cur_phys | (perms | VMM_PRESENT);
            sprintf("\nNew: %lx P1: %lx P2: %lx P3: %lx P4: %lx", ptrs.p1->table[offs.p1_off], ptrs.p1, ptrs.p2, ptrs.p3, ptrs.p4);
            cur_phys += 0x1000;
            cur_virt += 0x1000;
        } else {
            sprintf("\nMapped already");
            ret = 1;
            cur_phys += 0x1000;
            cur_virt += 0x1000;
            continue;
        }
    }
    pt_t *pml4 = (pt_t *) (((uint64_t) p4 & ~(0xfff)) + NORMAL_VMA_OFFSET);
    pt_t *pml3 = (pt_t *) ((pml4->table[0] & ~(0xfff)) + NORMAL_VMA_OFFSET);
    pt_t *pml2 = (pt_t *) ((pml3->table[0] & ~(0xfff)) + NORMAL_VMA_OFFSET);
    pt_t *pml1 = (pt_t *) ((pml2->table[128] & ~(0xfff)) + NORMAL_VMA_OFFSET);
    //pt_t *pml1_ent = (pt_t *) ((pml1->table[0] & ~(0xfff)) + NORMAL_VMA_OFFSET);
    sprintf("\npml4: %lx pml3: %lx pml2 %lx pml1 %lx", pml3, pml2, pml1, pml1->table[0]);
    spinlock_unlock(&vmm_spinlock);
    return ret;
}

int vmm_remap_pages(void *phys, void *virt, void *p4, uint64_t count, uint16_t perms) {
    spinlock_lock(&vmm_spinlock);

    int ret = 0;

    uint8_t *cur_virt = (uint8_t *) virt;
    uint64_t cur_phys = ((uint64_t) phys) & ~(0xfff);

    for (uint64_t page = 0; page < count; page++) {
        pt_off_t offs = vmm_virt_to_offs((void *) cur_virt);
        pt_ptr_t ptrs = vmm_get_table(&offs, p4);

        /* Set the addresses */
        ptrs.p1->table[offs.p1_off] = cur_phys | (perms | VMM_PRESENT);
        cur_phys += 0x1000;
        cur_virt += 0x1000;
    }

    spinlock_unlock(&vmm_spinlock);
    return ret;
}

int vmm_unmap_pages(void *phys, void *virt, void *p4, uint64_t count) {
    spinlock_lock(&vmm_spinlock);

    int ret = 0;

    uint8_t *cur_virt = (uint8_t *) virt;
    uint64_t cur_phys = ((uint64_t) phys) & ~(0xfff);

    for (uint64_t page = 0; page < count; page++) {
        pt_off_t offs = vmm_virt_to_offs((void *) cur_virt);
        pt_ptr_t ptrs = vmm_get_table(&offs, p4);

        /* Set the addresses */
        if ((ptrs.p1->table[offs.p1_off] & VMM_PRESENT)) {
            ptrs.p1->table[offs.p1_off] = 0;
            cur_phys += 0x1000;
            cur_virt += 0x1000;
        } else {
            ret = 1;
            cur_phys += 0x1000;
            cur_virt += 0x1000;
            continue;
        }
    }

    spinlock_unlock(&vmm_spinlock);
    return ret;
}

int vmm_map(void *phys, void *virt, uint64_t count, uint16_t perms) {
    return vmm_map_pages(phys, virt, (void *) vmm_get_pml4t(), count, perms);
}

int vmm_remap(void *phys, void *virt, uint64_t count, uint16_t perms) {
    return vmm_remap_pages(phys, virt, (void *) vmm_get_pml4t(), count, perms);
}

int vmm_unmap(void *phys, void *virt, uint64_t count) {
    return vmm_unmap_pages(phys, virt, (void *) vmm_get_pml4t(), count);
}