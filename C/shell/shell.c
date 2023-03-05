#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <wait.h>

#define ever ;;
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"
#define MAX_COMMAND_CAPACITY 10
#define BUFSIZE 128

/* Double linked list for history of commands */
typedef struct stack stack;

struct stack {
    char* command;
    stack* prev;
    stack* next;
};

void change_dir(char **);
void run_command(char **);
void parse_string(char *, char **, int *);
void add_command(char *, stack**, stack**);
void free_cmd_stack(stack *);
void show_cmd_stack(stack *);
char* get_cmd_stack(stack *);

int COMMAND_CAPACITY = 0;

int
main() {

    int i, arglen, wstat, pid;
    char *args[32];
    char buf[BUFSIZE];
    char *cwd, *user, *tok, *last_dir;
    char *commandargs[32];
    stack *cmd_stack = NULL;
    stack *cmd_stack_tail = NULL;

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
        printf(ANSI_COLOR_CYAN "[%s@%s]~> " ANSI_COLOR_RESET, user, last_dir);
        free(cwd);

        fgets(buf, BUFSIZE, stdin);
        if(buf[strlen(buf)-1] == '\n') {
            buf[strlen(buf)-1] = '\0';
        }
PARSE:
        add_command(buf, &cmd_stack, &cmd_stack_tail);

        // Arguments
        parse_string(buf, args, &arglen);

        // Prev
        if(strcmp(args[0], "prev") == 0) {
            show_cmd_stack(cmd_stack);
            char* temp;
            temp = get_cmd_stack(cmd_stack);
            if(temp == NULL) {
                fprintf(stderr, "Invalid command number\n");
                continue;
            }
            else {
                strncpy(buf, temp, strlen(temp));
                buf[strlen(temp)] = '\0'; // Make sure buf does not read above temp's length
                goto PARSE;
            }
        }
        // exit command
        else if(strcmp(args[0], "exit") == 0) {
            free_cmd_stack(cmd_stack);
            exit(0);
        }
        // cd command
        else if(strcmp(args[0], "cd") == 0) {
            if(arglen > 2) {
                fprintf(stderr, "%s: Too many arguments\n", args[0]);
                continue;
            }
            else {
                change_dir(args);
            }
        }
        else {
            pid = fork();
            if(pid == 0) {
                run_command(args);
            }
            else {
                wait(&wstat);
            }
        }
    }
}

char*
get_cmd_stack(stack* head) {
    
    char buf[BUFSIZE];
    int target, counter = 1;
    stack *trav = head;

    printf("Enter command number: ");
    fgets(buf, BUFSIZE, stdin);
    if(buf[strlen(buf)-1] == '\n') {
        buf[strlen(buf)-1] = '\0';
    }

    target = atoi(buf);
    if(target > 10 || target < 1) {
        return NULL;
    }

    while(trav) {
        if(counter == target) {
            return trav->command;
        }
        counter++;
        trav = trav->next;
    }
    return NULL;
}

void
show_cmd_stack(stack* head) {
    int counter = 1;
    stack* trav = head;

    printf("-------------------------\nPrevious commands are:\n");
    while(trav) {
        printf("\t%d : %s\n", counter++, trav->command);
        trav = trav->next;
    }
    printf("-------------------------\n");

}

void
free_cmd_stack(stack* head) {
    stack *prev = NULL;
    while (head) {
        if (prev) {
            free(prev);
        }
        prev = head;
        head = head->next;
    }
    free(prev);
}

void
add_command(char *to_add, stack** head, stack** tail) {

    if(strcmp(to_add, "prev") == 0) { // Skip "prev" command
        return;
    };
    stack* prev = NULL;
    stack* temp_stack = (stack *) malloc(sizeof(stack));
    char* temp_cmd = (char *) malloc(strlen(to_add));
    strcpy(temp_cmd, to_add);
    temp_stack->command = temp_cmd;
    if (*head == NULL) {
        *head = *tail = temp_stack;
        (*head)->next = NULL;
        (*head)->prev = NULL;
        COMMAND_CAPACITY++;
        return;
    }
    else {
        temp_stack->next = *head;
        (*head)->prev = temp_stack;
        (*head) = (*head)->prev;
        (*head) = temp_stack;
        COMMAND_CAPACITY++;
    }
    if (COMMAND_CAPACITY > MAX_COMMAND_CAPACITY) {
        prev = (*tail)->prev;
        prev->next = NULL;
        free((*tail)->command);
        free(*tail);
        *tail = prev;
        COMMAND_CAPACITY--;
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
run_command(char **args) {

    int n;

    n = execvp(args[0], args);
    if(n == -1) {
        perror(args[0]);
        exit(2);
    }
}

void
parse_string(char *to_parse, char **parsed, int *parsed_len) {

    int i;

    *parsed_len = 0;
    parsed[0] = strtok(to_parse, " \t\n");
    (*parsed_len)++;
    for(i = 1; i < 32; i++) {
        parsed[i] = strtok(NULL, " \t\n");
        if(parsed[i] == NULL) {
            break;
        }
        (*parsed_len)++;
    }
}
