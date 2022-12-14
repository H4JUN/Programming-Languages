#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <wait.h>

#define ever ;;

void change_dir(char **);
void run_command(char *);
void run_command_helper(char *);
void parse_string(char *, char **, int *, char*);
void trim(char *, char *);
void close_pipes(int [][2], int);

int main() {
    
    int BUFSIZE = 128;
    int arglen, wstat, pid;
    char *args[32];
    char buf[BUFSIZE], copied_buf[BUFSIZE];
    char *cwd, *user, *tok, *last_dir;

    for(ever) {
        //Get user
        user = getlogin();
        // Get current working directory
        cwd = getcwd(NULL, 0);
        tok = strtok(cwd, "/");
        // No point in printing the absolute path, get the last dir only
        while((tok = strtok(NULL, "/")) != NULL) {
            last_dir = tok;
        }
        // Console:
        printf("[%s@%s]~> ", user, last_dir);
        free(cwd);

        fgets(buf, BUFSIZE, stdin);
        if(buf[strlen(buf)-1] == '\n') {
            buf[strlen(buf)-1] = '\0';
        }
        // Arguments
        // We need to pass a copy of the buf, so that we could keep the original one unmodified
        strcpy(copied_buf, buf);
        parse_string(copied_buf, args, &arglen, " \t\n");
        // exit command
        if(strcmp(args[0], "exit") == 0) {
            exit(0);
        }
        // cd command
        else if(strcmp(args[0], "cd") == 0) {
            if(arglen > 2) {
                fprintf(stderr, "%s: Too many arguments\n", args[0]);
            }
            else {
                change_dir(args);
            }
        }
        else {
            pid = fork();
            if(pid == 0) {
                run_command(buf);
            }
            else {
                wait(&wstat);
            }
        }
    }
}

void 
change_dir(char **args) {
    
    char *user, *home; 
    int n;
    
    user = getlogin();
    
    if(args[1] == NULL) { // Switch to home directory if used without given path
        // We won't get it from the environment. We'll just assume that it's /home/$USER
        home = malloc(7+strlen(user)); // '/home/' = 6, '\0' = 1 => 7
        sprintf(home, "/home/%s", user);
        home[7+strlen(user)-1] = '\0';
        chdir(home);
        free(home);
    }
    else { // change directory to given input
        n = chdir(args[1]);
        if(n == -1) {
            if(errno == ENOENT) {
                fprintf(stderr, "%s: directory not found\n", args[0]);
            }
            else if(errno == ENOTDIR) {
                fprintf(stderr, "%s: not a directory\n", args[0]);
            }
            else {
                fprintf(stderr, "%s: bad input\n", args[0]);
            }
        }
    }
}

void
run_command(char *cmd) {

    char *pipe_ptr;
    char *subcmd[32]; // To store the parsed commands by pipes
    char trimmed[32]; // To store the trimmed command
    int i, pid, subcmd_len, numPipes, pipe_flag;

    pipe_flag = 0;

    //First, check the presence of pipe:
    pipe_ptr = strchr(cmd, '|');
    if(pipe_ptr != NULL) { // '|' exists:
        pipe_flag = 1;
        parse_string(cmd, subcmd, &subcmd_len, "|");
    }
    if(pipe_flag == 1) {
        numPipes = subcmd_len - 1; // Number of pipes is number of parsed commands - 1
        int pfd[numPipes][2]; // Declare the number of pipes we need
        for(i = 0; i < numPipes; i++) {
            pipe(pfd[i]);   // Initialize the pipes
        }
        // Handle each command in subcmd
        for(i = 0; subcmd[i] != NULL; i++) { // First command
            pid = fork();
            if(pid == 0) {
                if(i == 0) {
                    close(1);
                    dup(pfd[i][1]);
                    close_pipes(pfd, numPipes);
                    trim(subcmd[i], trimmed);
                    run_command_helper(trimmed);
                }
                else if(subcmd[i+1] == NULL) { // Last command
                    close(0);
                    dup(pfd[i-1][0]);
                    close_pipes(pfd, numPipes);
                    trim(subcmd[i], trimmed);
                    run_command_helper(trimmed);
                }
                else { // Middle Command
                    close(0);
                    dup(pfd[i-1][0]);
                    close(1);
                    dup(pfd[i][1]);
                    close_pipes(pfd, numPipes);
                    trim(subcmd[i], trimmed);
                    run_command_helper(trimmed);
                }
            }
        }
        close_pipes(pfd, numPipes);
        // Wait for children
        for(i = 0; i < subcmd_len; i++) {
            wait(NULL);
        }
        exit(0);
    }
    else { // No pipe was found
        run_command_helper(cmd);
    }
}


void
run_command_helper(char *cmd) {
   
    int output_arglen, input_arglen, cmd_arglen;
    int append_redir_flag, overwrite_redir_flag, input_redir_flag;
    char *double_redir, *one_redir, *inp_redir;
    char output_redir_file[32], input_redir_file[32];
    char *output_args[32], *input_args[32], *cmdargs[32];
    int fd, n;

    // Initialize flags:
    append_redir_flag = 0;
    overwrite_redir_flag = 0;
    input_redir_flag = 0;
    // First, take care of output redirections
    //Check if output redir exists:

    double_redir = strstr(cmd, ">>"); // ">>" redirection
    one_redir = strchr(cmd, '>');   //  '>' redirection
    if(double_redir != NULL) { // Case when ">>" exists
        append_redir_flag = 1;
        parse_string(cmd, output_args, &output_arglen, ">>");
        // The last element is the file to append to. Remove any whitespace.
        trim(output_args[output_arglen-1], output_redir_file);
    }
    else if(one_redir != NULL) {
        overwrite_redir_flag = 1;
        parse_string(cmd, output_args, &output_arglen, ">");
        // The last element is the file to overwrite to. Remove any whitespace.
        trim(output_args[output_arglen-1], output_redir_file);
    }
    else { // if both are NULL, then no output redirection
        output_args[0] = cmd;
    }
    inp_redir = strchr(output_args[0], '<'); // Look for '<' in the command
    if(inp_redir != NULL) {
        input_redir_flag = 1;
        parse_string(output_args[0], input_args, &input_arglen, "<");
        // The last element is the file to redirect input from. Remove any whitespace.
        trim(input_args[input_arglen-1], input_redir_file);
    }
    else {
        input_args[0] = output_args[0];
    }
    // Now parse the actual command without the '<', '>', ">>":
    parse_string(input_args[0], cmdargs, &cmd_arglen, " \t\n");
    
    if(append_redir_flag) {
        fd = open(output_redir_file, O_WRONLY|O_APPEND);
        if(fd == -1) {
            perror("open");
            exit(1);
        }
        close(1);
        dup(fd);
        close(fd);
    }
    else if(overwrite_redir_flag) {
        fd = open(output_redir_file, O_CREAT|O_WRONLY|O_TRUNC, 0644);
        if(fd == -1) {
            perror("open");
            exit(1);
        }
        close(1);
        dup(fd);
        close(fd);
    }
    if(input_redir_flag) {
        fd = open(input_redir_file, O_RDONLY);
        if(fd == -1) {
            perror("open");
            exit(1);
        }
        close(0);
        dup(fd);
        close(fd);
    }
    n = execvp(cmdargs[0], cmdargs);
    if(n == -1) {
        printf("Error is exec\n");
        perror(cmdargs[0]);
        exit(2);
    } 
}

void
parse_string(char *to_parse, char **parsed, int *parsed_len, char *delim) {
    
    int i;
    
    *parsed_len = 0;
    parsed[0] = strtok(to_parse, delim);
    (*parsed_len)++;
    for(i = 1; i < 32; i++) {
        parsed[i] = strtok(NULL, delim);
        if(parsed[i] == NULL) {
            break;
        }
        (*parsed_len)++;
    }
}

void
trim(char *to_trim, char *post_trim) {
    
    while(*to_trim != '\0') {
        if(*to_trim != ' ') {
            *post_trim = *to_trim;
            post_trim++;
            to_trim++;
        }
        else {
            to_trim++;
        }
    }
    *post_trim = '\0';
}

void
close_pipes(int pfd[][2], int numPipes) {
    
    int j;
    for(j = 0; j < numPipes; j++) {
        close(pfd[j][0]);
        close(pfd[j][1]);
    }
}
