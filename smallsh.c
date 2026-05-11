// smallsh.c -- simple command processor with basic job control

#include "smallsh.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

struct Node {
    int jobID;
    char* job;
    char where;       // F = foreground, B = background
    int processID;    // process group ID
    char state;       // R = running, S = stopped
    struct Node* next;
};

struct Node* head = NULL;
int ID = 1;
pid_t foregroundPGID = 0;

char inpbuf[MAXBUF], tokbuf[2 * MAXBUF];
char *ptr = inpbuf, *tok = tokbuf;

int searchNode(struct Node** head_ref, int key) {
    struct Node* current = *head_ref;

    while (current != NULL) {
        if (current->jobID == key)
            return current->processID;
        current = current->next;
    }

    return 0;
}

int searchForF(struct Node** head_ref) {
    struct Node* current = *head_ref;

    while (current != NULL) {
        if (current->where == 'F')
            return current->processID;
        current = current->next;
    }

    return 0;
}

int updatectrlZ(struct Node** head_ref, int pgid) {
    struct Node* current = *head_ref;

    while (current != NULL) {
        if (current->processID == pgid) {
            current->state = 'S';
            current->where = 'B';
            return 0;
        }
        current = current->next;
    }

    return 0;
}

int updatebg(struct Node** head_ref, int key) {
    struct Node* current = *head_ref;

    while (current != NULL) {
        if (current->jobID == key) {
            current->state = 'R';
            current->where = 'B';
            return 0;
        }
        current = current->next;
    }

    return 0;
}

int updatefg(struct Node** head_ref, int key) {
    struct Node* current = *head_ref;

    while (current != NULL) {
        if (current->jobID == key) {
            current->state = 'R';
            current->where = 'F';
            return 0;
        }
        current = current->next;
    }

    return 0;
}

void insertAtEnd(struct Node** head_ref, int jobID, char* job, char where, int processID, char state) {
    struct Node* new_node = (struct Node*)malloc(sizeof(struct Node));
    struct Node* last = *head_ref;

    new_node->jobID = jobID;
    new_node->job = strdup(job);
    new_node->where = where;
    new_node->processID = processID;
    new_node->state = state;
    new_node->next = NULL;

    if (*head_ref == NULL) {
        *head_ref = new_node;
        return;
    }

    while (last->next != NULL)
        last = last->next;

    last->next = new_node;
}

void deleteNode(struct Node** head_ref, int jobID) {
    struct Node *temp = *head_ref;
    struct Node *prev = NULL;

    if (temp != NULL && temp->jobID == jobID) {
        *head_ref = temp->next;
        free(temp->job);
        free(temp);
        return;
    }

    while (temp != NULL && temp->jobID != jobID) {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL)
        return;

    prev->next = temp->next;
    free(temp->job);
    free(temp);
}

void deleteNodeID(struct Node** head_ref, int pgid) {
    struct Node *temp = *head_ref;
    struct Node *prev = NULL;

    if (temp != NULL && temp->processID == pgid) {
        *head_ref = temp->next;
        free(temp->job);
        free(temp);
        return;
    }

    while (temp != NULL && temp->processID != pgid) {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL)
        return;

    prev->next = temp->next;
    free(temp->job);
    free(temp);
}

void printList(struct Node* node) {
    printf("| Job ID | Command | Where | PGID | State |\n");

    while (node != NULL) {
        printf("| %d | %s | %c | %d | %c |\n",
               node->jobID,
               node->job,
               node->where,
               node->processID,
               node->state);

        node = node->next;
    }
}

void hndlInt(int sig) {
    if (foregroundPGID > 0) {
        kill(-foregroundPGID, SIGINT);
    }

    write(STDOUT_FILENO, "\n", 1);
}

void hndlStp(int sig) {
    if (foregroundPGID > 0) {
        kill(-foregroundPGID, SIGTSTP);
    }

    write(STDOUT_FILENO, "\n", 1);
}

void checkBackgroundJobs() {
    int status;
    pid_t done;

    while ((done = waitpid(-1, &status, WNOHANG)) > 0) {
        deleteNodeID(&head, done);
    }
}

int userin(char *p) {
    int c, count;

    ptr = inpbuf;
    tok = tokbuf;

    printf("%s", p);
    fflush(stdout);

    count = 0;

    while (1) {
        c = getchar();

        if (c == EOF)
            return EOF;

        if (count < MAXBUF - 1)
            inpbuf[count++] = c;

        if (c == '\n') {
            inpbuf[count] = '\0';
            return count;
        }
    }
}

int inarg(char c) {
    static char special[] = " \t&;\n\0";

    for (int i = 0; special[i]; i++) {
        if (c == special[i])
            return 0;
    }

    return 1;
}

int gettok(char **outptr) {
    int type;

    *outptr = tok;

    while (*ptr == ' ' || *ptr == '\t')
        ptr++;

    *tok++ = *ptr;

    switch (*ptr++) {
        case '\n':
            type = EOL;
            break;

        case '&':
            type = AMPERSAND;
            break;

        case ';':
            type = SEMICOLON;
            break;

        default:
            type = ARG;

            while (inarg(*ptr))
                *tok++ = *ptr++;
    }

    *tok++ = '\0';
    return type;
}

void waitForForeground(pid_t pgid) {
    int status;
    pid_t result;

    while (1) {
        result = waitpid(-pgid, &status, WUNTRACED);

        if (result == -1) {
            if (errno == EINTR)
                continue;
            break;
        }

        if (WIFSTOPPED(status)) {
            updatectrlZ(&head, pgid);
            break;
        }

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            deleteNodeID(&head, pgid);
            break;
        }
    }

    foregroundPGID = 0;
}

int runcommand(char **cline, int where) {
    pid_t pid;
    char* commands = cline[0];

    if (strcmp(cline[0], "jobs") == 0) {
        printList(head);
        return 0;
    }

    else if (strcmp(cline[0], "bg") == 0) {
        if (cline[1] == NULL) {
            printf("bg requires a job ID\n");
            return 0;
        }

        int jobID = atoi(cline[1]);
        int pgid = searchNode(&head, jobID);

        if (pgid > 0) {
            kill(-pgid, SIGCONT);
            updatebg(&head, jobID);
        } else {
            printf("No such job\n");
        }

        return 0;
    }

    else if (strcmp(cline[0], "fg") == 0) {
        if (cline[1] == NULL) {
            printf("fg requires a job ID\n");
            return 0;
        }

        int jobID = atoi(cline[1]);
        int pgid = searchNode(&head, jobID);

        if (pgid > 0) {
            updatefg(&head, jobID);
            foregroundPGID = pgid;
            kill(-pgid, SIGCONT);
            waitForForeground(pgid);
        } else {
            printf("No such job\n");
        }

        return 0;
    }

    else if (strcmp(cline[0], "kill") == 0) {
        if (cline[1] == NULL) {
            printf("kill requires a job ID\n");
            return 0;
        }

        int jobID = atoi(cline[1]);
        int pgid = searchNode(&head, jobID);

        if (pgid > 0) {
            kill(-pgid, SIGKILL);
            deleteNode(&head, jobID);
        } else {
            printf("No such job\n");
        }

        return 0;
    }

    else if (strcmp(cline[0], "exit") == 0) {
        exit(0);
    }

    switch (pid = fork()) {
        case -1:
            perror("smallsh");
            return -1;

        case 0:
            setpgid(0, 0);

            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);

            execvp(*cline, cline);

            perror(*cline);
            exit(1);

        default:
            setpgid(pid, pid);

            if (where == BACKGROUND) {
                printf("[Job %d] [Process Group ID %d]\n", ID, pid);
                insertAtEnd(&head, ID, commands, 'B', pid, 'R');
                ID++;
            } else {
                insertAtEnd(&head, ID, commands, 'F', pid, 'R');
                foregroundPGID = pid;
                ID++;
                waitForForeground(pid);
            }
    }

    return 0;
}

int procline(void) {
    char *arg[MAXARG + 1];
    int toktype, type;
    int nargs = 0;

    for (;;) {
        toktype = gettok(&arg[nargs]);

        switch (toktype) {
            case ARG:
                if (nargs < MAXARG)
                    nargs++;
                break;

            case EOL:
            case SEMICOLON:
            case AMPERSAND:
                type = (toktype == AMPERSAND) ? BACKGROUND : FOREGROUND;

                if (nargs != 0) {
                    arg[nargs] = NULL;
                    runcommand(arg, type);
                }

                if (toktype == EOL)
                    return 0;

                nargs = 0;
                break;
        }
    }
}

int main() {
    printf("type help to get info\n");

    signal(SIGINT, hndlInt);
    signal(SIGTSTP, hndlStp);

    char *prompt = "Command> ";

    while (userin(prompt) != EOF) {
        procline();
        checkBackgroundJobs();
    }

    return 0;
}
