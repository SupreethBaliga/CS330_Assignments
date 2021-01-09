#include<types.h>
#include<mmap.h>

//defining the page_sizes
#define PGSIZE 4096
#define HPGSIZE 2097152
#define HPOINTER (1<<7)

// Helper function to create a new vm_area
struct vm_area* create_vm_area(u64 start_addr, u64 end_addr, u32 flags, u32 mapping_type)
{
	struct vm_area *new_vm_area = alloc_vm_area();
	new_vm_area-> vm_start = start_addr;
	new_vm_area-> vm_end = end_addr;
	new_vm_area-> access_flags = flags;
	new_vm_area->mapping_type = mapping_type;
	return new_vm_area;
}


void tlb_flush(u64 addr, u64 length) {
	
	//addr is already aligned 
	// length was needed during checks
	asm volatile (
	"invlpg (%0);" 
	:: "r"(addr) 
	: "memory"
	); 
	return;
}

/**
 * helper function to merge the nodes
*/
void merge_nodes(struct vm_area* head) {
	struct vm_area* itr = head->vm_next; //not modifying the dummy node
	while(itr!=NULL) {
		if(itr->vm_next == NULL) return;
		else {
			struct vm_area* node = itr->vm_next;
			if(itr->vm_end == node->vm_start && itr->mapping_type == node->mapping_type && itr->access_flags == node ->access_flags) {
				itr->vm_end = node->vm_end;
				itr->vm_next = node->vm_next;
				node->vm_next = NULL;
				dealloc_vm_area(node);
			}
			else {
				itr = itr->vm_next;
			}
		}
	}
	return;
}

/**
 * Helper function
 * which deallocates the unused pfns in the page table
*/
void maintain_page_table(struct exec_context* ctx, u64 addr) {
	u64 *vaddr_base = (u64 *)osmap(ctx->pgd);
	u64* entries[5];
	u64* pfns[5];
	int sz = 0;
	u64 *entry;
	u64 pfn;

	// note that we never remove the page which is pointed to by the cr3 register
	// find the entry in page directory level 1
	entry = vaddr_base + ((addr & PGD_MASK) >> PGD_SHIFT);
	entries[sz] = entry;
	pfns[sz] = vaddr_base;
	sz++;
	if(*entry & 0x1) {
		// PGD->PUD Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		goto label;
	}

	//level 2
	entry = vaddr_base + ((addr & PUD_MASK) >> PUD_SHIFT);
	entries[sz] = entry;
	pfns[sz] = vaddr_base;
	sz++;
	if(*entry & 0x1) {
		// PUD->PMD Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		goto label;
	}

	//level 3
	entry = vaddr_base + ((addr & PMD_MASK) >> PMD_SHIFT);
	entries[sz] = entry;
	pfns[sz] = vaddr_base;
	sz++;
	if(*entry & 0x1) {
		// PMD->PTE Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		goto label;
	}

	entry = vaddr_base + ((addr & PTE_MASK) >> PTE_SHIFT);
	entries[sz] = entry;
	pfns[sz] = vaddr_base;
	sz++;
	if((*entry) & 0x1) return;
	else goto label;

	label:
	sz--;
	while(sz>0) { //since dont want to deallocate the level 1 ever
		vaddr_base = pfns[sz];
		//check if all entries of this pfn is 0
		entry = vaddr_base;
		for(int i=0;i<512;i++) {
			if((*entry) & 0x1) return; //entry valid so no maintenance in this level and above
			entry ++; //go to the next entry
		}
		//control reached here means this pfn is completely empty
		// get value of this pfn from the previous level
		pfn = ((*entries[sz-1])>>PTE_SHIFT);
		*entries[sz-1] = *entries[sz-1] ^ 0x1; //setting the present bit of previous level to 0
		os_pfn_free(OS_PT_REG,pfn);
		sz--;
	}
}

/**
 * Helpfer function
 * to check if a frame exists
 * is already allocated or not
**/
int check_if_exists(struct exec_context* ctx, u64 addr) {
	u64 *vaddr_base = (u64 *)osmap(ctx->pgd);
	u64 *entry;
	u64 pfn;

	// find the entry in page directory level 1
	entry = vaddr_base + ((addr & PGD_MASK) >> PGD_SHIFT);
	if(*entry & 0x1) {
		// PGD->PUD Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		return 0;
	}

	//level 2
	entry = vaddr_base + ((addr & PUD_MASK) >> PUD_SHIFT);
	if(*entry & 0x1) {
		// PUD->PMD Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		return 0;
	}

	//level 3
	entry = vaddr_base + ((addr & PMD_MASK) >> PMD_SHIFT);
	if(*entry & 0x1) {
		// PMD->PTE Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		return 0;
	}

	entry = vaddr_base + ((addr & PTE_MASK) >> PTE_SHIFT);
	if((*entry) & 0x1) return 1;
	else return 0;
}

/**
 * Helpfer function
 * to check if a frame exists
 * is already allocated or not
**/
int check_if_hugepage_create(struct exec_context* ctx, u64 addr) {
	//addr is the beginning address of the huge page
	int ans = 0;
	for(u64 x=addr;x<addr+HPGSIZE;x+=PGSIZE) {
		ans = ans | check_if_exists(ctx,x);
	} 
	return ans;
}

/**
 * Helper function
 * Check if a huge page is allocated corresponding to the address
 * 
**/
int check_if_huge_page_exists(struct exec_context *ctx, u64 addr) {
	u64 *vaddr_base = (u64 *)osmap(ctx->pgd);
	u64 *entry;
	u64 pfn;

	// find the entry in page directory level 1
	entry = vaddr_base + ((addr & PGD_MASK) >> PGD_SHIFT);
	if(*entry & 0x1) {
		// PGD->PUD Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		return 0;
	}

	//level 2
	entry = vaddr_base + ((addr & PUD_MASK) >> PUD_SHIFT);
	if(*entry & 0x1) {
		// PUD->PMD Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		return 0;
	}

	//level 3
	entry = vaddr_base + ((addr & PMD_MASK) >> PMD_SHIFT);
	
	if((*entry & 0x1) && (*entry & HPOINTER)) {
		return 1;
	}
	else return 0;
}


/*
 * Helper function to allocate a pfn to a page in vaddr, given an address
 */
void install_hugepage_pfn(struct exec_context *ctx, u64 addr, u64 ac_flags, u64 hppfn) {

	//only the flags in the last level matter. Others are all ok.
	// get base addr of pgdir
	u64 *vaddr_base = (u64 *)osmap(ctx->pgd);
	u64 *entry;
	u64 pfn;

	if(ac_flags & PROT_WRITE) {
		ac_flags = 0x5 | 0x2; // PWU
	} 
	else {
		ac_flags = 0x5; //just present and user bits
	}
	
	// find the entry in page directory level 1
	entry = vaddr_base + ((addr & PGD_MASK) >> PGD_SHIFT);
	if(*entry & 0x1) {
		// PGD->PUD Present, access it
		*entry = (*entry) | ac_flags;
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		// allocate PUD
		pfn = os_pfn_alloc(OS_PT_REG);
		*entry = (pfn << PTE_SHIFT) | ac_flags;
		vaddr_base = osmap(pfn);
	}

	//level 2
	entry = vaddr_base + ((addr & PUD_MASK) >> PUD_SHIFT);
	if(*entry & 0x1) {
		// PUD->PMD Present, access it
		*entry = (*entry) | ac_flags;
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		// allocate PMD
		pfn = os_pfn_alloc(OS_PT_REG);
		*entry = (pfn << PTE_SHIFT) | ac_flags;
		vaddr_base = osmap(pfn);
	}

	//level 3
	entry = vaddr_base + ((addr & PMD_MASK) >> PMD_SHIFT);
	
	*entry = (hppfn << 21) | ac_flags | HPOINTER; // set flags here
}

/*
 * Helper function to allocate a pfn to a page in vaddr, given an address
 */
u64 install_pfn(struct exec_context *ctx, u64 addr, u64 ac_flags) {

	//only the flags in the last level matter. Others are all ok.
	// get base addr of pgdir
	u64 *vaddr_base = (u64 *)osmap(ctx->pgd);
	u64 *entry;
	u64 pfn;

	if(ac_flags & PROT_WRITE) {
		ac_flags = 0x5 | 0x2; // PWU
	} 
	else {
		ac_flags = 0x5; //just present and user bits
	}
	
	// find the entry in page directory level 1
	entry = vaddr_base + ((addr & PGD_MASK) >> PGD_SHIFT);
	if(*entry & 0x1) {
		// PGD->PUD Present, access it
		*entry = (*entry) | ac_flags;
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		// allocate PUD
		pfn = os_pfn_alloc(OS_PT_REG);
		*entry = (pfn << PTE_SHIFT) | ac_flags;
		vaddr_base = osmap(pfn);
	}

	//level 2
	entry = vaddr_base + ((addr & PUD_MASK) >> PUD_SHIFT);
	if(*entry & 0x1) {
		// PUD->PMD Present, access it
		*entry = (*entry) | ac_flags;
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		// allocate PMD
		pfn = os_pfn_alloc(OS_PT_REG);
		*entry = (pfn << PTE_SHIFT) | ac_flags;
		vaddr_base = osmap(pfn);
	}

	//level 3
	entry = vaddr_base + ((addr & PMD_MASK) >> PMD_SHIFT);
	if(*entry & 0x1) {
		// PMD->PTE Present, access it
		*entry = (*entry) | ac_flags;
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		// allocate PLD 
		pfn = os_pfn_alloc(OS_PT_REG);
		*entry = (pfn << PTE_SHIFT) | ac_flags;
		vaddr_base = osmap(pfn);
	}

	entry = vaddr_base + ((addr & PTE_MASK) >> PTE_SHIFT);
	// since this fault occured as frame was not present, we don't need present check here
	pfn = os_pfn_alloc(USER_REG);
	*entry = (pfn << PTE_SHIFT) | ac_flags; // set flags here
	return pfn;
}

/***
 * Helper function
 * To get hugepage_pfn_number
**/

u64 get_hugepage_pfn_number(struct exec_context* ctx, u64 addr) {
	u64 *vaddr_base = (u64 *)osmap(ctx->pgd);
	u64 *entry;
	u64 pfn;

	// find the entry in page directory level 1
	entry = vaddr_base + ((addr & PGD_MASK) >> PGD_SHIFT);
	if(*entry & 0x1) {
		// PGD->PUD Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		return 0;
	}

	//level 2
	entry = vaddr_base + ((addr & PUD_MASK) >> PUD_SHIFT);
	if(*entry & 0x1) {
		// PUD->PMD Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		return 0;
	}

	//level 3
	entry = vaddr_base + ((addr & PMD_MASK) >> PMD_SHIFT);
	
	if((*entry & 0x1) && (*entry & (HPOINTER))) {
		//make present bit 0 since 
		*entry = *(entry) ^ 0x1; //turning present bit to 0
		*entry = *(entry) ^ HPOINTER; //turning 7th bit zero
		return ((*entry)>>21);
	}
	else return 0;
}

/****
 * Helper function
 * Returns the pfn number corresponding to a virtual address and mapping
 * also sets the present bit to zero. Since used only during unmapping or unallocating
****/
u64 get_pfn_number(struct exec_context* ctx, u64 addr) {
	//returning 0 implies the area is not allocated anyways
	u64 *vaddr_base = (u64 *)osmap(ctx->pgd);
	u64 *entry;
	u64 pfn;
	
	entry = vaddr_base + ((addr & PGD_MASK) >> PGD_SHIFT);
	if(*entry & 0x1) {
		// PGD->PUD Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		return 0;
	}

	//level 2
	entry = vaddr_base + ((addr & PUD_MASK) >> PUD_SHIFT);
	if(*entry & 0x1) {
		// PUD->PMD Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		return 0;
	}

	//level 3
	entry = vaddr_base + ((addr & PMD_MASK) >> PMD_SHIFT);
	if(*entry & 0x1) {
		// PMD->PTE Present, access it
		pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
		vaddr_base = (u64 *)osmap(pfn);
	}else{
		return 0;
	}

	entry = vaddr_base + ((addr & PTE_MASK) >> PTE_SHIFT);
	if(*entry & 0x1) {
		*entry = *(entry) ^ 0x1; //turning present bit into zero
		return ((*entry) >> 12);
	}
	else return 0;
	// since this fault occured as frame was not present, we don't need present check here
}

/**
 * Helper function
 * inserts into list and applies merging of the vm_areas if possibel
 */
struct vm_area* insert_into_list(struct vm_area* node, struct vm_area* head) {
	struct vm_area* itr = head,*prev;
	while((itr != NULL) && (itr->vm_start <= node->vm_end-1)) {
		prev = itr;
		itr = itr->vm_next;
	}
	//prev is never null because of dummy node
	prev->vm_next = node;
	node->vm_next = itr;

	//Now merge the overlapping
	
	if((itr!=NULL) && (node->vm_end == itr->vm_start) && (node->access_flags == itr->access_flags) && (node->mapping_type == itr->mapping_type)) {
		node->vm_end = itr->vm_end;
		node->vm_next = itr->vm_next;
		itr->vm_next = NULL;
		dealloc_vm_area(itr);
	}

	//merging with the prev node only if it is not the dummy node
	if((prev->vm_start != MMAP_AREA_START) && (prev->vm_end == node->vm_start) && (prev->access_flags == node->access_flags) && (prev->mapping_type == node->mapping_type)) {
		prev->vm_end = node->vm_end;
		prev->vm_next = node->vm_next;
		node->vm_next = NULL;
		dealloc_vm_area(node);
	}

	return head;
}


/**
 * Function will invoked whenever there is page fault. (Lazy allocation)
 * 
 * For valid access. Map the physical page 
 * Return 0
 * 
 * For invalid access, i.e Access which is not matching the vm_area access rights (Writing on ReadOnly pages)
 * return -1. 
 */
int vm_area_pagefault(struct exec_context *current, u64 addr, int error_code)
{
	//trivial checks
	if(current == NULL) return -1; //invalid
	if(addr<MMAP_AREA_START || addr>MMAP_AREA_END) return -1; //not in vma 

	if(error_code == 0x7) return -1;

	//get protection flags of the address
	u64 ac_flags = 0,mapping_type;
	struct vm_area* itr = current->vm_area;
	int isPresent = 0; //checks if the addr given is actually valid to map
	while(itr!=NULL) {
		if(addr>= itr->vm_start && addr< itr->vm_end) {
			ac_flags = itr->access_flags;
			mapping_type = itr->mapping_type;
			isPresent = 1;
			break;
		} 
		itr = itr->vm_next;
	}

	if(isPresent == 0) return -1; //address not present in vma

	if(((ac_flags & PROT_WRITE) ==0) && (error_code == 0x6)) return -1; //tried writing to a read_only space

	if(ac_flags & PROT_WRITE !=0) ac_flags = ac_flags | PROT_READ; //since pfns with write access should have read implicitly
	// no invalid access so map the unmapped
	if(mapping_type == NORMAL_PAGE_MAPPING) {
		install_pfn(current,addr,ac_flags);
	}
	else {
		u64 hpaddr = (u64)os_hugepage_alloc();
		u64 hpppfn = (u64)get_hugepage_pfn((void*) hpaddr);
		install_hugepage_pfn(current,addr,ac_flags,hpppfn);
	}
	return 1;
}

/**
 * mmap system call implementation.
 */
long vm_area_map(struct exec_context *current, u64 addr, int length, int prot, int flags)
{
	// trivial checks 
	if(current==NULL || length<=0) return -1; //invalid argument given

	struct vm_area* head = current->vm_area; //stores the vm_area linked list
	struct vm_area* itr = NULL, *node = NULL;

	if(head == NULL) { //insert the dummy node
		head = create_vm_area(MMAP_START,MMAP_START+4096,0x4,NORMAL_PAGE_MAPPING);
		head->vm_next = NULL;
	}
	unsigned long up,down;

	// Let us first make the length aligned with 4kb
	if(length % PGSIZE !=0) {
		length = (length/PGSIZE +1)*PGSIZE;
	}

	if(flags & MAP_FIXED) { // Fixed Start Address
		if(addr == 0) return -EINVAL; //throw an error
		if(addr%PGSIZE!=0) return -EINVAL; // invalid argument
		if(addr < MMAP_AREA_START || addr + length -1 > MMAP_AREA_END) return -EINVAL; //invalid address

		down = MMAP_AREA_START; 
		itr = head;
		while(itr!=NULL) {
			up = itr->vm_start-1;
			if((addr >= down) && (addr + length-1 <=up)) {
				node = create_vm_area(addr, addr+length, prot, NORMAL_PAGE_MAPPING);
				head = insert_into_list(node,head);
				current->vm_area = head;
				return addr;
			} 
			down = itr->vm_end;
			itr  = itr->vm_next;
		}

		// not yet assigned
		up = MMAP_AREA_END;
		if((addr >= down) && (addr + length-1 <=up)) {
			node = create_vm_area(addr, addr+length, prot, NORMAL_PAGE_MAPPING);
			head = insert_into_list(node,head);
			merge_nodes(head);
			current->vm_area = head;
			return addr;
		}
		else return -1; // not able to map
	}

	else { // address map not fixed

		if(addr != 0) { // hint address given

			if(addr % PGSIZE !=0) { // align the hint address
				addr  = (addr/PGSIZE+1)*PGSIZE;
			}
			
			// check address errors
			if(addr < MMAP_AREA_START || addr + length -1 > MMAP_AREA_END) {
				return -1; //since error
			}
			else {
				//check if possible to enter the vm_area at hint address. Else we follow the same procedure as no hint.
			
				itr = head; down = MMAP_AREA_START;
				while(itr != NULL) {
					up = itr->vm_start-1;
					if((addr >= down) && (addr + length-1 <=up)) {
						node = create_vm_area(addr, addr+length, prot, NORMAL_PAGE_MAPPING);
						head = insert_into_list(node,head);
						current->vm_area = head;
						return addr;
					}
					down = itr->vm_end;
					itr = itr->vm_next;
				}
				// still not assigned
				up = MMAP_AREA_END;
				if((addr >= down) && (addr + length-1 <=up)) {
					node = create_vm_area(addr, addr+length, prot, NORMAL_PAGE_MAPPING);
					head = insert_into_list(node,head);
					current->vm_area = head;
					return addr;
				}
			}
			// STILL NOT RETURNED. SO NODE NOT ASSIGNED
			//not possible to put at hint addres. So do without hint address
		}

		//without hint address
		down = MMAP_AREA_START; itr = head;
		while(itr!=NULL) {
			up = itr->vm_start-1;
			if(up-down+1 >= length) {
				node = create_vm_area(down,down+length,prot,NORMAL_PAGE_MAPPING);
				head = insert_into_list(node,head);
				current->vm_area = head;
				return down;
			}
			down = itr->vm_end;
			itr = itr->vm_next;
		}
		// still not assigned
		up = MMAP_AREA_END;
		if(up-down+1 >= length) {
			node = create_vm_area(down,down+length,prot,NORMAL_PAGE_MAPPING);
			head = insert_into_list(node,head);
			current->vm_area = head;
			return down;
		}
		merge_nodes(head);

		return -1; // since not possible to assign this VM_AREA

	}




	return -1; //dummy
}


/**
 * munmap system call implemenations
 */
int vm_area_unmap(struct exec_context *current, u64 addr, int length)
{
	//trivial checks
	if(current==NULL || length<=0) return -1; //invalid 
	if(addr % PGSIZE !=0) return -1; //since address must be page boundary
	//given that address range with be in [MMAP_START,MMAP_END] so no need to worry about those
	struct vm_area* head  = current->vm_area, *itr=NULL,*node=NULL,*prev;
	u64 start_addr=-1,end_addr=-1; //end_addr inclusive

	//let us align the unmaps first so that they start and end inside a vm_area and at page boundary
	itr = head;
	//setting the start_addr
	while(itr!=NULL) {
		if(addr<itr->vm_start && addr+length-1<itr->vm_start) {
			// nothing to unmp	
			break;
		}
		else if(addr<itr->vm_start && addr+length-1>=itr->vm_start) {
			start_addr = itr->vm_start;
			break;
		}
		else if(addr>=itr->vm_start && addr<itr->vm_end) {
			start_addr = addr;
			if(itr->mapping_type == NORMAL_PAGE_MAPPING) start_addr = (start_addr/PGSIZE)*PGSIZE;
			else start_addr = (start_addr/HPGSIZE)*HPGSIZE;
			break;
		}
		else {
			itr = itr->vm_next;
		}
	}

	if(start_addr == -1) return 0; //nothing to unmap since start address after all the vm_area mappings

	itr= head,prev=NULL; 
	//setting the end_addr
	//setting of the start_address removes the case of nothing to unmap anyways 
	while(itr!=NULL) {
		if(addr+length-1 < itr->vm_start) {
			end_addr = prev->vm_end-1; //because of dummy node, prev will never be NULL;
			break;
		}
		else if(addr+length-1>=itr->vm_start && addr+length-1 <itr->vm_end) {
			end_addr = addr+length; //easy to check page boundary if assigned like this
			if(itr->mapping_type == NORMAL_PAGE_MAPPING) {
				//if not aligned, align it
				if(end_addr%PGSIZE!=0) end_addr = (end_addr/PGSIZE + 1) *PGSIZE;
			}			
			else {
				if(end_addr%HPGSIZE !=0) end_addr = (end_addr/HPGSIZE +1)*HPGSIZE;
			}
			end_addr--;
			break;
		}
		else {
			prev = itr;
			itr = itr->vm_next;
		}
	}

	if(end_addr==-1) { //prev is the tail node
		end_addr = prev->vm_end-1;
	}
	if(end_addr<=start_addr) return 0; // nothing to unmap


	u64 pfn;

	//start_addr and end_addr are now set and algined. Now, delete the nodes
	// Note that any kind of new merging wont appear in unmap
	itr= head; prev=NULL; //prev not null inside while loop because of dummy node
	while(itr!=NULL) {
		pfn=0;

		if(start_addr<=itr->vm_start && end_addr>=itr->vm_end-1) {
			prev->vm_next = itr->vm_next;
			itr->vm_next = NULL;
			u64 x = itr->vm_start;

			/**************************deallocate page frames here**********************************/
			while(x<=itr->vm_end-1) {
				if(itr->mapping_type == HUGE_PAGE_MAPPING) {
					pfn = get_hugepage_pfn_number(current,x);
					if(pfn) {
						void* hpaddr = (void*)(pfn*HPGSIZE);
						os_hugepage_free((void*)hpaddr);
						maintain_page_table(current,x);
						tlb_flush(x,HPGSIZE);
					}
					x+= HPGSIZE;
				}
				else {
					pfn = get_pfn_number(current,x);
					if(pfn) {
						os_pfn_free(USER_REG,pfn);
						maintain_page_table(current,x);
						tlb_flush(x,PGSIZE);
					}
					x+=PGSIZE;
				}
			}

			dealloc_vm_area(itr);
			itr = prev->vm_next;
		}
		else if(start_addr<=itr->vm_start && end_addr<itr->vm_end-1 && end_addr>=itr->vm_start) {
			/**************************deallocate page frames here**********************************/
			u64 x = itr->vm_start;
			while(x<=end_addr) {
				if(itr->mapping_type == HUGE_PAGE_MAPPING) {
					pfn = get_hugepage_pfn_number(current,x);
					if(pfn) {
						void* hpaddr = (void*)(pfn*HPGSIZE);
						os_hugepage_free((void*)hpaddr);
						maintain_page_table(current,x);
						tlb_flush(x,HPGSIZE);
					}
					x+= HPGSIZE;
				}
				else {
					pfn = get_pfn_number(current,x);
					if(pfn) {
						os_pfn_free(USER_REG,pfn);
						maintain_page_table(current,x);
						tlb_flush(x,PGSIZE);
					}
					x+=PGSIZE;
				}
			}

			itr->vm_start = end_addr+1;
			break;
		}
		else if(start_addr>itr->vm_start && start_addr<=itr->vm_end-1 && end_addr>=itr->vm_end-1) {
			/**************************deallocate page frames here**********************************/
			u64 x = start_addr;
			while(x<= itr->vm_end-1) {
				if(itr->mapping_type == HUGE_PAGE_MAPPING) {
					pfn = get_hugepage_pfn_number(current,x);
					if(pfn) {
						void* hpaddr = (void*)(pfn*HPGSIZE);
						os_hugepage_free((void*)hpaddr);
						maintain_page_table(current,x);
						tlb_flush(x,HPGSIZE);
					}
					x+= HPGSIZE;
				}
				else {
					pfn = get_pfn_number(current,x);
					if(pfn) {
						os_pfn_free(USER_REG,pfn);
						maintain_page_table(current,x);
						tlb_flush(x,PGSIZE);
					}
					x+=PGSIZE;
				}
			}

			itr->vm_end = start_addr;
			prev = itr;
			itr = itr->vm_next;
		}
		else if(start_addr>itr->vm_start && end_addr<itr->vm_end-1) {
			u64 x = start_addr;
			while(x<= end_addr) {
				if(itr->mapping_type == HUGE_PAGE_MAPPING) {
					pfn = get_hugepage_pfn_number(current,x);
					if(pfn) {
						void* hpaddr = (void*)(HPGSIZE*pfn);
						os_hugepage_free((void*)hpaddr);
						maintain_page_table(current,x);
						tlb_flush(x,HPGSIZE);
					}
					x+= HPGSIZE;
				}
				else {
					pfn = get_pfn_number(current,x);
					if(pfn) {
						os_pfn_free(USER_REG,pfn);
						maintain_page_table(current,x);
						tlb_flush(x,PGSIZE);
					}
					x+=PGSIZE;
				}
			}

			node = create_vm_area(end_addr+1,itr->vm_end,itr->access_flags,NORMAL_PAGE_MAPPING);
			node->vm_next = itr->vm_next;
			itr->vm_next = node;
			itr->vm_end = start_addr;
			break;
		}
		else {
			prev = itr;
			itr = itr->vm_next;
		}
	}

	return 0;
}


/**
 * make_hugepage system call implemenation
 */
long vm_area_make_hugepage(struct exec_context *current, void *addr, u32 length, u32 prot, u32 force_prot)
{
	//trivial checks
	if(current == NULL || length<=0 ||addr ==NULL) return -EINVAL;

	struct vm_area* head = current->vm_area, *itr = NULL, *node = NULL, *prev = NULL;
	//setting the start and end addresses
	u64 start,end;
	start = (u64) addr;
	end = ((u64)addr)+length;
	end--; //end inclusive
	u64 temp_start = start;
	if(end<=start) return -1;

	int huge_present = 0;

	//Check if all the regions from start to end are mapped and if huge_mapping exists in this range
	u64 temp = 0;
	itr = head;
	while(itr!=NULL) {
		if(start <= itr->vm_start && end>=itr->vm_end-1) {
			if(temp_start<itr->vm_start) {
				return -ENOMAPPING;
			}
			temp += itr->vm_end - itr->vm_start;
			temp_start = itr->vm_end;
			if(itr->mapping_type == HUGE_PAGE_MAPPING) huge_present =1;
		}
		else if(start<=itr->vm_start && end>=itr->vm_start && end <= itr->vm_end-1) {
			if(temp_start<itr->vm_start) {
				return -ENOMAPPING;
			}
			temp += end - itr->vm_start +1;
			if(itr->mapping_type == HUGE_PAGE_MAPPING) huge_present =1;
			temp_start = end+1;
			break;
		}
		else if(start>=itr->vm_start && start<=itr->vm_end-1 && end>=itr->vm_end-1) {
			temp_start = itr->vm_end;
			temp += itr->vm_end - start;
			if(itr->mapping_type == HUGE_PAGE_MAPPING) huge_present =1;
		}
		else if(start>=itr->vm_start && end<=itr->vm_end-1) {
			temp_start = end+1;
			temp += end-start+1;
			if(itr->mapping_type == HUGE_PAGE_MAPPING) huge_present =1;
			break;
		}
		itr = itr->vm_next;
	}
	// 2 checks
	//full area not covered
	if(temp_start!=end+1) return -ENOMAPPING;
	
	//check if full area covered
	if(temp != length) return -ENOMAPPING;

	//full area covered now
	// check if already huge mapping exis
	if(huge_present == 1) return -EVMAOCCUPIED;

	// check if different permissions in case force_prot = 0;
	if(force_prot == 0) {
		itr = head;
		while(itr!=NULL) {
			if(start <= itr->vm_start && end>=itr->vm_end-1) {
				if(itr->access_flags !=prot) return -EDIFFPROT;
			}
			else if(start<=itr->vm_start && end>=itr->vm_start && end <= itr->vm_end-1) {
				if(itr->access_flags !=prot) return -EDIFFPROT;		
			}
			else if(start>=itr->vm_start && start<=itr->vm_end-1 && end>=itr->vm_end-1) {
				if(itr->access_flags !=prot) return -EDIFFPROT;
			}
			else if(start>=itr->vm_start && end<=itr->vm_end-1) {
				if(itr->access_flags !=prot) return -EDIFFPROT;
			}
			itr = itr->vm_next;
		}
	}
	// no errors 
	//remove all previous vm_nodes which overlap
	end++;
	if(start%HPGSIZE!=0) start = (start/HPGSIZE + 1)*HPGSIZE;
	if(end%HPGSIZE!=0) end = (end/HPGSIZE)*HPGSIZE;
	end--;
	if(end<=start) return -1; // since no pages can be added
	length = end-start+1; // new length
	itr = head; prev=NULL;
	while(itr!=NULL) {
		if(start <= itr->vm_start && end>=itr->vm_end-1) {
			prev->vm_next = itr->vm_next;
			itr->vm_next = NULL;
			dealloc_vm_area(itr);
			itr = prev->vm_next;
		}
		else if(start<=itr->vm_start && end>=itr->vm_start && end < itr->vm_end-1) {
			itr->vm_start = end+1;
			prev = itr;
			itr = itr->vm_next;
			break;
		}
		else if(start>itr->vm_start && start<=itr->vm_end-1 && end>=itr->vm_end-1) {
			itr->vm_end = start;
			prev = itr;
			itr = itr->vm_next;
		}
		else if(start>=itr->vm_start && end<=itr->vm_end-1) {
			node = create_vm_area(end+1,itr->vm_end,itr->access_flags,NORMAL_PAGE_MAPPING);
			node->vm_next = itr->vm_next;
			itr->vm_next = node;
			itr->vm_end = start;
			break;
		}
		else {
			prev = itr;
			itr = itr->vm_next;
		}
	}
	node = create_vm_area(start,end+1,prot,HUGE_PAGE_MAPPING);
	head = insert_into_list(node,head);
	merge_nodes(head);
	//inserting of vm_area done
	//handling physical memory
	//check if any entry already allocated. If not, we can simply return
	// or if entry.. then allocate and copy
	u64 x =start;
	while(x<=end) {
		int isPresent = check_if_hugepage_create(current,x);
		if(isPresent) { //allocates the hugepage since entries present
			u64 hpaddr = (u64)os_hugepage_alloc();
			u64 hppfn = (u64)get_hugepage_pfn((void *)hpaddr);
			u64 pfn;
			u64 i = 0; // maintains size of pgframe copied
			while(i<HPGSIZE) {
				u64 address = x+i;
				if(check_if_exists(current,address)) {
					pfn = get_pfn_number(current,address);
					void* pfn_ptr = osmap(pfn);
					memcpy((char*)(hpaddr+i),(char*)(pfn_ptr),PGSIZE);
					// free the frame
					os_pfn_free(USER_REG,pfn);
					maintain_page_table(current,address);
					tlb_flush(address,PGSIZE);
				}
				i += PGSIZE;
			}
			// since it is present...make the entry in the page table
			install_hugepage_pfn(current,x,prot,hppfn);

		}
		x += HPGSIZE;
	}
	//I guess this is it

	return start;
}


/**
 * break_system call implemenation
 */
int vm_area_break_hugepage(struct exec_context *current, void *addr, u32 length)
{
	//trivial checks
	if(current == NULL ||length<=0 || addr == NULL) return -EINVAL; //invalid parameters
	u64 address = (u64) addr; // typecast -_-

	//check if the address and length is 2MB aligned
	if(address % HPGSIZE !=0) return -EINVAL;
	if(length % HPGSIZE !=0) return -EINVAL;

	u64 start = address, end = address + length -1; //end inclusive
	if(start>=end) return -1; 
	struct vm_area* head = current->vm_area, *itr = NULL, *node = NULL, *node1 =NULL;
	// address and length is now aligned
	// modify the vm_area

	itr = head;
	while(itr!=NULL) {
		if(start<=itr->vm_start && end>=itr->vm_end-1 && itr->mapping_type == HUGE_PAGE_MAPPING) {
			itr->mapping_type = NORMAL_PAGE_MAPPING;
			u64 x = itr->vm_start;
			while(x<=itr->vm_end-1) {
				if(check_if_huge_page_exists(current,x)) { // if not...will be done later by lazy allocation
					u64 hppfn = get_hugepage_pfn_number(current,x); //sets the present bit and 7th bit to 0
					u64 hpaddr = (u64)hppfn*HPGSIZE;
					tlb_flush(x,HPGSIZE);
					for(int i=0;i<512;i++) {
						u64 pfn = install_pfn(current,x+PGSIZE*i,itr->access_flags);
						void* pfn_ptr = osmap(pfn);
						memcpy((char*)(pfn_ptr),(char*)(hpaddr+i*PGSIZE),PGSIZE);
					}
					os_hugepage_free((void*)hpaddr);
					maintain_page_table(current,x);
				}
				x += HPGSIZE;
			}
		}
		else if(start<=itr->vm_start && end>=itr->vm_start && end<itr->vm_end-1 && itr->mapping_type == HUGE_PAGE_MAPPING) {
			node = create_vm_area(end+1,itr->vm_end,itr->access_flags,HUGE_PAGE_MAPPING);
			u64 x = itr->vm_start;
			while(x<=end) {
				if(check_if_huge_page_exists(current,x)) { // if not...will be done later by lazy allocation
					u64 hppfn = get_hugepage_pfn_number(current,x); //sets the present and 7th bit to 0
					u64 hpaddr = (u64)hppfn*HPGSIZE;
					tlb_flush(x,HPGSIZE);
					for(int i=0;i<512;i++) {
						u64 pfn = install_pfn(current,x+PGSIZE*i,itr->access_flags);
						void* pfn_ptr = osmap(pfn);
						memcpy((char*)(pfn_ptr),(char*)(hpaddr+i*PGSIZE),PGSIZE);
					}
					os_hugepage_free((void*)hpaddr);
					maintain_page_table(current,x);
				}
				x += HPGSIZE;
			}
			itr->vm_end = end+1;
			itr->mapping_type = NORMAL_PAGE_MAPPING;
			node->vm_next = itr->vm_next;
			itr->vm_next = node;
		}
		else if(start>itr->vm_start && start<=itr->vm_end-1 && end>=itr->vm_end-1 && itr->mapping_type == HUGE_PAGE_MAPPING) {
			node = create_vm_area(start,itr->vm_end,itr->access_flags,NORMAL_PAGE_MAPPING);
			u64 x = start;
			while(x<=itr->vm_end-1) {
				if(check_if_huge_page_exists(current,x)) { // if not...will be done later by lazy allocation
					u64 hppfn = get_hugepage_pfn_number(current,x); //sets the present bit to 0
					u64 hpaddr = (u64)hppfn*HPGSIZE;
					tlb_flush(x,HPGSIZE);
					for(int i=0;i<512;i++) {
						u64 pfn = install_pfn(current,x+PGSIZE*i,itr->access_flags);
						void* pfn_ptr = osmap(pfn);
						memcpy((char*)(pfn_ptr),(char*)(hpaddr+i*PGSIZE),PGSIZE);
					}
					os_hugepage_free((void*)hpaddr);
					maintain_page_table(current,x);
				}
				x += HPGSIZE;
			}
			node->vm_next = itr->vm_next;
			itr->vm_next = node;
			itr->vm_end = start;
		}
		else if(start>itr->vm_start && end< itr->vm_end-1 && itr->mapping_type == HUGE_PAGE_MAPPING) {
			node = create_vm_area(start,end+1,itr->access_flags,NORMAL_PAGE_MAPPING);
			node1 = create_vm_area(end+1,itr->vm_end,itr->access_flags,HUGE_PAGE_MAPPING);
			u64 x = start;
			while(x<=end) {
				if(check_if_huge_page_exists(current,x)) { // if not...will be done later by lazy allocation
					u64 hppfn = get_hugepage_pfn_number(current,x); //sets the present bit to 0
					u64 hpaddr = (u64)hppfn*HPGSIZE;
					tlb_flush(x,HPGSIZE);
					for(int i=0;i<512;i++) {
						u64 pfn = install_pfn(current,x+PGSIZE*i,itr->access_flags);
						void* pfn_ptr = osmap(pfn);
						memcpy((char*)(pfn_ptr),(char*)(hpaddr+i*PGSIZE),PGSIZE);
					}
					os_hugepage_free((void*)hpaddr);
					maintain_page_table(current,x);
				}
				x += HPGSIZE;
			}
			node1->vm_next = itr->vm_next;
			node->vm_next = node1;
			itr->vm_next = node;
			itr->vm_end = start;
		}
		itr = itr->vm_next;
	}
	merge_nodes(head);


	//modification of VM areas done and physical memory allocation done
	//Please be over >_<
	return 0;//success
}
