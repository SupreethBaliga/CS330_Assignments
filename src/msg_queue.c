#include <msg_queue.h>
#include <context.h>
#include <memory.h>
#include <file.h>
#include <lib.h>
#include <entry.h>



/************************************************************************************/
/***************************Do Not Modify below Functions****************************/
/************************************************************************************/

void remove_msg_from_array(struct message* msg_buffer, int idx , int sz) {
	for(int i=idx;i<sz-1;i++) {
		msg_buffer[i] = msg_buffer[i+1];
	}
	return;
}

struct msg_queue_info *alloc_msg_queue_info()
{
	struct msg_queue_info *info;
	info = (struct msg_queue_info *)os_page_alloc(OS_DS_REG);
	
	if(!info){
		return NULL;
	}
	return info;
}

void free_msg_queue_info(struct msg_queue_info *q)
{
	os_page_free(OS_DS_REG, q);
}

struct message *alloc_buffer()
{
	struct message *buff;
	buff = (struct message *)os_page_alloc(OS_DS_REG);
	if(!buff)
		return NULL;
	return buff;	
}

void free_msg_queue_buffer(struct message *b)
{
	os_page_free(OS_DS_REG, b);
}

/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/


int do_create_msg_queue(struct exec_context *ctx)
{
	/** 
	 * TODO Implement functionality to
	 * create a message queue
	 **/
	// first check for errors
	if(ctx==NULL) return -EINVAL; // since invalid parameter

	int fd=3; //as answered by instructor on piazza
	while(fd<MAX_OPEN_FILES && ctx->files[fd]!=NULL) fd++;
	if(fd==MAX_OPEN_FILES) return -EOTHERS; // max file limit reached

	struct file* ff = alloc_file();
	if(ff == NULL) return -ENOMEM;

	ff->type = MSG_QUEUE;
	ff->ref_count = 1;
	ff->inode = NULL;
	ff->fops = NULL;
	ff->pipe = NULL;
	struct msg_queue_info* msg_ptr = alloc_msg_queue_info();
	if(msg_ptr==NULL) return -ENOMEM;
	/* Initialising the msg_ptr */

	msg_ptr->msg_buffer = alloc_buffer(); 
	if(msg_ptr->msg_buffer==NULL) return -ENOMEM;
	
	msg_ptr->messages=0;

	// msg_ptr->members = alloc_buffer();
	// if(msg_ptr->members==NULL) return -ENOMEM;
	for(int i=0;i<MAX_MEMBERS;i++) msg_ptr->members.member_pid[i]=-1;
	msg_ptr->members.member_pid[0] = ctx->pid;
	msg_ptr->members.member_count=1;

	for(int i=0;i<15;i++) {
		for(int j=0;j<15;j++) msg_ptr->blocking[i][j]=0; // initialising the blocking behaviours
	}
	/* Check if any more initialisations required over here as the msg_queu info block progresses ***********************/
	ff->msg_queue = msg_ptr;
	ctx->files[fd] = ff;
	return fd;
}


int do_msg_queue_rcv(struct exec_context *ctx, struct file *filep, struct message *msg)
{
	/** 
	 * TODO Implement functionality to
	 * recieve a message
	 **/

	// trivial error checks
	if(ctx==NULL) return -EINVAL;
	if(filep==NULL || filep->msg_queue==NULL) return -EINVAL;

	struct msg_queue_info* msg_ptr = filep->msg_queue;
	// check if the ctx belongs to this msg_queue
	struct msg_queue_member_info mem = msg_ptr->members;
	int isPresent = 0;
	for(int i=0;i<MAX_MEMBERS;i++) {
		if(mem.member_pid[i]==ctx->pid) isPresent=1;
	}
	if(isPresent==0) {
		return -EINVAL; ///check this again very carefully/********************************************
	}
	// for(int j=0;j<msg_ptr->messages;j++) {
	// 	struct message temp = msg_ptr->msg_buffer[j];
	// 	printk("From_pid: %d, To_pid: %d, Message: %s\n",temp.from_pid,temp.to_pid,temp.msg_txt);
	// }
	// printk("\n");
	// printk("PIDS inside\n");
	for(int i=0;i<MAX_MEMBERS;i++) {
	
	}
	for(int i=0;i<msg_ptr->messages;i++) {
		struct message curr = msg_ptr->msg_buffer[i];
		if(curr.to_pid==ctx->pid) {
			*msg = curr;
			remove_msg_from_array(msg_ptr->msg_buffer,i,msg_ptr->messages);
			msg_ptr->messages--;
			return 1;
		}
	}
	return 0;
}


int do_msg_queue_send(struct exec_context *ctx, struct file *filep, struct message *msg)
{
	/** 
	* TODO Implement functionality to
	* send a message
	**/
	// trivial checks
	if(ctx==NULL) return -EINVAL;
	if(filep==NULL || filep->msg_queue==NULL) return -EINVAL;
	if(ctx->pid!=msg->from_pid) return -EINVAL;
	struct msg_queue_info* msg_ptr = filep->msg_queue;
	struct msg_queue_member_info mem = msg_ptr->members;
	// check if from_pid belongs to the msg_queue
	int isPresent=0;
	for(int i=0;i<MAX_MEMBERS;i++) {
		if(mem.member_pid[i]==ctx->pid) isPresent=1;
	}
	if(isPresent==0) return -EINVAL;
	//check if to_pid belongs to the msg_queue
	if(msg->to_pid != BROADCAST_PID) {
		isPresent=0;
		for(int i=0;i<MAX_MEMBERS;i++) {
			if(mem.member_pid[i]==msg->to_pid) isPresent=1;
		}
		if(isPresent==0) return -EINVAL;
	}
	int from,to;
	//checking if msg blocked
	if(msg->to_pid != BROADCAST_PID) {
		for(int i=0;i<MAX_MEMBERS;i++) {
			if(mem.member_pid[i]==ctx->pid) from = i;
			if(mem.member_pid[i]==msg->to_pid) to = i;
		}
		if(msg_ptr->blocking[from][to] == 1) {
			return -EINVAL;
		}
	}
	//checks done
	int num_delivered=0;
	if(msg->to_pid==BROADCAST_PID) {
		for(int i=0;i<MAX_MEMBERS;i++) {
			if(mem.member_pid[i]!=ctx->pid && mem.member_pid[i]!=-1) {
				if(msg_ptr->blocking[from][i]==1) continue;
				struct message curr = *msg;
				curr.from_pid = ctx->pid;
				curr.to_pid = mem.member_pid[i];
				msg_ptr->msg_buffer[msg_ptr->messages] = curr;
				msg_ptr->messages++;
				num_delivered++;
			}
		}
	}
	else {
		struct message curr = *msg;
		curr.from_pid = ctx->pid;
		msg_ptr->msg_buffer[msg_ptr->messages] = curr;
		msg_ptr->messages++;
		num_delivered++;
	}
	return num_delivered;
}

void do_add_child_to_msg_queue(struct exec_context *child_ctx)
{
	/** 
	 * TODO Implementation of fork handler 
	 **/
	if(child_ctx==NULL) return;
	for(int fd=3;fd<MAX_OPEN_FILES;fd++) {
		if(child_ctx->files[fd]==NULL) continue;
		if(child_ctx->files[fd]->msg_queue==NULL) continue;
		child_ctx->files[fd]->ref_count++;
		struct file* filep = child_ctx->files[fd];
		struct msg_queue_info* msg_ptr = child_ctx->files[fd]->msg_queue;
		for(int i=0;i<MAX_MEMBERS;i++) {
			if(msg_ptr->members.member_pid[i]==-1) {
				msg_ptr->members.member_pid[i] = child_ctx->pid;
				// for(int j=0;j<MAX_MEMBERS;j++) {
				// 	printk("Pid at %d: %d\n",j,msg_ptr->members.member_pid[j]);
				// }
				break;
			}
		}
		// printk("\n");
		msg_ptr->members.member_count++;
	}
	return;
}

void do_msg_queue_cleanup(struct exec_context *ctx)
{
	/** 
	 * TODO Implementation of exit handler 
	 **/
	//trivial check
	if(ctx==NULL) return;
	
	for(int i=0;i<MAX_OPEN_FILES;i++) {
		if(ctx->files[i]==NULL) continue;
		if(ctx->files[i]->msg_queue==NULL) continue;
		int dummy = do_msg_queue_close(ctx,i);
	}
	return;
}

int do_msg_queue_get_member_info(struct exec_context *ctx, struct file *filep, struct msg_queue_member_info *info)
{
	/** 
	 * TODO Implementation of exit handler 
	 **/
	//trivial checks
	if(ctx==NULL) return -EINVAL;
	if(filep==NULL || filep->msg_queue==NULL) return -EINVAL;
	struct msg_queue_member_info mems = filep->msg_queue->members;
	// check if the ctx belongs to the given message queue
	int isPresent = 0;
	for(int i=0;i<MAX_MEMBERS;i++) {
		if(mems.member_pid[i]==ctx->pid) isPresent=1;
	}
	if(isPresent==0) return -EINVAL;

	//checks done
	info->member_count = mems.member_count;
	int j=0;
	for(int i=0;i<MAX_MEMBERS;i++) {
		if(mems.member_pid[i]==-1) continue;
		info->member_pid[j] = mems.member_pid[i];
		j++;
	}
	return 0;
}


int do_get_msg_count(struct exec_context *ctx, struct file *filep)
{
	/** 
	 * TODO Implement functionality to
	 * return pending message count to calling process
	 **/
	//trivial checks
	if(ctx==NULL) return -EINVAL;
	if(filep==NULL || filep->msg_queue==NULL) return -EINVAL;
	struct msg_queue_info* msg_ptr = filep->msg_queue;
	struct msg_queue_member_info mems = msg_ptr->members;
	// check if ctx belongs to the msg_queue
	int isPresent = 0;
	for(int i=0;i<MAX_MEMBERS;i++) {
		if(mems.member_pid[i]==ctx->pid) isPresent=1;
	}
	if(isPresent==0) return -EINVAL;
	//checks done
	int count=0;
	for(int i=0;i<msg_ptr->messages;i++) {
		if(msg_ptr->msg_buffer[i].to_pid == ctx->pid) count++;
	}
	return count;
}

int do_msg_queue_block(struct exec_context *ctx, struct file *filep, int pid)
{
	/** 
	 * TODO Implement functionality to
	 * block messages from another process 
	 **/
	// trivial checks
	if(ctx==NULL) return -EINVAL;
	if(filep==NULL || filep->msg_queue==NULL) return -EINVAL;
	struct msg_queue_info* msg_ptr = filep->msg_queue;
	struct msg_queue_member_info mems = msg_ptr->members;
	//Check if both of them exist in the msg queue
	int from,to;
	//check if to_pid exists and assign to to
	int isPresent=0;
	for(int i=0;i<MAX_MEMBERS;i++) {
		if(mems.member_pid[i]==ctx->pid) {
			isPresent = 1;
			to = i;
			break;
		}
	}
	if(isPresent==0) return -EINVAL;
	isPresent = 0;
	// check if from_pid exists in msg_queue
	for(int i=0;i<MAX_MEMBERS;i++) {
		if(mems.member_pid[i]==pid) {
			isPresent = 1;
			from = i;
		}
	}
	if(isPresent==0) return -EINVAL;
	msg_ptr->blocking[from][to]=1;
	for(int i=0;i<MAX_MEMBERS;i++) {
		for(int j=0;j<MAX_MEMBERS;j++) {

		}
	}
	return 0;
}

int do_msg_queue_close(struct exec_context *ctx, int fd)
{
	/** 
	 * TODO Implement functionality to
	 * remove the calling process from the message queue 
	 **/
	//trivial checks
	if(ctx==NULL) return -EINVAL;
	struct file* filep = ctx->files[fd];
	if(filep==NULL || filep->msg_queue==NULL) return -EINVAL;
	struct msg_queue_info* msg_ptr = filep->msg_queue;
	struct msg_queue_member_info mems = msg_ptr->members;
	//check if the process belongs to that message queue
	int idx= -1;
	for(int i=0;i<MAX_MEMBERS;i++) {
		if(mems.member_pid[i] == ctx->pid) idx=i;
	}
	if(idx==-1) return -EINVAL;

	//checks done
	//remove the process from the members
	mems.member_pid[idx]=-1;
	mems.member_count--;


	// for(int i=0;i<msg_ptr->messages;i++) {
	// 	struct message curr  = msg_ptr->msg_buffer[i];
	// 	printk("from: %d, to: %d, msg: %s\n",curr.from_pid,curr.to_pid,curr.msg_txt);
	// }
	//remove all messages addressed to that process
	for(int i=0;i<msg_ptr->messages;i++) {
		struct message curr = msg_ptr->msg_buffer[i];
		if(curr.to_pid == ctx->pid) {
			remove_msg_from_array(msg_ptr->msg_buffer,i,msg_ptr->messages);
			msg_ptr->messages--;
			i--;
		}
	}


	//remove all blocks related to that process
	for(int i=0;i<MAX_MEMBERS;i++) msg_ptr->blocking[i][idx]=0;
	for(int i=0;i<MAX_MEMBERS;i++) msg_ptr->blocking[idx][i]=0;
	// printk("After closing\n");
	// for(int i=0;i<msg_ptr->messages;i++) {
	// 	struct message curr  = msg_ptr->msg_buffer[i];
	// 	printk("from: %d, to: %d, msg: %s\n",curr.from_pid,curr.to_pid,curr.msg_txt);
	// }
	if(mems.member_count==0) {
		free_msg_queue_buffer(msg_ptr->msg_buffer);
		free_msg_queue_info(msg_ptr);
	}
	int retval=0;
	retval = do_file_close(ctx->files[fd]);

	//remove file object from the file descriptors of the process
	ctx->files[fd] = NULL;
	return retval;
}
