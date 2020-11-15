// implement fork from user space

#include "inc/assert.h"
#include "inc/mmu.h"
#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	pte_t pte = uvpt[PGNUM(addr)];
	if (!((err & FEC_WR) && (pte & PTE_COW)))
	{
		panic("not cow page fault!");
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	uintptr_t cowpg_addr = ROUNDDOWN((uintptr_t)addr, PGSIZE);
	sys_page_alloc(0, PFTEMP, PTE_P | PTE_U | PTE_W);
	memcpy((void*)PFTEMP, (void*)cowpg_addr, PGSIZE);
	sys_page_map(0, (void*)PFTEMP, 0, (void*)cowpg_addr, PTE_U | PTE_P | PTE_W);
	sys_page_unmap(0, (void*)PFTEMP);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	uintptr_t addr = pn * PGSIZE;
	pte_t pte = uvpt[pn];
	if ((pte & PTE_W) || (pte & PTE_COW))
	{
		sys_page_map(0, (void*)addr, envid, (void*)addr, PTE_P | PTE_U | PTE_COW);
		sys_page_map(0, (void*)addr, 0, (void*)addr, PTE_P | PTE_U | PTE_COW);
	}
	else {
		sys_page_map(0, (void*)addr, envid, (void*)addr, PTE_P | PTE_U);
	}

	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.

	// set parent fault handler
	set_pgfault_handler(pgfault);

	// create child env
	envid_t child_envid = sys_exofork();

	if (child_envid == 0)
	{
		// child
		envid_t envid = sys_getenvid();
		thisenv = envs + ENVX(envid);
		return 0;
	}

	// set child fault upcall (handler would be set with memory copy)
	extern void _pgfault_upcall(void);
	sys_env_set_pgfault_upcall(child_envid, _pgfault_upcall);

	// duplicate all presenting page
	for (int pdx = 0; pdx < PDX(UTOP); pdx++)
	{
		pde_t pde = uvpd[pdx];
		if (pde & PTE_P)
		{
			for (int ptx = 0; ptx < NPTENTRIES; ptx++)
			{
				void* addr = PGADDR(pdx, ptx, 0);

				// don't duplicate the exception stack!
				if (addr == (void*)(UXSTACKTOP - PGSIZE)) continue;

				int pn = PGNUM(addr);
				pte_t pte = uvpt[pn];
				if (pte & PTE_P)
				{
					duppage(child_envid, pn);
				}
			}
		}
	}

	// special handle the exception stack
	sys_page_alloc(child_envid, (void*)(UXSTACKTOP - PGSIZE), PTE_U | PTE_P | PTE_W);

	// child env ready to run
	sys_env_set_status(child_envid, ENV_RUNNABLE);

	return child_envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
