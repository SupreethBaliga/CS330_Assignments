#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/wait.h>

int executeCommand (char *cmd) {
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
	int pid = fork();
	if(pid<0) {
		printf("UNABLE TO EXECUTE\n");
		exit(-1);
	}
	if(!pid) {
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
		printf("UNABLE TO EXECUTE\n");
		exit(-1);
	}
	int status=0;
	pid_t cpid = wait(&status);
	if(WEXITSTATUS(status)==255) return -1;
	else return 0;
}

int main (int argc, char *argv[]) {
	return executeCommand(argv[1]);
}
