#include <debug.h>
#include <context.h>
#include <entry.h>
#include <lib.h>
#include <memory.h>


/*****************************HELPERS******************************************/

/* 
 * allocate the struct which contains information about debugger
 *
 */

void copy_info_registers(struct exec_context* parent, struct exec_context* child) {

	struct registers parent_regs;
	
	parent_regs.r15 = child->regs.r15;
	parent_regs.r14 = child->regs.r14;
	parent_regs.r13 = child->regs.r13;
	parent_regs.r12 = child->regs.r12;
	parent_regs.r11 = child->regs.r11;
	parent_regs.r10 = child->regs.r10;
	parent_regs.r9 = child->regs.r9;
	parent_regs.r8 = child->regs.r8;
	parent_regs.rbp = child->regs.rbp;
	parent_regs.rdi = child->regs.rdi;
	parent_regs.rsi = child->regs.rsi;
	parent_regs.rdx = child->regs.rdx;
	parent_regs.rcx = child->regs.rcx;
	parent_regs.rbx = child->regs.rbx;
	parent_regs.rax = child->regs.rax;
	parent_regs.entry_rip = child->regs.entry_rip-1;
	parent_regs.entry_cs = child->regs.entry_cs;
	parent_regs.entry_rflags = child->regs.entry_rflags;
	parent_regs.entry_rsp = child->regs.entry_rsp;
	parent_regs.entry_ss = child->regs.entry_ss;
	
	parent->dbg->regs = parent_regs;

	return;
}

void save_stack_trace(struct exec_context* parent, struct exec_context* child) {

	//note while running this we are in the child process.
	int count=0;
	u64 *arr = parent->dbg->trace;
	arr[count] = child->regs.entry_rip-1;
	count++;
	u64 base, itr;
	base = child->regs.rbp;
	itr = child->regs.entry_rsp;
	while(*(u64 *)itr != END_ADDR) {
		arr[count] = *(u64 *) itr;
		count++;
		itr = base+8;
		base = *(u64 *)base;
	} 
	parent->dbg->trace_size = count;
	return ;

}

struct debug_info *alloc_debug_info()
{
	struct debug_info *info = (struct debug_info *) os_alloc(sizeof(struct debug_info)); 
	if(info)
		bzero((char *)info, sizeof(struct debug_info));
	return info;
}

/*
 * frees a debug_info struct 
 */
void free_debug_info(struct debug_info *ptr)
{
	if(ptr)
		os_free((void *)ptr, sizeof(struct debug_info));
}

/*
 * allocates memory to store registers structure
 */
struct registers *alloc_regs()
{
	struct registers *info = (struct registers*) os_alloc(sizeof(struct registers)); 
	if(info)
		bzero((char *)info, sizeof(struct registers));
	return info;
}

/*
 * frees an allocated registers struct
 */
void free_regs(struct registers *ptr)
{
	if(ptr)
		os_free((void *)ptr, sizeof(struct registers));
}

/* 
 * allocate a node for breakpoint list 
 * which contains information about breakpoint
 */
struct breakpoint_info *alloc_breakpoint_info()
{
	struct breakpoint_info *info = (struct breakpoint_info *)os_alloc(
		sizeof(struct breakpoint_info));
	if(info)
		bzero((char *)info, sizeof(struct breakpoint_info));
	return info;
}

/*
 * frees a node of breakpoint list
 */
void free_breakpoint_info(struct breakpoint_info *ptr)
{
	if(ptr)
		os_free((void *)ptr, sizeof(struct breakpoint_info));
}

/*
 * Fork handler.
 * The child context doesnt need the debug info
 * Set it to NULL
 * The child must go to sleep( ie move to WAIT state)
 * It will be made ready when the debugger calls wait_and_continue
 */
void debugger_on_fork(struct exec_context *child_ctx)
{
	child_ctx->dbg = NULL;	
	child_ctx->state = WAITING;	
}


/******************************************************************************/

/* This is the int 0x3 handler
 * Hit from the childs context
 */
long int3_handler(struct exec_context *ctx)
{
	//trivial check 
	if(ctx==NULL) return -1;
	//get parent context ppid
	int parent_pid = ctx->ppid;
	struct exec_context* parent = get_ctx_by_pid(parent_pid);

	// attributes to be saved
	u64 addr = ctx->regs.entry_rip;
	copy_info_registers(parent, ctx); // saves the register state
	save_stack_trace(parent,ctx); //saves the function trace
	parent->regs.rax = addr-1; // for return value of wait and continue

	//simulates the push %rbp instruction if the child process at breakpoint
	ctx->regs.entry_rsp -=8;
	u64 stack_top = ctx->regs.entry_rsp;
	*(u64 *) stack_top = ctx->regs.rbp;
	
	ctx->state = WAITING;
	parent->state = READY;
	schedule(parent);
	return -1; //dummy if error
}

/*
 * Exit handler.
 * Called on exit of Debugger and Debuggee 
 */
void debugger_on_exit(struct exec_context *ctx)
{
	//trivial
	if(ctx==NULL) return;
	if(ctx->dbg==NULL) { // exit of debugee - nothing to free
		struct exec_context* parent = get_ctx_by_pid(ctx->ppid);
		parent->state = READY;
		return;
	}
	else { //exit of debugger
		struct breakpoint_info * curr = ctx->dbg->head;
		while(curr!=NULL) {
			struct breakpoint_info* n = curr->next;
			curr->next=NULL;
			free_breakpoint_info(curr);
			curr = n;
		}
		free_debug_info(ctx->dbg);
		ctx->dbg = NULL;
		return;
	}
}

/*
 * called from debuggers context
 * initializes debugger state
 */
int do_become_debugger(struct exec_context *ctx)
{
	if(ctx==NULL) return -1; // invalid
	struct debug_info* dbg = ctx->dbg;
	//initialising the data structures in the debugger
	dbg = alloc_debug_info();
	if(dbg == NULL) return -1; //error in memory allocation

	dbg->head = NULL;	//since initially no node in list
	dbg->breakpoint_num = 0; //since initially zero breakpoints
	dbg->curr_breakpoints = 0; //since no node in the list
	dbg->trace_size = 0; //since none here till now
	ctx->dbg = dbg;
	return 0;
}

/*
 * called from debuggers context
 */
int do_set_breakpoint(struct exec_context *ctx, void *addr)
{
	//trivial checks
	if(ctx==NULL) {
		return -1;
	}
	if(ctx->dbg == NULL) return -1; //since no debugger
	struct debug_info* dbg = ctx->dbg;

	//check if maximum number of breakpoints reached
	if(dbg->curr_breakpoints == MAX_BREAKPOINTS) return -1; //error

	struct breakpoint_info* itr = dbg->head;

	//check if breakpoint at addr already exists
	int isPresent = 0;
	while(itr!=NULL) {
		if(itr->addr == (u64)addr) {
			isPresent = 1;
			itr -> status = 1;
			break;
		}
		else {
			itr = itr->next;
		}
	}
	if(isPresent==1) {
		return 0;
	}


	//not present already so add new breakpoint
	struct breakpoint_info* curr = alloc_breakpoint_info();

	if(curr==NULL) return -1; //error in memory allocation


	//increase fields of dbg
	dbg->breakpoint_num++;
	dbg->curr_breakpoints++;

	//set up new breakpoint
	curr->num = dbg->breakpoint_num;
	curr->status = 1;
	curr->addr = (u64)addr;
	curr->next = NULL;

	//manipulating debuggee's address space
	*(unsigned char*)addr = (0xCC); //1 byte values

	//insert into list
	if(dbg->head == NULL) {
		dbg->head = curr;
	}
	else {
		itr = dbg->head;
		while(itr->next!=NULL) {
			itr = itr->next;
		}
		itr->next = curr;
	}
	return 0;
}

/*
 * called from debuggers context
 */
int do_remove_breakpoint(struct exec_context *ctx, void *addr)
{
	//trivial checks
	if(ctx==NULL) return -1; //invalid
	if(ctx->dbg==NULL) return -1; //no debugger
	struct debug_info* dbg = ctx->dbg;

	struct breakpoint_info* itr = dbg->head;

	//check if breakpoint exists on that address
	int isPresent = 0;
	while(itr!=NULL) {
		if(itr->addr == (u64)addr) {
			isPresent = 1;
			break;
		}
		else {
			itr = itr->next;
		}
	}
	if(isPresent == 0) {
		return -1; //no such breakpoint exists at that addr
	}

	// restore the value at that address
	*(unsigned char *)addr = 0x55;
	//now itr points to the nodes to be removed;
	
	struct breakpoint_info* itr2 = dbg->head;
	if(itr==itr2) {
		dbg->head = itr->next;
		itr->next = NULL;
		free_breakpoint_info(itr);
	}
	else {
		while(itr2->next!=itr) {
			itr2 = itr2->next;
		}
		itr2->next = itr->next;
		itr->next = NULL;
		free_breakpoint_info(itr);
	}
	//decrease the curr_breakpoints to reflect removal
	dbg->curr_breakpoints --;

	//success
	return 0;
}

/*
 * called from debuggers context
 */
int do_enable_breakpoint(struct exec_context *ctx, void *addr)
{
	if(ctx == NULL || ctx->dbg==NULL) return -1;

	//check if breakpoint exists at addr
	struct breakpoint_info* itr = ctx->dbg->head;
	int isPresent=0;
	while(itr!=NULL) {
		if(itr->addr == (u64)addr) {
			itr->status = 1;
			isPresent = 1;
			break;
		}
		else {
			itr = itr->next;
		}
	}
	if(isPresent == 0) return -1; //since breakpoint does not exist at addr

	// breakpoint exists so enable
	*(unsigned char*) addr = 0xCC; //place the breakpoint
	
	return 0; // success
}

/*
 * called from debuggers context
 */
int do_disable_breakpoint(struct exec_context *ctx, void *addr)
{
	// trivial check
	if(ctx == NULL || ctx->dbg==NULL) return -1;

	//check if breakpoint exists at addr
	struct breakpoint_info* itr = ctx->dbg->head;
	int isPresent=0;
	while(itr!=NULL) {
		if(itr->addr == (u64)addr) {
			itr->status = 0;
			isPresent = 1;
			break;
		}
		else {
			itr = itr->next;
		}
	}
	if(isPresent == 0) return -1; //since breakpoint does not exist at addr

	//breakpoint existss, so disable
	*(unsigned char*) addr = 0x55; // put original data

	return 0; //success
}

/*
 * called from debuggers context
 */ 
int do_info_breakpoints(struct exec_context *ctx, struct breakpoint *ubp)
{
	// trivial check
	if(ctx==NULL || ctx->dbg ==NULL) return -1; //since error	
	int count=0;
	struct breakpoint_info* itr = ctx->dbg->head;
	while(itr!=NULL) {
		struct breakpoint curr;
		curr.addr = itr->addr;
		curr.status = itr->status;
		curr.num = itr->num;
		ubp[count] = curr;
		count++;
		itr = itr->next;
	}
	return count;
}

/*
 * called from debuggers context
 */
int do_info_registers(struct exec_context *ctx, struct registers *regs)
{
	//trivial checks
	if(ctx==NULL || ctx->dbg==NULL) return -1;
	*regs = ctx->dbg->regs;
	return 0;
}

/* 
 * Called from debuggers context
 */
int do_backtrace(struct exec_context *ctx, u64 bt_buf)
{
	//trivial check 
	if(ctx==NULL || ctx->dbg ==NULL) return -1;
	u64* ptr = (u64*) bt_buf;
	for(int i=0;i<ctx->dbg->trace_size;i++) {
		ptr[i] = ctx->dbg->trace[i];
	}
	return ctx->dbg->trace_size;
}


/*
 * When the debugger calls wait
 * it must move to WAITING state 
 * and its child must move to READY state
 */

s64 do_wait_and_continue(struct exec_context *ctx)
{
	//ctx is of debugger and child_ctx is of debuggee
	//trivial check
	if(ctx==NULL || ctx->dbg==NULL) return -1;
	struct exec_context *child_ctx = NULL;
	for(int i=0;i<=MAX_PROCESSES;i++) {
		child_ctx = get_ctx_by_pid(i);
		if(child_ctx->ppid == ctx->pid) break;
	}

	if(child_ctx==NULL) {
		return CHILD_EXIT;
	} // since no other process
	if(child_ctx->ppid != ctx->pid) {
		return CHILD_EXIT;
	} //since not its child process
	

	ctx->state = WAITING;
	child_ctx->state = READY;
	schedule(child_ctx);
	return -1; // error if not breakpoint
}

