/* smallsh.h -- defs for smallsh command processor */

#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>

#define EOL 1         /* end of line */
#define ARG 2         /* normal arguments */
#define AMPERSAND 3   /* background process indicator */
#define SEMICOLON 4   /* command separator */

#define MAXARG 512    /* max number of command arguments */
#define MAXBUF 512    /* max length of input line */

#define FOREGROUND 0  /* foreground process */
#define BACKGROUND 1  /* background process */