/* sendtx.c -- broadcast TXs in upload_TXs
  Author: Danny Wu
  2019-02-14 : v0.1
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

//#define TESTNET

#define SENDTX

int cmd_run(const char* cmd, char* argv[], char* outstr)
{
    char buffer[65536] = { 0 };
    int len;
    int pfd[2];
    int status;
    pid_t pid;
 
    /* create pipe */
    if (pipe(pfd)<0)
        return -1;
 
    /* fork to execute external program or scripts */
    pid = fork();
    if (pid<0) {
        return 0;
    }
	else if (pid==0) { /* child process */
        dup2(pfd[1], STDOUT_FILENO);
        close(pfd[0]);
 
        /* execute CGI */
        execvp(cmd, argv);
        close(pfd[1]);
        exit(0);
    }
	else { /* parent process */
        close(pfd[1]);
 
        /* print output from CGI */
        while((len=read(pfd[0], buffer, 65536))>0) {
            memcpy(outstr, buffer, len);
            outstr += len;
        }
        outstr[0] = '\0';
 
        /* waiting for CGI */
        waitpid((pid_t)pid, &status, 0);
        close(pfd[0]);
    }
    return 0;
}

int finaltxnum = 0;
int totalsize = 0;

void broadcasttx(char* tx) {
    char send[30] = "sendrawtransaction";
#ifdef TESTNET
    char cmd[30] = "bitcoin-cli";
    char cmd2[30] = "-testnet";
    char *argv2[5]={cmd,cmd2,send,tx,0};
#else
    char cmd[20] = "bitcoin-cli";
    char *argv2[4]={cmd,send,tx,0};
#endif
    char outputs[200];
#ifdef SENDTX
    cmd_run(argv2[0],&argv2[0],outputs);
    printf("\n----SendTransction ID: %s\n",outputs);
#endif
    totalsize += strlen(tx);
    printf("txnum = %d, txsize = %d, total size = %d\n",finaltxnum++,strlen(tx),totalsize);
}

char tx_buf[500000];    //500KB

int main( int argc, char *argv[] ) {

    FILE * fp;
    size_t read_len;
 
    char fname[200] = "upload_TXs";
    fp = fopen (fname, "r");
    while(fgets (tx_buf , 500000 , fp) != NULL) {
        tx_buf[strlen(tx_buf)-1] = '\0';
        broadcasttx(tx_buf);
    }
    fclose(fp);
    
    return 0;
}
