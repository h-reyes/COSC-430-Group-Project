// smallsh.c -- simple command processor

#include "smallsh.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

//LINKED LIST BEGINNING
struct Node {
	int jobID;
	char* job;
	char where;
	int processID;
	char state;
	struct Node* next;
};

// Search a node
int searchNode(struct Node** head_ref, int key) {
	struct Node* current = *head_ref;

	while (current != NULL) {
		if (current->jobID == key) return current->processID;
		current = current->next;
	}
	return 0;
}
//finds node with F
int searchForF(struct Node** head_ref) {
	struct Node* current = *head_ref;

	while (current != NULL) {
		if (current->where == 'F') return current->processID;
		current = current->next;
	}
	
	return 0;
}

int updatectrlZ(struct Node** head_ref) {
	struct Node* current = *head_ref;

	while (current != NULL) {
		if (current->where == 'F') {
			current->state = 'S';
			current->where = 'B';
		} else {
			current = current->next;
		}
	}
	return 0;
}



// changes for bg
int updatebg(struct Node** head_ref, int key) {
	struct Node* current = *head_ref;

	while (current != NULL) {
		if (current->jobID == key) current->state = 'R';
		current = current->next;
	}
	return 0;
}

//changes for fg
int updatefg(struct Node** head_ref, int key) {
	struct Node* current = *head_ref;

	while (current != NULL) {
		if (current->jobID == key) {
			current->state = 'R';
			current->where = 'F';
			return 0;
		} else {
			current = current->next;
			
		}
	}
	return 0;
}



//insert at the end
void insertAtEnd(struct Node** head_ref, int jobID,char* job, char where, int processID,char state) {
	struct Node* new_node = (struct Node*)malloc(sizeof(struct Node));
	struct Node* last = *head_ref; /* used in step 5*/

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

	while (last->next != NULL) last = last->next;

	last->next = new_node;
	return;
}

// Delete a node
void deleteNode(struct Node** head_ref, int jobID) {
	struct Node *temp = *head_ref, *prev;

	if (temp != NULL && temp->jobID == jobID) {
		*head_ref = temp->next;
		free(temp);
		return;
	}
	// Find the key to be deleted
	while (temp != NULL && temp->jobID != jobID) {
		prev = temp;
		temp = temp->next;
	}

	// If the key is not present
	if (temp == NULL) return;

	// Remove the node
	prev->next = temp->next;
	free(temp);
}

void deleteNodeID(struct Node** head_ref, int ID) {
	struct Node *temp = *head_ref, *prev;

	if (temp != NULL && temp->processID == ID) {
		*head_ref = temp->next;
		free(temp);
		return;
	}
	// Find the key to be deleted
	while (temp != NULL && temp->processID != ID) {
		prev = temp;
		temp = temp->next;
	}

	// If the key is not present
	if (temp == NULL) return;

	// Remove the node
	prev->next = temp->next;
	free(temp);
}

void printList(struct Node* node) {
	while (node != NULL) {
		printf("| %d | %s | %c | %d | %c |\n", node->jobID, node->job, node->where, node->processID, node->state);
		node = node->next;
	}
}

//LINKED LIST END

//GLOBAL VARIABLES ON FONEM
struct Node* head = NULL;
int ID = 0;
char* commands;
pid_t pid;



//start of smallsh

char inpbuf[MAXBUF], tokbuf[2 * MAXBUF];
char *ptr = inpbuf, *tok = tokbuf;

void hndlInt(int sig) {
//you can use kill(childPID, SIGKILL);
//also when doing this you do not have to call deleteNode because I did the delete in the hndlCHLD
	int temp = searchForF(&head);
	if(temp >0){
	
	kill(temp,SIGINT);
	deleteNodeID(&head, pid);
	pid = 0;
	}
	
	
	printf("control c called\n");
}

void hndlStp(int sig) {
//you can use kill(chldpid, SIGSTOP);
//when calling this you have to update the linked list
	int temp = searchForF(&head);
	kill(temp,SIGSTOP);
	updatectrlZ(&head);
	printf("control z called\n");

}

void hndlChld(int sig) {
    int status;
    pid_t child;
while ((child = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
if (WIFSTOPPED(status)) {
            printf("child stopped: PID %d by signal %d\n", child, WSTOPSIG(status));
            // Do NOT delete here — it's only stopped, not gone.
        }
 else{
 printf("child has been killed %d\n", pid);
 deleteNodeID(&head, pid);
}

    }
	
}


// Read user input and place it in inpbuf
int userin(char *p) {
	int c, count;
	ptr = inpbuf;
	tok = tokbuf;

	printf("%s", p);
	fflush(stdout);  // Ensure prompt is shown immediately
	count = 0;

	while (1) {
		c = getchar();
		if (c == EOF) return EOF;

		if (count < MAXBUF - 1)
			inpbuf[count++] = c;

		if (c == '\n') {
			if (count < MAXBUF) {
				inpbuf[count] = '\0';
				return count;
			} else {
				printf("smallsh: input line too long\n");
				count = 0;
				printf("%s", p);
				fflush(stdout);
			}
		}
	}
}

// Check if character is part of a normal argument
int inarg(char c) {
	static char special[] = " \t&;\n\0";
	for (int i = 0; special[i]; i++) {
		if (c == special[i])
			return 0;
	}
	return 1;
}

// Get next token from input
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

// Execute a command with optional background execution
int runcommand(char **cline, int where) {

//jobID; job; where; processID;
	int status;
	commands = cline[0];
	if (strcmp(cline[0], "jobs") == 0) {
		printList(head);
		return 0;
	}
	else if (strcmp(cline[0], "bg") == 0) {
		//I have to implement the update of the linked list
		
		updatebg(&head,atoi(cline[1]));
		return 0;
	}
	else if (strcmp(cline[0], "fg") == 0) {
//I DONT KNOW IF THIS WORKS OR NOT
		updatefg(&head, atoi(cline[1]));
		return 0;
	}
	else if (strcmp(cline[0], "kill") == 0) {
		printf("killing %s\n",cline[1]);
//you can make it kill the child with
// kill(atoi(cline[1]), SIGKILL);
// but idk if he wants us to kill it with the id or job ID
		kill(searchNode(&head, atoi(cline[1])), SIGKILL);
		return 0;
	}
	else if (strcmp(cline[0], "exit") == 0) {
		exit(0);
	}
	else if (strcmp(cline[0], "help") == 0) {
		printf("COMMANDS\n");
		printf("jobs : list all running jobs\n");
		printf("bg <Process ID> : turns the stopped background process into running background process\n");
		printf("fg <Process ID> : turns a stopped or running background job to a running in foreground process\n");
		printf("kill <Process ID> : terminates job\n");
		printf("exit : quit the smallsh\n");
		return 0;
	}
	else {

		switch (pid = fork()) {
		case -1:
			perror("smallsh");
			return -1;

		case 0:

			execvp(*cline, cline);

			perror(*cline);
			exit(1);
		}
		//STARTS COMMAND
		printf("command started\n");

		if (where == BACKGROUND) {
			printf("[Process id %d]\n", pid);
			insertAtEnd(&head,ID,commands,'B',pid,'R');
			ID +=1;

			return 0;
		} else {
			insertAtEnd(&head,ID,commands,'F',pid,'R');
			ID +=1;
		}

	if (waitpid(pid, &status, WUNTRACED) == -1)

			return -1;
		else
			printf("command done\n");

		return status;

		
	}
}

// Process an entire input line
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

// Main loop: get input and process it
int main() {
	printf("type help to get info\n");
	signal(SIGINT, hndlInt);
	signal(SIGTSTP,hndlStp);
	signal(SIGCHLD,hndlChld);
	char *prompt = "Command> ";
	while (userin(prompt) != EOF) {
		procline();
	}
	return 0;
}

