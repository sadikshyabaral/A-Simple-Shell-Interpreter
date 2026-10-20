#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

/* Linked list used to track background jobs */
typedef struct BackgroundJob {
    pid_t pid;
    char command[1024];
    struct BackgroundJob *next;
} BackgroundJob;

/* PID of the current foreground process */
volatile pid_t fg_pid = -1;

/* Handle Ctrl-C:
 *  Interrupt foreground process if one exists
 *  Otherwise clear the current input line
 */
void sigint_handler(int sig) {
    (void)sig;

    if (fg_pid > 0) {
        kill(fg_pid, SIGINT);
        return;
    }

    write(STDOUT_FILENO, "^C\n", 3);

    rl_on_new_line();
    rl_replace_line("", 0);
    rl_redisplay();
}

int main() {
	int bailout = 0;

    BackgroundJob *jobList = NULL;
    signal(SIGINT, sigint_handler);
    rl_catch_signals = 0;

	while (!bailout) {

        /* Check for terminated background jobs and remove them from the list */
        BackgroundJob *curr = jobList;
        BackgroundJob *prev = NULL;

        while (curr != NULL) {

            pid_t result = waitpid(curr->pid, NULL, WNOHANG);

            if (result > 0) {

                printf("%d: %s has terminated.\n",
                    curr->pid,
                    curr->command);

                if (prev == NULL) {
                    jobList = curr->next;
                } else {
                    prev->next = curr->next;
                }

                BackgroundJob *temp = curr;
                curr = curr->next;

                free(temp);

            } else {

                prev = curr;
                curr = curr->next;
            }
        }

        fflush(stdout);

        // get username
        char* username = getlogin();
        if (username == NULL) {
            username = "unknown";
        }

        // get hostname
        size_t max_len = HOST_NAME_MAX + 1;
        char hostname[max_len];
        if (gethostname(hostname, sizeof(hostname)) != 0) {
            perror("gethostname");
            strcpy(hostname, "unknown");
        }

        // get current working directory
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            perror("getcwd");
            strcpy(cwd, "?");
        }

        /* Build and display shell prompt */
        char prompt[PATH_MAX + HOST_NAME_MAX + 100];
        snprintf(prompt, sizeof(prompt), "%s@%s: %s > ", username, hostname, cwd);

		char* reply = readline(prompt);

        /* Exit shell on Ctrl-D  */
        if (reply == NULL) {
            printf("\n");
            break;
        }

        if (strlen(reply) == 0) {
            free(reply);
            continue;
        }

        /* Store command in history */
        add_history(reply);

        char originalCommand[1024];
        strncpy(originalCommand, reply, sizeof(originalCommand) - 1);
        originalCommand[sizeof(originalCommand) - 1] = '\0';

        char *argv[100];
        int argc = 0;

        char *token = strtok(reply, " ");
        while (token != NULL && argc < 100) {
            argv[argc++] = token;
            token = strtok(NULL, " ");
        }
        argv[argc] = NULL;

        if (argc == 0 || argv[0] == NULL) {
            free(reply);
            continue;
        } 

        /* Built-in cd command */
        if (strcmp(argv[0], "cd") == 0) {
            const char* home = getenv("HOME");

            if (argc == 1) {
                if (home == NULL) {
                    fprintf(stderr, "cd: HOME not set\n");
                } else if (chdir(home) != 0) {
                    perror("cd failed");
                }
            }
            else if (argc >= 2) {
                char path[PATH_MAX];

                // handle ~ 
                if (argv[1][0] == '~') {
                    if (home == NULL) {
                        fprintf(stderr, "cd: HOME not set\n");
                    } else {
                        
                        if (argv[1][1] == '\0') {
                            snprintf(path, sizeof(path), "%s", home);
                        } else if (argv[1][1] == '/') {
                            snprintf(path, sizeof(path), "%s%s", home, argv[1] + 1);
                        } else {
                            fprintf(stderr, "cd: unsupported ~ syntax\n");
                            free(reply);
                            continue;
                        }

                        if (chdir(path) != 0) {
                            perror("cd failed");
                        }
                    }
                }
                else {
                    // normal path (relative or absolute)
                    if (chdir(argv[1]) != 0) {
                        perror("cd failed");
                    }
                }
            }

            free(reply);
            continue;
        }

        /* Built-in bg command: execute process in background */
        if (argc > 0 && strcmp(argv[0], "bg") == 0) {
            
            // case one: invalid 
            if (argc == 1) {
                // print error message and ignore command
                fprintf(stderr, "bg: missing command\n");
                free(reply);
                continue;
            }

            // case two: real command 

            // remove bg from argv 
            for (int i = 1; i < argc; i++) {
                argv[i - 1] = argv[i];
            }
            argv[argc - 1] = NULL;

            // run command in background (fork and execvp)
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork failed");
            } else if (pid == 0) {
                signal(SIGINT, SIG_DFL);
                execvp(argv[0], argv);

                if (errno == ENOENT) {
                    fprintf(stderr, "%s: command not found\n", argv[0]);
                } else {
                    perror(argv[0]);
                }

                exit(1);
            } else {
                
                BackgroundJob *newJob = malloc(sizeof(BackgroundJob));

                if (newJob == NULL) {
                    perror("malloc");
                    free(reply);
                    continue;
                }

                newJob->pid = pid;

                strncpy(newJob->command,
                        originalCommand,
                        sizeof(newJob->command)-1);

                newJob->command[
                    sizeof(newJob->command)-1
                ] = '\0';

                newJob->next = jobList;
                jobList = newJob;

                free(reply);
                continue;
            }

        } // bg if statment 

        /* Built-in bglist command: display active background jobs */
        if (strcmp(argv[0], "bglist") == 0) {
            int count = 0;
            BackgroundJob *curr = jobList;

            while (curr != NULL) {
                printf("%d: %s\n", curr->pid, curr->command);
                curr = curr->next;
                count++;
            }

            printf("Total Background jobs: %d\n", count);

            free(reply);
            continue;
        }

        /* Foreground command */
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
        } else if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            execvp(argv[0], argv);

            if (errno == ENOENT) {
                fprintf(stderr, "%s: command not found\n", argv[0]);
            } else {
                perror(argv[0]);
            }

            exit(1);
        } else {
            fg_pid = pid;
            waitpid(pid, NULL, 0);
            fg_pid = -1;
        }

        free(reply);

	} // while

	// printf("Bye Bye\n");
	return 0;

} // main
