#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#define MAX_INPUT_SIZE 1027
#define MAX_TOKEN_SIZE 66
#define MAX_NUM_TOKENS 66

// Parsing the command into tokens
char **tokenize(char *line)
{
	char **tokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
	char *token = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
	int i, tokenIndex = 0, tokenNo = 0;

	for(i =0; i < strlen(line); i++){
		char readChar = line[i];
		if (readChar == ' ' || readChar == '\n' || readChar == '\t') {
			token[tokenIndex] = '\0';
			if (tokenIndex != 0){
				tokens[tokenNo] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
				strcpy(tokens[tokenNo++], token);
				tokenIndex = 0; 
			}
		} 
		else {
			token[tokenIndex++] = readChar;
		}
	}
	free(token);
	tokens[tokenNo] = NULL ;
	return tokens;
}

// Parsing the input to commands
char **commandSep(char* line) {
	char **tokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
	char *token = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
	int i, tokenIndex = 0, tokenNo = 0,len = strlen(line);
	char readChar, readChar1;
	// seperate the commands till length -1 
	for(i =0; i < len-1; i++){
		readChar = line[i];
		readChar1 = line[i+1];
		if (readChar == ';' && readChar1 == ';' ) {
			i++;
			token[tokenIndex++] = '\n';
			token[tokenIndex] = '\0';
			if (tokenIndex != 0){
				tokens[tokenNo] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
				strcpy(tokens[tokenNo++], token);
				tokenIndex = 0; 
			}
		} 
		else {
			token[tokenIndex++] = readChar;
		}
	}
	// adding the last token to tokens
	if( (len>1 && line[len-2]!= ';') || line[len-1]!=';') {
		token[tokenIndex++] = line[len-1];
		token[tokenIndex++] = '\n';
		token[tokenIndex] = '\0';
		tokens[tokenNo] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
		strcpy(tokens[tokenNo++], token);
		tokenIndex = 0;
	}
	free(token);
	tokens[tokenNo] = NULL;
	return tokens;
}

// Seperates the command based on the delimite(sep) -  used to seperated io and piping commands 
char **tokenizeSep(char* line, char* sep) {
	char **tokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
	char *token = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
	int i, tokenIndex = 0, tokenNo = 0;
	token = strtok(line,sep);
	while(token!=NULL) {
		tokens[tokenNo] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
		strcpy(tokens[tokenNo++],token);
		token = strtok(NULL,sep);
	}
	free(token);
	tokens[tokenNo] = NULL;
	return tokens;
}

//memory free
void Free(char** arr) {
	for (int i=0;arr[i]!=NULL;i++) free(arr[i]);
	free(arr);
}

// executing command using exec
void execNrml(char **tokens) {
	if (execvp(tokens[0],tokens)<0) {
		fprintf(stderr,"\n Invalid command\n");
		Free(tokens);
		exit(0);
	}
	return;
}

// exit command
void isExit(char **tokens) {
	if(strcmp(tokens[0],"exit") == 0) {
		Free(tokens);
		exit(0);
	}
}

//change directory function
int isCd(char **tokens) {
	if (strcmp(tokens[0],"cd") == 0) {
		int arguments = 0;
		for (int i=1;tokens[i]!=NULL;i++) {
			arguments++;
		}
		if(arguments == 1) {
			int e = chdir(tokens[1]);
			if (e <0) {
				fprintf(stderr,"No such directory\n");
				Free(tokens);
				return 1;
			}
		}
		else
			fprintf(stderr,"Invalid arguments for cd\n");
		Free(tokens);
		return 1;
	}
	return 0;
}

// executing single comand
void execCmd(char* command) {
	char** tokens = tokenize(command);
	isExit(tokens);
	if (isCd(tokens)) return;
	else if (fork() ==0) {
		execNrml(tokens);
	}
	Free(tokens);
	wait(NULL);
}
// executing the piping commands
void execPipe(char **cmd,int pipeCount) {
	char *cmd1 = strcat(cmd[0],"\n");
	char *cmd2 = strcat(cmd[1],"\n");

	char **tokens1 = tokenize(cmd1);	
	char **tokens2 = tokenize(cmd2);
	int p[2];

	pipe(p);

	// child 1
	if(fork() == 0) {
		signal(SIGINT, SIG_IGN);
		close(1); // closes stdout
		dup(p[1]); // this creates the same file descriptor but with stdout number
		close(p[0]); // closes p[0], p[1] so that we can write to the dup 
		close(p[1]);
		isExit(tokens1);
		if(!isCd(tokens1)) execNrml(tokens1);
	}
	//child 2
	if (fork() == 0) {
		close(0);
		dup(p[0]);
		close(p[0]);
		close(p[1]);
		isExit(tokens2);
		if(!isCd(tokens2)) execNrml(tokens2);
	}
	close(p[0]);
	close(p[1]);
	wait(NULL);
	wait(NULL);

}

//redirecting the output
void outRedirection(char **cmd,int redirectionCount) {
	char *cmd1 = strcat(cmd[0], "\n");
	char *cmd2 = strcat(cmd[1] , "\n");	
	char **tokens1 = tokenize(cmd1);	
	char **tokens2 = tokenize(cmd2);
	int fd = open(tokens2[0], O_CREAT | O_WRONLY | O_TRUNC, 0644);	

	if (fd == -1) {
		printf("File creating failed");
		return;
	}
	else {
		if (fork() == 0) {
			close(1);
			dup(fd); // allocates the stdout file number to the duplicate of fd
			close(fd);
			isExit(tokens1);
			if(!isCd(tokens1)) execNrml(tokens1);
		}
		close(fd);
		wait(NULL);
	}
}

// input redirection
void inRedirection(char **cmd, int redirectionCount) {
	char *cmd1 = strcat(cmd[0],"\n");
	char *cmd2 = strcat(cmd[1],"\n");
	char **tokens1 = tokenize(cmd1);
	char **tokens2 = tokenize(cmd2);
	int fd = open(tokens2[0], O_RDONLY);
	if (fd == -1) {
		printf("Reading %s failed\n",tokens2[0]);
		return;
	}
	else {
		if (fork()==0) {
			close(0);
			dup(fd);
			close(fd);
			isExit(tokens1);
			if(!isCd(tokens1)) execNrml(tokens1);
		}
		close(fd);
		wait(NULL);
	}
}


void clear(){
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
	system("clear");
#endif

#if defined(_WIN32) || defined(_WIN64)
	system("cls");
#endif
}
void shell_init() {
	clear();
	printf("\033[1;31m");
	printf("             *** Hi, welcome to C-shell***           \n");
	printf("            type 'exit' to exit the shell            \n");
	sleep(1);
	  printf("\033[0m");
}

// pipe checking and execution
int isPipe(char *command) {

	int pipeCount = 0;
	char** tokens = tokenizeSep(command,"|");
	for (int j=0;tokens[j]!=NULL;j++) {
		pipeCount++;
	}
	if(pipeCount > 1) // there is piping that needs to be done
	{
		execPipe(tokens,pipeCount);
		Free(tokens);
		return 1;
	}
	Free(tokens);
	return 0;
}
//redirection checking and execution
int isIO(char *command) {

	int redirection = 0;
	char** tokens = tokenizeSep(command,"<");
	for (int j=0;tokens[j]!=NULL;j++) {
		redirection++;
	}
	if (redirection >1) {
		inRedirection(tokens,redirection);
		Free(tokens);
		return 1;
	}
	Free(tokens);
	redirection = 0;
	tokens = tokenizeSep(command,">");
	for (int j=0;tokens[j]!=NULL;j++) {
		redirection++;
	}
	if (redirection > 1)
	{
		outRedirection(tokens,redirection);
		Free(tokens);
		return 1;
	}
	Free(tokens);
	return 0;
}

void main(void)
{
	char  cwd[5000], line[MAX_INPUT_SIZE];            
	char  **commands, **tokens;              
	int i;
	shell_init();

	// signal handling of Interrupt
	signal(SIGINT, SIG_IGN);

	bzero(line, MAX_INPUT_SIZE);
	while (1) {
		if (getcwd(cwd,sizeof(cwd))==NULL) {
			printf("Error in getting present working directory\n");
			exit(0);
		}		
		printf("\033[0;32m");
		printf("@sd: ");
		printf("\033[0;34m");
		printf("%s",cwd);
		printf("\033[0m >");
				
		gets(line);           
		if(strlen(line)==0) {
			continue;
		}
		commands = commandSep(line);	

		for (i=0;commands[i]!=NULL;i++) {
			if (isPipe(commands[i])) 
				continue;

			if(isIO(commands[i]))
				continue;
			execCmd(commands[i]);

		}
		Free(commands);
	}
}


