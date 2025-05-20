#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define CMD_MAX_LENGTH 1024
#define ARG_MAX_COUNT 64
#define MAX_BG_JOBS 4

typedef struct {
    pid_t pid;
    char command[CMD_MAX_LENGTH];
} BackgroundJob;

BackgroundJob bgJobs[MAX_BG_JOBS];
int activeJobs = 0;

void executeCommand(char *args[], int isBackground) {
    pid_t pid = fork();

    if (pid == -1) {
        printf("hw1shell: fork failed, errno is %d\n", errno);
    } else if (pid == 0) {
        // Child process
        if (isBackground && activeJobs == MAX_BG_JOBS)
            exit(EXIT_FAILURE);

        if (execvp(args[0], args) == -1) {
            printf("hw1shell: invalid command\n");
            printf("hw1shell: execvp failed, errno is %d\n", errno);
            exit(EXIT_FAILURE);
        }
    } else {
        // Parent process
        if (isBackground) {
            if (activeJobs < MAX_BG_JOBS) {
                printf("hw1shell: %d started\n", pid);
                bgJobs[activeJobs].pid = pid;
                strncpy(bgJobs[activeJobs].command, args[0], CMD_MAX_LENGTH - 1);
                bgJobs[activeJobs].command[CMD_MAX_LENGTH - 1] = '\0';
                activeJobs++;
            } else {
                printf("hw1shell: too many background commands running\n");
            }
        } else {
            if (waitpid(pid, NULL, 0) == -1) {
                printf("hw1shell: waitpid failed, errno is %d\n", errno);
            }
        }
    }
}

void printJobs() {
    printf("PID\tCommand\n");
    for (int i = 0; i < activeJobs; i++) {
        printf("%d\t%s\n", bgJobs[i].pid, bgJobs[i].command);
    }
}

void reapFinishedJobs() {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("hw1shell: %d finished\n", pid);

        for (int i = 0; i < activeJobs; i++) {
            if (bgJobs[i].pid == pid) {
                for (int j = i; j < activeJobs - 1; j++) {
                    bgJobs[j] = bgJobs[j + 1];
                }
                activeJobs--;
                break;
            }
        }
    }
}

void cleanupOnExit() {
    int status;
    while (waitpid(-1, &status, 0) > 0) {
        // Wait for all child processes to finish
    }
}

int main() {
    while (1) {
        reapFinishedJobs();
        char input[CMD_MAX_LENGTH];
        char *args[ARG_MAX_COUNT];

        // Display the shell prompt
        printf("hw1shell$ ");
        fflush(stdout);

        // Read user input
        if (fgets(input, sizeof(input), stdin) == NULL) {
            perror("fgets");
            exit(EXIT_FAILURE);
        }

        // Trim newline character
        input[strcspn(input, "\n")] = '\0';

        if (strlen(input) == 0) {
            continue;  // Skip empty commands
        }

        // Tokenize input into arguments
        char *token = strtok(input, " ");
        int argCount = 0;

        while (token != NULL && argCount < ARG_MAX_COUNT - 1) {
            args[argCount++] = token;
            token = strtok(NULL, " ");
        }
        args[argCount] = NULL;  // Null-terminate the arguments array

        if (strcmp(args[0], "&") == 0) {
            printf("hw1shell: invalid command\n");
            continue;
        }

        // Check for background command
        int isBackground = 0;
        if (argCount > 0 && strcmp(args[argCount - 1], "&") == 0) {
            isBackground = 1;
            args[argCount - 1] = NULL;  // Remove "&" from arguments
        }

        reapFinishedJobs();

        // Internal commands
        if (strcmp(args[0], "exit") == 0) {
            cleanupOnExit();
            printf("Exiting hw1shell...\n");
            exit(EXIT_SUCCESS);
        } else if (strcmp(args[0], "cd") == 0) {
            if (chdir(args[1]) != 0) {
                printf("hw1shell: invalid command\n");
                printf("hw1shell: chdir failed, errno is %d\n", errno);
            }
        } else if (strcmp(args[0], "jobs") == 0) {
            printJobs();
        } else {
            executeCommand(args, isBackground);
        }

        reapFinishedJobs();
    }

    return 0;
}

