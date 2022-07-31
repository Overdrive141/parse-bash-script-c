/**
* Author: Farhan H (OverDrive141)
* All Rights Reserved. 
*/ 

#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

char **getargs(char *command){
	
	char delim[2]=" ";
	char **args=malloc(sizeof(command));
	int i = 0;
	args[i++]=strtok(command, delim);
	do{
		args[i++]=strtok(NULL,delim);
	}while(args[i-1] != NULL);
	
	int arrSize=0;
	for(int i = 0; args[i]; i++){
		//trimwhitespace(args[i]);
		arrSize=i+1;
		//printf("Args of %d? %s\n", getpid(),args[i]);

	}

	//printf("Argument Size of Command: %d\n", arrSize);
	args[arrSize]=NULL;

	return args;
}



char* trimwhitespace(char *str_base) {
    char* buffer = str_base;
    
    for(int i =0; i<=strlen(buffer); i++){

        if(i==0 || i==strlen(buffer)){
                    
                if(buffer[0] == ' '){
                    
                     strcpy(buffer, buffer+1);
                }
        } 
    }
    
    return str_base;
}

void firstProcess(int* pipe1, char **command){
	printf("First Process %d . %d  =>  %s\n", pipe1[0], pipe1[1], command[0]);
	char **args;
	args=getargs(command[0]);

	printf("Args Final %s\n", args);

	dup2(pipe1[1],1); // Output to Pipe1
	close(pipe1[0]);
	close(pipe1[1]);
	execvp(command[0], args);
	perror("Bad exec in first");
	exit(1);
}
void middleProcess(int *pipe1, int *pipe2, char **command, int index){

	char **args;
	args=getargs(command[index]);

	printf("Stuck here %d %d\n", pipe1[0], pipe2[1]);

	dup2(pipe1[0],0); // Input from Pipe1
	dup2(pipe2[1],1); // Output to Pipe2
	close(pipe1[0]);
	close(pipe1[1]);
	printf("Here?\n");
	close(pipe2[0]);
	close(pipe2[1]);

	printf("Are we not here yet? %s\n", command[index]);
	
	execvp(command[index], args);
	
	perror("Bad exec in middle");
	exit(1);
}

void lastProcess(int *pipe1, char **command, int index){
	printf("Last Process");
	char **args;
	args=getargs(command[index]);

	close(pipe1[1]);
	dup2(pipe1[0],0); // Input from Pipe1
	close(pipe1[0]);
	execvp(command[index], args);
	perror("Bad exec in last");
	exit(1);
}

int processJob(char *buffer){
   
    int fd1[2];
    int pid;
    int status;

    int count = -1; // count will go to 0 since delimiter gives one command even if no pipe

    char delim[4]="|";

    char **vectorArgument=malloc(10*sizeof(char *));
    int idx=0;       

    vectorArgument[idx++]=strtok(buffer,delim);
    do{
	    vectorArgument[idx++]=strtok(NULL,delim);
    }while(vectorArgument[idx-1] != NULL);


    for(int i =0; vectorArgument[i] ; i++){
    	count++;
	trimwhitespace(vectorArgument[i]); // Remove white spaces from command
    }

    // printf("Count of Pipes %d\n", count);
    // printf("Sizeof %d\n", sizeof(vectorArgument));

    if(count==0){
	   if((pid=fork()) == 0){
		   char **args;
		   args=getargs(vectorArgument[0]);
            if((execvp(vectorArgument[0], args)) == -1){
                perror("Error: Executing Command");
            }
	   }
	   wait(&status);
	   return 0;
    }
    else if(count > 0){
    	

        if(count==1){
            int pid1, pid2;

            if(pipe(fd1) < 0){
                perror("Pipe unsuccessful:\n");
                exit(0);
            }

            if((pid1=fork())==0){
                //printf("Child1 came %d %d\n", fd1[0], fd1[1]);
                firstProcess(fd1, vectorArgument);		
            }
            
            if((pid2=fork())==0){
                //printf("Child2 came\n");
                lastProcess(fd1, vectorArgument, 1);
            }

            close(fd1[0]);
            close(fd1[1]);

            for(int i=0;i<=count;i++){
                //printf("Child terminated");
                wait(&status);
            }  

            return 0;
	
	    } else {
		
            int i, lastPipeDes;

            lastPipeDes = 0; // STDIN FILENO

            for(i=0; i<=count; i++){

                if(pipe(fd1) < 0){
                    perror("Pipe unsuccessful:\n");
                    exit(0);
                }

                pid = fork();
                if(pid==0){
                    //printf("Child w/ PID %d\n", getpid());

                    if (lastPipeDes != STDIN_FILENO) { // if not first process then redirect the previous pipe to STDIN
                        dup2(lastPipeDes, STDIN_FILENO);
                        close(lastPipeDes);
                    }

                    char **args;
	                args=getargs(vectorArgument[i]);

                    if(count==i){
                        // Start last command
                        execvp(vectorArgument[i], args);

                        perror("execvp failed");
                        exit(1);
                    }

                    else{ // if not last process then redirect stdout to current pipe
                        
                        dup2(fd1[1], STDOUT_FILENO);
                        close(fd1[1]);

                        execvp(vectorArgument[i], args);
                        perror("execvp failed");
                        exit(1);
                    }

                }

                // Parent closes read end of last pipe used
                close(lastPipeDes);
                // Parent closes write end of pipe since it is not needed
                close(fd1[1]);
                // Update lastPipeDescriptor with current read end to use in next iteration of loop
                lastPipeDes = fd1[0];


            }


            for(i=0; i<=count; i++) {
                wait(NULL);		
            }

            printf("\n");
            
            //printf("Hi Parent %d\n", getpid());
            return 0;
        }
     }
	printf("I am parent %d. My own pid is %d\n", pid, getpid());
	return 0;
}


int main(int argc, char *argv[]){

    int fd;
    char buffer[255];

    if(argc != 2){
        perror("Usage: ./parseBashScript <scriptFileName.sh>");
        exit(-1);
    }

    if((fd=open(argv[1],O_RDONLY,0777)) == -1){
        perror("Cannot open the file\n");
        exit(1);
    }

    int idx=0, temp=0;
    while(read(fd, &buffer[idx], 1) > 0){	    
        printf("%c", buffer[idx]);	
        temp++;
        int offset = lseek(fd, 0, SEEK_CUR);
        //printf("Current Offset %d\n",offset);	
        if(buffer[idx] == ';' || buffer[idx] == '\n')
        {
            temp=0; // flush buffer to write new line
            printf("\n");
            if(buffer[0] != '#' && 
            buffer[0] != ' ' && 
            buffer[0] != '\n')
            {
               // printf("%c\n",buffer[0]);
                buffer[idx]='\0';
                processJob(buffer);
            }
                //break;
        }

        idx=temp;	

    }

   // for(int i = 0; i<strlen(buffer); i++) printf("%c", buffer[i]);

    close(fd);

    exit(0);

}

/**
* Author: Farhan H (OverDrive141)
* All Rights Reserved. 
*/ 