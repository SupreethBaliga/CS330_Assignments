#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<fcntl.h>
#include <sys/stat.h>
#include<sys/wait.h>


void executeCommand (char *cmd, int idx,int** pipfd) {
	char* t = strtok(cmd," ");
	char* command = t;
	char* args[50];
	args[0] = command;
	int i=1;
	t = strtok(NULL," ");
	while(t!=NULL) {
		args[i]=t;
		t = strtok(NULL," ");
		i++;
	}
	args[i]=NULL;
	dup2(pipfd[idx][1],1);
	int pid = fork();
	if(pid<0) {
		exit(-1);
	}
	if(!pid) {
		// close(1);
		// dup(pipfd[idx][1]);
		char* environ_path = getenv("CS330_PATH");
		t = strtok(environ_path,":");
		while(t!=NULL) {
			char path[5000];
			path[0]='\0';
			strcat(path,t);	
			strcat(path,"/");
			strcat(path,command);
			strcat(path,"\0");
			if(execv(path,args)) {
				t = strtok(NULL,":");
			}
		}
		exit(-1);
	}
	return;
}


int execute_in_parallel(char *infile, char *outfile) {
	int input = open(infile,O_RDONLY);
	int output = open(outfile,O_RDWR | O_CREAT,0777);
	int iferror=0;
	dup2(output,1);
	struct stat sbuf;
	stat(infile,&sbuf);
	int sz = sbuf.st_size;
	char input_text[sz+1];
	int read_bytes = read(input,input_text,sz);
	input_text[sz]='\0';

	char* commands[51]; int idx=0;
	char* t = strtok(input_text,"\n");
	while(t!=NULL) {
		commands[idx] = t;
		idx++;
		t = strtok(NULL,"\n");
	}

	int** pipfd = (int**)malloc(idx*sizeof(int*));
	for(int i=0;i<idx;i++) {
		pipfd[i] = (int*)malloc(2*sizeof(int));
	}
	for(int l=0;l<idx;l++) {
		if(pipe(pipfd[l])<0) {
			return -1;
		}
	}
	for(int l=0;l<idx;l++) {
		executeCommand(commands[l],l,pipfd);
	}
	//parent
	for(int l=0;l<idx;l++) close(pipfd[l][1]);
	dup2(output,1);
	char temp_pipe_output[40000];
	for(int l=0;l<idx;l++) {
		// temp_pipe_output[read(pipfd[l][0],temp_pipe_output,10000)]
		int k = read(pipfd[l][0],temp_pipe_output,10000);
		temp_pipe_output[k]='\0';
		if(k==0) {
			printf("UNABLE TO EXECUTE\n");
			iferror=-1;
		}
		else {
			printf("%s",temp_pipe_output);
		}
		close(pipfd[l][0]);
	}
	return iferror;
}

int main(int argc, char *argv[])
{
	return execute_in_parallel(argv[1], argv[2]);
}
