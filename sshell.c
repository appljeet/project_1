#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define CMDLINE_MAX 512

struct currentInput
{
        const char *command;
        char *arguments[16];
        int num_args;
};

enum{
        TOO_MANY_ARGS,
        MISSING_CMD,
        NO_OUTPUT_FILE,
        NO_INPUT_FILE,
        FAIL_OPEN_OUTFILE,
        FAIL_OPEN_INFILE,
        MISLOCATE_OUTPUT_REDIRECTION,
        MISLOCATE_INPUT_REDIRECTION
};

void errMsg(int error){
        fprintf(stderr, "Error: ");
        switch(error){
                case TOO_MANY_ARGS:
                        fprintf(stderr, "too many process arguments\n");
                        break;
                case MISSING_CMD:
                        fprintf(stderr, "missing command\n");
                        break;
                case NO_OUTPUT_FILE:
                        fprintf(stderr, "no output file\n");
                        break;
                case NO_INPUT_FILE:
                        fprintf(stderr, "no input file\n");
                        break;
                case FAIL_OPEN_OUTFILE:
                        fprintf(stderr, "cannot open output file\n");
                        break;
                case FAIL_OPEN_INFILE:
                        fprintf(stderr, "cannot open input file\n");
                        break;
                case MISLOCATE_OUTPUT_REDIRECTION:
                        fprintf(stderr, "mislocated output redirection\n");
                        break;
                case MISLOCATE_INPUT_REDIRECTION:
                        fprintf(stderr, "mislocated input redirection\n");
                        break;
        }
}


void split(struct currentInput *obj, char *current_line)
{
        // splits commands and args
        /* get the first token */
        char *token = strtok(current_line, " ");
        // first token is the command
        obj->command = token;
        /* walk through other tokens */
        int argumentsIndex = 0;
        while (token != NULL)
        {
                // append token to arguments
                obj->arguments[argumentsIndex] = token;

                token = strtok(NULL, " ");
                argumentsIndex++;
        }

        obj->num_args = argumentsIndex;
        // append NULL to arguments
        obj->arguments[argumentsIndex] = NULL;
}

void *get_cur_dir(size_t size){
        char *buffer = (char*) malloc(size);
        if(getcwd(buffer, size) == buffer)
                return buffer;
        free(buffer);
        return 0;
}

void dirs(char **dir_stack, int pos){
        for(int i = pos; i >= 0; i--){
                fprintf(stderr, "%s\n", dir_stack[i]);
        }
}

void pushd(char **dir_stack, int pos, size_t size){
        dir_stack[pos] = get_cur_dir(size);
}

void complete(char *cmd, int retval){
        fprintf(stderr, "+ completed '%s' [%d]\n", cmd, retval);
}

void pipeline2(struct currentInput command1, struct currentInput command2){
        int fd[2];
        pipe(fd);

        if(fork() != 0){/* Parent */
                /* No need for write access */
                close(fd[1]);
                /* Replace stdout with pipe */
                dup2(fd[1], STDOUT_FILENO);
                /* Close now unused FD */
                close(fd[1]);
                /* Parent becomes process1 */
                execvp(command1.command,command1.arguments);
        }else{/* Child */
                /* No need for write access */
                close(fd[1]);
                /* Replace stdin with pipe */
                dup2(fd[0], STDIN_FILENO);
                /* Close now unused FD */
                close(fd[0]);
                /* Child becomes process2 */
                execvp(command2.command,command2.arguments);
        }
}

int main(void)
{
        char cmd[CMDLINE_MAX];
        size_t size = 100;
        int terminal = dup(1);
        char **dir_stack = malloc(sizeof(char*) * size);
        int stack_pos = 0;
        dir_stack[stack_pos] = get_cur_dir(size);

        while (1) {
                char *nl;
                int retval;
                int num_args = 0;
                // make sure stdout and stdin are located at  the terminal
                dup2(terminal, STDOUT_FILENO);
                dup2(terminal, STDIN_FILENO);
                /* Print prompt */
                printf("sshell$ ");
                fflush(stdout);
                /* Get command line */
                fgets(cmd, CMDLINE_MAX, stdin);

                /* Print command line if stdin is not provided by terminal */
                if (!isatty(STDIN_FILENO)) {
                        printf("%s", cmd);
                        fflush(stdout);
                }

                /* Remove trailing newline from command line */
                nl = strchr(cmd, '\n');
                if (nl)
                        *nl = '\0';

                /* spliting command and do the output redirection if '>' exist */
                struct currentInput command_line;
                char *cp = strdup(cmd);
                // if cmd contains redirect symbol
                if((strchr(cmd, '>') != NULL) | (strchr(cmd, '<') != NULL)){
                        num_args = 2;
                        char *command;
                        char *file;
                        int fd;
                        if(strchr(cmd, '>') != NULL){
                                // output redirection
                                command = strtok(cp, ">");
                                file = strtok(NULL, " ");
                                if(file == NULL){
                                        errMsg(NO_OUTPUT_FILE);
                                        continue;
                                }
                                if(strchr(file, '/') != NULL){
                                        errMsg(FAIL_OPEN_OUTFILE);
                                        continue;
                                }

                                fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR);
                                dup2(fd, STDOUT_FILENO);
                        } else {
                                // input redirection
                                command = strtok(cp, "<");
                                file = strtok(NULL, " ");

                                if(file == NULL){
                                        errMsg(NO_INPUT_FILE);
                                        continue;
                                }
                                if(access(file, F_OK) != 0){
                                        errMsg(FAIL_OPEN_INFILE);
                                        continue;
                                }

                                fd = open(file, O_RDONLY);
                                dup2(fd, STDIN_FILENO);
                        }
                        close(fd);
                        cp = strdup(command);
                }
                split(&command_line, cp);
                num_args += command_line.num_args;
                /* Error msg for too many arguments */
                if(num_args > 16){
                        errMsg(TOO_MANY_ARGS);
                }

                /* Extra feature: dirs */
                if(!strcmp(command_line.command, "dirs")){
                        dirs(dir_stack, stack_pos);
                        complete(cmd, 0);
                        continue;
                }

                /* Extra feature: pushd */
                if(!strcmp(command_line.command, "pushd")){
                        if(chdir(command_line.arguments[1])){
                                fprintf(stderr, "Error: no such directory\n");
                                complete(cmd, 1);
                        }else{
                                stack_pos++;
                                pushd(dir_stack, stack_pos, size);
                                complete(cmd, 0);
                        }
                        continue;
                }

                /* Extra features: popd */
                if(!strcmp(command_line.command, "popd")){
                        if(stack_pos == 0){
                                fprintf(stderr, "Error: directory stack empty\n");
                                complete(cmd, 1);
                        } else {
                                chdir("..");
                                stack_pos--;
                                complete(cmd, 0);
                        }
                        continue;
                }

                /* Builtin command */
                if (!strcmp(cmd, "exit")) {
                        fprintf(stderr, "Bye...\n");
                        complete(cmd, 0);
                        break;
                }

                /* Builtin Command: cd */
                if (!strcmp(command_line.command, "cd")){
                        if(chdir(command_line.arguments[1])){
                                fprintf(stderr, "Error: cannot cd into directory\n");
                                complete(cmd, 1);
                                continue;
                        }
                        complete(cmd, 0);
                        continue;
                }

                /* Builtin Command: pwd */
                if(!strcmp(cmd, "pwd")) {
                        char *buffer = get_cur_dir(size);
                        fprintf(stderr, "%s%s", buffer, "\n");
                        complete(cmd, 0);
                        continue;
                }

                /* Piping */
                // if | exists in command
                if (strchr(cmd, '|') != NULL){
                        // make an array of struct currentInput
                        struct currentInput commands[4];

                        const char s[2] = "|";
                        char *token;
                        char *save = NULL;
                        /* get the first token */
                        token = strtok_r(cmd, s, &save);
                        /* walk through other tokens */

                        int processIndex = 0;
                        while (token != NULL){
                                struct currentInput currentProcess;
                                // make token currentProcess
                                split(&currentProcess, token);

                                // add currentProcess to array
                                commands[processIndex] = currentProcess;

                                token = strtok_r(NULL, s, &save);
                                processIndex++;
                        }

                        // now commands array has all processes

                        pipeline2(commands[0], commands[1]);
                        continue;
                }


                /* Regular command */

                retval = 0;

                pid_t pid;
                pid = fork();
                if(pid > 0){
                        // parent
                        wait(&retval);
                } else if(pid == 0){
                        // child
                        // execute cmd
                        execvp(command_line.command, command_line.arguments);
                        // 1 means failed
                        exit(1);
                } else {
                        // fork failed
                        perror("fork");
                        exit(1);
                }
                fprintf(stderr, "+ completed '%s' [%d]\n", cmd, WEXITSTATUS(retval));
        }

        return EXIT_SUCCESS;
}

