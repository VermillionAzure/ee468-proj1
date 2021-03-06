/*
 *  This is a simple shell program from
 *  rik0.altervista.org/snippetss/csimpleshell.html
 *  It's been modified a bit and comments were added.
 *
 *  It doesn't allow misdirection, e.g., <, >, >>, or |
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#define BUFFER_SIZE 80
#define ARR_SIZE 80

// #define DEBUG 1  /* In case you want debug messages */


void parse_delim_pipe(char *buffer, char** args, 
                      size_t args_size, size_t *nargs)
{
    char *buf_args[args_size]; /* You need C99.  Note that args_size
                                  is normally a constant. */
    char **cp;  /* This is used as a pointer into the string array */
    char *wbuf;  /* String variable that has the command line */
    size_t i, j; 
    
    wbuf=buffer;
    buf_args[0]=buffer; 
    args[0] =buffer;

    for (cp = buf_args; (*cp = strsep(&wbuf, "|")) != NULL; ) {
        if ((*cp != '\0') && (++cp >= &buf_args[args_size]))
            break;
    }

    for (j = i = 0; buf_args[i] != NULL; i++) {
        if (strlen(buf_args[i]) > 0) {
            args[j++] = buf_args[i];
        }
    }

    *nargs = j;
    args[j] = NULL;
}

void parse_args(char *buffer, char** args, 
                size_t args_size, size_t *nargs)
{
    char *buf_args[args_size]; /* You need C99.  Note that args_size
                                  is normally a constant. */
    char **cp;  /* This is used as a pointer into the string array */
    char *wbuf;  /* String variable that has the command line */
    size_t i, j; 
    
    wbuf=buffer;
    buf_args[0]=buffer; 
    args[0] =buffer;

    for (cp = buf_args; (*cp = strsep(&wbuf, " \n\t")) != NULL; ) {
        if ((*cp != '\0') && (++cp >= &buf_args[args_size]))
            break;
    }

    for (j = i = 0; buf_args[i] != NULL; i++) {
        if (strlen(buf_args[i]) > 0) {
            args[j++] = buf_args[i];
        }
    }

    *nargs = j;
    args[j] = NULL;
}

/*
 *
 * IDEA:
 * - Create a function just to handle the execution.
 * - Have the parent/child establish the pipes.
 *
 * - Redirect stdin and stdio to the pipes.
 *
 */

struct exec_info {
    char* str_args[ARR_SIZE];
    size_t num_args;
};

/*
 *
 *
 *
 */


int main(int argc, char *argv[], char *envp[]){
    char buffer[BUFFER_SIZE];
    char* args[ARR_SIZE];

    int* ret_status;
    size_t nargs;
    
    while(1){
        printf("ee468>> "); /* Prompt */
        fgets(buffer, BUFFER_SIZE, stdin); /* Read in command line */
              /* Parse the command line into args */

        /* Clear the buffers. */
        memset((void*)&args, '\0', sizeof(char*)*ARR_SIZE);

        parse_delim_pipe(buffer, args, ARR_SIZE, &nargs);
        size_t num_exec = nargs;

        struct exec_info execinfo[num_exec];
       
        {
            size_t it;
            for (it = 0; it < num_exec; ++it) {
                memset((void*)execinfo[it].str_args, '\0', sizeof(char*)*ARR_SIZE);
                execinfo[it].num_args = 0;
            }
        }

        /*
         * For each |-deliminated string, split further.
         * Then, assign the information into its struct exec_info
         */
        size_t i;
        for(i = 0; i < num_exec; ++i) {
            /* get the string */
            char* it_str = args[i]; 
            /* let's map our outputs onto exec_info struct object... */
                /* 
                 * void parse_args(char* buffer,        INPUT
                 *                 char** args,         OUTPUT arr<char* const> 
                 *                 size_t args_size,    INPUT
                 *                 size_t *nargs)       OUTPUT
                 */
            /*
             * We also need to make sure that we have execinfo[i].str_args buffer
             * ready. It's in the definition for struct exec_info now--it has a size of
             * ARGS_SIZE or so.
             */
            parse_args(it_str, execinfo[i].str_args, ARR_SIZE, &execinfo[i].num_args);
#ifdef DEBUG
            printf("exec%2zu: num args: %zu\n", i,  execinfo[i].num_args);
#endif
        }

#ifdef DEBUG
        /*
         * DEBUG: Print out the exec() number, then the arguments involved.
         */
        size_t it;
        for (it = 0; it < num_exec; ++it) {
            printf("(%zu):\n", it);
            
            /*
             * Print out each argument.
             */
            size_t jt;
            for (jt = 0; jt < execinfo[it].num_args; ++jt) {
                printf("\t(%zu): %s\n", jt, execinfo[it].str_args[jt]);
            }
        }
#endif
 
        if (nargs==0) continue; /* Nothing entered so prompt again */
        if (!strcmp(args[0], "exit" )) exit(0);       

        /*
         *
         * BEGIN EXECUTION OF PROCESSES
         *
         */
        /* okay! */
        /* ALGORITHM:
         * 1. Setup in/out pipes.
         * 2. Duplicate pipes onto stdin and out as needed.
         * 3. Setup process A (where A is our current process)
         * 4. Execute process A
         * 5. Wait until A is done, then clean it up.
         * 6. Do such that the next process B (where
         *    B is the next process in line) is setup where
         *    it will be A on the next iteration
         * 7. Iterate
         */

        /* So the way I have it coded is to keep one "global"
         * pipe_in pipe. This is because the out-going pipe will
         * need to become the in-going pipe when we take the
         * output of process A into the pipe which needs to go into B,
         * where B will replace the "position" of A on the next
         * iteration.
         */
        int pipe_in[2];
        pipe(pipe_in);
        /* proc_it is the loop iterator variable. */
        size_t proc_it;
        for (proc_it = 0; proc_it < num_exec; ++proc_it) {
            pid_t pid;

            /* Intialize the out-going pipe. */ 
            int pipe_out[2];
            pipe(pipe_out);
            /* Fork into parent, child. */
            pid = fork();
            if (pid == 0){  /* The child */

                /* Duplicate pipe_in.out onto A.stdin
                 *           A.stdout onto pipe_out.in
                 *           A.stderr onto pipe_out.in
                 */
                close(0);
                close(1);
                close(2);
                if (proc_it != 0)  {
                    dup2(pipe_in[0], 0);
                }
                dup2(pipe_out[1], 1);
                dup2(pipe_out[1], 2);

                /* Close the out side of the 
                 * out-going pipe. Since the child is not
                 * going to read from its own output.
                 */

                /* Run the program using the correct
                 * <proc_it>-th exec() info.
                 */
                /*
                 * 
                 *
                 *
                 *
                 * THIS KEEPS CRASHING EVEN THOUGH IT'S BEING CALLED
                 * WITH THE EXACT SAME PARAMETERS IN MEMORY
                 * WITH THE SAME EXACT FUNCTION
                 * THE ONLY DIFFERENCE IS THAT IT'S HELD IN THE STRUCT.
                 */
                if (execvp(execinfo[proc_it].str_args[0],
                           execinfo[proc_it].str_args)) {
                    puts(strerror(errno));
                    exit(127);
                }
            } else { /* The parent */

                #ifdef DEBUG
                    printf("Waiting for child (%d)\n", pid);
                #endif


                /* Wait until the child process A is done. */
                pid = wait(ret_status);

                #ifdef DEBUG
                    printf("Child (%d) finished\n", pid);
                #endif
                
                /* Print error string on error. */
                if (pid == -1) { 
                    puts(strerror(errno));
                    exit(126);
                }

                /* Setup next process B onto process A's place:
                 * - pipe_out --> pipe_in
                 * - Setup execvp() info to execute with process B
                 *   information already occurs based on proc_it.
                 */

                /* Close in-going pipe. */
                close(pipe_in[0]);
                close(pipe_in[1]);
                /* Then, copy out-going pipe info to
                 * in-going pipe info.
                 */
                dup2(pipe_out[0], pipe_in[0]);
                dup2(pipe_out[1], pipe_in[1]);
                /* Iterate. */
            } 
        }
    }    
    return 0;
}


