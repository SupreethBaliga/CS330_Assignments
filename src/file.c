#include<types.h>
#include<context.h>
#include<file.h>
#include<lib.h>
#include<serial.h>
#include<entry.h>
#include<memory.h>
#include<fs.h>
#include<kbd.h>


/************************************************************************************/
/***************************Do Not Modify below Functions****************************/
/************************************************************************************/

void free_file_object(struct file *filep)
{
	if(filep)
	{
		os_page_free(OS_DS_REG ,filep);
		stats->file_objects--;
	}
}

struct file *alloc_file()
{
	struct file *file = (struct file *) os_page_alloc(OS_DS_REG); 
	file->fops = (struct fileops *) (file + sizeof(struct file)); 
	bzero((char *)file->fops, sizeof(struct fileops));
	file->ref_count = 1;
	file->offp = 0;
	stats->file_objects++;
	return file; 
}

void *alloc_memory_buffer()
{
	return os_page_alloc(OS_DS_REG); 
}

void free_memory_buffer(void *ptr)
{
	os_page_free(OS_DS_REG, ptr);
}

/* STDIN,STDOUT and STDERR Handlers */

/* read call corresponding to stdin */

static int do_read_kbd(struct file* filep, char * buff, u32 count)
{
	kbd_read(buff);
	return 1;
}

/* write call corresponding to stdout */

static int do_write_console(struct file* filep, char * buff, u32 count)
{
	struct exec_context *current = get_current_ctx();
	return do_write(current, (u64)buff, (u64)count);
}

long std_close(struct file *filep)
{
	filep->ref_count--;
	if(!filep->ref_count)
		free_file_object(filep);
	return 0;
}
struct file *create_standard_IO(int type)
{
	struct file *filep = alloc_file();
	filep->type = type;
	if(type == STDIN)
		filep->mode = O_READ;
	else
		filep->mode = O_WRITE;
	if(type == STDIN){
		filep->fops->read = do_read_kbd;
	}else{
		filep->fops->write = do_write_console;
	}
	filep->fops->close = std_close;
	return filep;
}

int open_standard_IO(struct exec_context *ctx, int type)
{
	int fd = type;
	struct file *filep = ctx->files[type];
	if(!filep){
		filep = create_standard_IO(type);
	}else{
		filep->ref_count++;
		fd = 3;
		while(ctx->files[fd])
			fd++; 
	}
	ctx->files[fd] = filep;
	return fd;
}
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/

/* File exit handler */
void do_file_exit(struct exec_context *ctx)
{
	/*TODO the process is exiting. Adjust the refcount
	of files*/
	if(ctx==NULL) return;
	for(int i=0;i<MAX_OPEN_FILES;i++) { // clearing all the files array in ctx
		if(ctx->files[i]!=NULL) { // open file object - so close
			do_file_close(ctx->files[i]);
			ctx->files[i]=NULL;
		}
	}
	return;
}

/*Regular file handlers to be written as part of the assignmemnt*/


static int do_read_regular(struct file *filep, char * buff, u32 count)
{
	/** 
	*  TODO Implementation of File Read, 
	*  You should be reading the content from File using file system read function call and fill the buf
	*  Validate the permission, file existence, Max length etc
	*  Incase of Error return valid Error code 
	**/
	int ret_fd = -EINVAL; 
	if(!filep) return -EINVAL; // validate file existence
	if(filep->mode && O_READ ==0 ) return -EACCES; //check read permission

	int rbytes = flat_read(filep->inode,buff,count,&filep->offp);
	if(rbytes<0) return -EOTHERS;
	// flat_read does not update offset. So we update it manually
	filep->offp += rbytes;

	ret_fd = rbytes; // since we have to return the number of bytes read
	return ret_fd;
}

/*write call corresponding to regular file */

static int do_write_regular(struct file *filep, char * buff, u32 count)
{
	/** 
	*   TODO Implementation of File write, 
	*   You should be writing the content from buff to File by using File system write function
	*   Validate the permission, file existence, Max length etc
	*   Incase of Error return valid Error code 
	* */
	int ret_fd = -EINVAL; 
	if(!filep) return -EINVAL; // no existence of file so invalid argument
	if(filep->mode & O_WRITE == 0) return -EACCES; // no write permission

	int wbytes = flat_write(filep->inode, buff, count, &filep->offp);

	// if error, wbytes<0
	if(wbytes<0) return -ENOMEM; //since error in memory allocation
	// we have to update offset manually
	filep->offp += wbytes;
	ret_fd = wbytes; // since we have to return the number of bytes read
	return ret_fd;
}

long do_file_close(struct file *filep)
{
	/** TODO Implementation of file close  
	*   Adjust the ref_count, free file object if needed
	*   Incase of Error return valid Error code 
	*/
	if(filep == NULL) return -EINVAL;
	
	if(filep->ref_count<=0) return -EOTHERS;

	if(filep->ref_count==1) {
		free_file_object(filep);
		//if(filep!=NULL) return -EOTHERS;
		filep = NULL;
		return 0; // since no error
	}
	else {
		filep->ref_count--;
		return 0; // since no error
	}
}

static long do_lseek_regular(struct file *filep, long offset, int whence)
{
	/** 
	*   TODO Implementation of lseek 
	*   Set, Adjust the ofset based on the whence
	*   Incase of Error return valid Error code 
	* */
	int ret_fd = -EINVAL; 
	if(filep==NULL) return -EINVAL; // since invalid argument
	// check for errors where file pointer is set beyond the file end

	long file_end=0;
	if(whence==SEEK_SET) file_end = offset;
	else if(whence==SEEK_CUR) file_end = filep->offp + offset;
	else if(whence==SEEK_END) file_end = filep->inode->file_size+offset;
	// dprintk("%d",file_end);
	if(file_end<0 || file_end > filep->inode->file_size) return -EINVAL;
	// modify the file pointers if no error
	
	filep->offp = file_end;
	ret_fd = filep->offp; //since no error
	return ret_fd;
}

extern int do_regular_file_open(struct exec_context *ctx, char* filename, u64 flags, u64 mode)
{

	/**  
	*  TODO Implementation of file open, 
	*  You should be creating file(use the alloc_file function to creat file), 
	*  To create or Get inode use File system function calls, 
	*  Handle mode and flags 
	*  Validate file existence, Max File count is 16, Max Size is 4KB, etc
	*  Incase of Error return valid Error code 
	* */
	int ret_fd = -EINVAL; 
	// check if context exists(just to be safe)
	if(ctx==NULL) return -EINVAL; // since invalid arguments
	
	struct inode *curr = lookup_inode(filename);

	if(curr==NULL) {
		// nofile so create...only if O_CREAT passed
		if((O_CREAT & flags) == 0) return -EINVAL;
		curr = create_inode(filename,mode);
		if(!curr) return -ENOMEM; // error in memory allocation
	}
	
	//checking permissions

	if(flags & O_READ) {
		if((curr->mode & O_READ) == 0) return -EACCES;
	}

	if(flags & O_WRITE) {
		if((curr->mode & O_WRITE) == 0) return -EACCES;
	}

	if(flags & O_EXEC) {
		if((curr->mode & O_EXEC) == 0) return -EACCES;
	}

	// for file descriptor, leave 0,1,2 as standard file descriptors, start from 3
	int fd=3;
	while(fd<MAX_OPEN_FILES && ctx->files[fd]!=NULL) fd++; //max-file = 16 (given)
	
	if(fd==MAX_OPEN_FILES) return -EOTHERS; // max files reached

	// create new file object
	struct file* ff = alloc_file();

	if(ff==NULL) return -ENOMEM; // error in memory

	// fill the fields in file structure
	ff->type = REGULAR;
	ff->mode = flags;
	ff->offp = 0;
	ff->ref_count = 1; // CHECK THIS ONCE - ChEcKeD /****************************************************************************
	ff->inode = curr;
	ff->fops->read = do_read_regular;
	ff->fops->write = do_write_regular;
	ff->fops->lseek = do_lseek_regular;
	ff->fops->close = do_file_close;
	//assign the file to corresponding file descriptor in the context
	ctx->files[fd] = ff;
	ret_fd = fd;
	// return the file descriptor
	return ret_fd;
}

/**
 * Implementation dup 2 system call;
 */
int fd_dup2(struct exec_context *current, int oldfd, int newfd)
{
	/** 
	*  TODO Implementation of the dup2 
	*  Incase of Error return valid Error code 
	**/
	int ret_fd = -EINVAL; 
	// error checking
	//1. check if context exists (just to be safe) 
	if(current == NULL) return -EINVAL; // since invalid arguments
	//2. Check if both oldfd and newfd are valid
	if(oldfd<0 || newfd<0) return -EINVAL; //since invalid arguments
	if(newfd>=MAX_OPEN_FILES) return -EINVAL; //invalid argument since greater than max_open_files
	//3. check if oldfd is closed, if yes error
	if(current->files[oldfd] == NULL) return -EINVAL; //since invalid arguments

	// if newfd, is open, I have to close it
	long long err;
	if(current->files[newfd] != NULL) err = do_file_close(current->files[newfd]);
	if(err<0) return -EOTHERS; // since some error in closing file object

	//  copy oldfd fileobject to newfd
	current->files[newfd] = current->files[oldfd]; 

	// increasing ref_count // DO CHECK THIS - ChEcKed/***********************************************************************
	current->files[oldfd]->ref_count++; // since file copy increased
	ret_fd = newfd; // since dup2 returns new file descriptor
	return ret_fd;
}

int do_sendfile(struct exec_context *ctx, int outfd, int infd, long *offset, int count) {
	/** 
	*  TODO Implementation of the sendfile 
	*  Incase of Error return valid Error code 
	**/
	// check trivial errors
	if(ctx==NULL) return -EINVAL;
	// check the validity of infd and outfd
	if(ctx->files[infd]==NULL || ctx->files[outfd]==NULL) return -EINVAL;
	// check if opened for reading and writing resp.
	if(ctx->files[infd]->mode & O_READ == 0) return -EACCES;
	if(ctx->files[outfd]->mode & O_WRITE ==0) return -EACCES;
	//valid pointer, so we store the initial value of file pointer of infd
	long init_offset = ctx->files[infd]->offp;
	// printk("Init Offset of Input file : %d\n",init_offset);
	if(offset==NULL) {
		// printk("Offset passed was NULL\n");
		char* buff = alloc_memory_buffer();
		int rbytes = do_read_regular(ctx->files[infd],buff,count);
		if(rbytes<0) {
			free_memory_buffer(buff);
			return rbytes;
		}
		if(rbytes+ctx->files[outfd]->offp>FILE_SIZE) {
			rbytes = FILE_SIZE - ctx->files[outfd]->offp;
		}
		// printk("Bytes read from input file: %d\n",rbytes);
		int wbytes = do_write_regular(ctx->files[outfd],buff,rbytes);
		// printk("Bytes written to output file: %d\n",wbytes);
		if(wbytes<=0) ctx->files[infd]->offp = init_offset;
		else ctx->files[infd]->offp = init_offset + wbytes;
		// printk("Final offset of the input file: %d\n",ctx->files[infd]->offp);
		free_memory_buffer(buff);
		return wbytes;
	}
	else {
		// printk("Offset passed was NOT NULL\n");
		// check if offset is a valid one
		long offset_value = *offset;
		if(offset_value<0 || offset_value>ctx->files[infd]->inode->file_size) return -EINVAL; // if invalid file pointer
		// printk("Offset value was valid\n");

		ctx->files[infd]->offp = *offset;
		char* buff = alloc_memory_buffer();
		int rbytes = do_read_regular(ctx->files[infd],buff,count);
		// printk("Bytes read from input file: %d\n",rbytes);
		ctx->files[infd]->offp = init_offset;
		if(rbytes<0) {
			free_memory_buffer(buff);
			return rbytes;
		}
		if(rbytes+ctx->files[outfd]->offp>FILE_SIZE) {
			rbytes = FILE_SIZE - (int)ctx->files[outfd]->offp;
		}
		int wbytes = do_write_regular(ctx->files[outfd],buff,rbytes);
		if(wbytes<=0) {
			*offset = offset_value;
			ctx->files[infd]->offp = init_offset;
		}
		else {
			*offset = offset_value + wbytes;
			ctx->files[infd]->offp = init_offset;
		} // reflects change in offset
		// printk("Bytes written to output file: %d\n",wbytes);
		// printk("Final offset of the input file and *offset: %d, %d\n",ctx->files[infd]->offp,*offset);
		free_memory_buffer(buff);
		return wbytes;
	}

}

