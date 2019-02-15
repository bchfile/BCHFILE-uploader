/* bchfile_uploader.c -- uploader for BCH/BSV/BTC FILE
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

//#define BTC
//#define TESTNET
#define FEE 1

#ifdef BTC
//11MB Maximum
    #define TX_DATA_SATOSHI (1000*FEE)
    #define MAXTX           400
    #define MAXBRANCH       400
    #define MAXDATA         70
#else
//21MB Maximum
    #define TX_DATA_SATOSHI (1000*FEE)
    #define MAXTX           400
    #define MAXBRANCH       250
    #define MAXDATA         210
#endif

#ifdef TESTNET
    #define TT 1
#else
    #define TT 0
#endif

//#define SENDTX

char out_str[2000000];      //2MB

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

typedef unsigned char BYTE;
void StrToHex(BYTE *pbDest, BYTE *pbSrc, int nLen)
{
    char h1,h2;
    BYTE s1,s2;
    int i;

    for (i=0; i<nLen; i++)
    {
        h1 = pbSrc[2*i];
        h2 = pbSrc[2*i+1];

        s1 = toupper(h1) - 0x30;
        if (s1 > 9) 
        s1 -= 7;

        s2 = toupper(h2) - 0x30;
        if (s2 > 9) 
        s2 -= 7;

        pbDest[i] = s1*16 + s2;
    }
}

void HexToStr(BYTE *pbDest, BYTE *pbSrc, int nLen)
{
    char    ddl,ddh;
    int i;

    for (i=0; i<nLen; i++)
    {
        ddh = 48 + pbSrc[i] / 16;
        ddl = 48 + pbSrc[i] % 16;
        if (ddh > 57) ddh = ddh + 7;
        if (ddl > 57) ddl = ddl + 7;
        pbDest[i*2] = ddh;
        pbDest[i*2+1] = ddl;
    }

    pbDest[nLen*2] = '\0';
}

int finaltxnum = 0;
int totalsize = 0;
FILE* fp_alltx;
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
    fprintf(fp_alltx,"%s\n",tx);
}

//----------------------------------------------------------------------------------
// generate TX with utxo_num of outputs, each with out_balance except the lastone
//----------------------------------------------------------------------------------
void gen_utxos(const char* address, const char* scriptPubKey, char* privatekey, const char* txid_src, int vout_src, 
               int utxo_num, char* txid_dst, int in_balance, int out_balance, int* last_out_balance) {
    char argv[4+TT][500000];        //500KB
#ifdef BTC
    char sighashtype[20] = "ALL";
#else
    char sighashtype[20] = "ALL|FORKID";
#endif
    char privatekey_str[65] = "[\"";
    strcat(privatekey_str,privatekey);
    strcat(privatekey_str,"\"]");
#ifdef TESTNET
    char *argv2[8]={argv[0],argv[1],argv[2],argv[3],argv[4],privatekey_str,sighashtype,0};
#else
    char *argv2[7]={argv[0],argv[1],argv[2],argv[3],privatekey_str,sighashtype,0};
#endif
    int i;
    
    //create tx
#ifdef TESTNET
    strcpy(argv2[0],"bitcoin-tx");
    strcpy(argv2[1],"-testnet");
#else
    strcpy(argv2[0],"bitcoin-tx");
#endif
    strcpy(argv2[1+TT],"-create");
    argv2[2+TT] = 0;
    cmd_run(argv2[0],&argv2[0],out_str);
    argv2[2+TT] = argv[2+TT];
    
    //add inputs
    out_str[strlen(out_str)-1] = 0;
    strcpy(argv2[1+TT],out_str);
    sprintf(argv2[2+TT],"in=%s:%d",txid_src,vout_src);
    argv2[3+TT] = 0;
    cmd_run(argv2[0],&argv2[0],out_str);
    argv2[3+TT] = argv[3+TT];
    
    //add outputs
    for (i=0; i<utxo_num-1; i++) {
        out_str[strlen(out_str)-1] = 0;
        strcpy(argv2[1+TT],out_str);
        sprintf(argv2[2+TT],"outaddr=%.8lf:%s",out_balance*0.00000001,address);
        argv2[3+TT] = 0;
        cmd_run(argv2[0],&argv2[0],out_str);
    }
    *last_out_balance = in_balance - (out_balance * (utxo_num-1)) - (utxo_num * 36*FEE) - (164*FEE);    //tx fee=164+36*output_num
    out_str[strlen(out_str)-1] = 0;
    strcpy(argv2[1+TT],out_str);
    sprintf(argv2[2+TT],"outaddr=%.8lf:%s",(*last_out_balance)*0.00000001,address);
    argv2[3+TT] = 0;
    cmd_run(argv2[0],&argv2[0],out_str);
    argv2[3+TT] = argv[3+TT];

    //sign the TX
#ifdef TESTNET
    strcpy(argv2[0],"bitcoin-cli");
    strcpy(argv2[1],"-testnet");
#else
    strcpy(argv2[0],"bitcoin-cli");
#endif
    strcpy(argv2[1+TT],"signrawtransaction");
    out_str[strlen(out_str)-1] = 0;
    strcpy(argv2[2+TT],out_str);
    sprintf(argv2[3+TT],"[{\"txid\":\"%s\",\"vout\":%d,\"scriptPubKey\":\"%s\",\"amount\":%.8lf}]",txid_src,vout_src,scriptPubKey,in_balance*0.00000001);
    cmd_run(argv2[0],&argv2[0],out_str);
    
    //get the txID
#ifdef TESTNET
    strcpy(argv2[0],"bitcoin-tx");
    strcpy(argv2[1],"-testnet");
#else
    strcpy(argv2[0],"bitcoin-tx");
#endif
    strcpy(argv2[1+TT],"-txid");
    char *ss = strchr(out_str+10,'\"');
    char *ss2 = strchr(ss+1,'\"');
    ss2[0] = 0;
    broadcasttx(ss+1);              //broadcast transaction
    strcpy(argv2[2+TT],ss+1);
    argv2[3+TT] = 0;
    cmd_run(argv2[0],&argv2[0],out_str);
    argv2[3+TT] = argv[3+TT];
    out_str[strlen(out_str)-1] = 0;
    strcpy(txid_dst,out_str);
}

//----------------------------------------------------------------------------------
// generate TX with 2 outputs, first with OP_RETURN, second with residue_balance
//----------------------------------------------------------------------------------
void gen_data_tx(const char* address, const char* scriptPubKey, char* privatekey, const char* txid_src, int vout_src, char* txid_dst, char *buf, int len, int in_balance, int* out_balance, unsigned int picie_num) {
    char hex_str[430];
    char buf_data[223]="BCHFD";
    char argv[4+TT][200000];
#ifdef BTC
    char sighashtype[20] = "ALL";
#else
    char sighashtype[20] = "ALL|FORKID";
#endif
    char privatekey_str[65] = "[\"";
    strcat(privatekey_str,privatekey);
    strcat(privatekey_str,"\"]");
#ifdef TESTNET
    char *argv2[8]={argv[0],argv[1],argv[2],argv[3],argv[4],privatekey_str,sighashtype,0};
#else
    char *argv2[7]={argv[0],argv[1],argv[2],argv[3],privatekey_str,sighashtype,0};
#endif

    //create tx
#ifdef TESTNET
    strcpy(argv2[0],"bitcoin-tx");
    strcpy(argv2[1],"-testnet");
#else
    strcpy(argv2[0],"bitcoin-tx");
#endif
    strcpy(argv2[1+TT],"-create");
    argv2[2+TT] = 0;
    cmd_run(argv2[0],&argv2[0],out_str);
    argv2[2+TT] = argv[2+TT];
    
    //add inputs
    out_str[strlen(out_str)-1] = 0;
    strcpy(argv2[1+TT],out_str);
    sprintf(argv2[2+TT],"in=%s:%d",txid_src,vout_src);
    argv2[3+TT] = 0;
    cmd_run(argv2[0],&argv2[0],out_str);
    argv2[3+TT] = argv[3+TT];

    //add op_return outputs
    out_str[strlen(out_str)-1] = 0;
    strcpy(argv2[1+TT],out_str);
    memcpy(buf_data+9,buf,len);
    *(unsigned int*)(buf_data+5) = picie_num;
    HexToStr(hex_str,buf_data,len+9);
    sprintf(argv2[2+TT],"outdata=0:%s",hex_str);
    argv2[3+TT] = 0;
    cmd_run(argv2[0],&argv2[0],out_str);
    argv2[3+TT] = argv[3+TT];
    
    //add outputs
    *out_balance = in_balance - MAXDATA*FEE - 220*FEE;  //tx fee=430, actually need 424~425
    out_str[strlen(out_str)-1] = 0;
    strcpy(argv2[1+TT],out_str);
    sprintf(argv2[2+TT],"outaddr=%.8lf:%s",(*out_balance)*0.00000001,address);
    argv2[3+TT] = 0;
    cmd_run(argv2[0],&argv2[0],out_str);
    argv2[3+TT] = argv[3+TT];

    //sign the TX
#ifdef TESTNET
    strcpy(argv2[0],"bitcoin-cli");
    strcpy(argv2[1],"-testnet");
#else
    strcpy(argv2[0],"bitcoin-cli");
#endif
    strcpy(argv2[1+TT],"signrawtransaction");
    out_str[strlen(out_str)-1] = 0;
    strcpy(argv2[2+TT],out_str);
    sprintf(argv2[3+TT],"[{\"txid\":\"%s\",\"vout\":%d,\"scriptPubKey\":\"%s\",\"amount\":%.8lf}]",txid_src,vout_src,scriptPubKey,in_balance*0.00000001);
    cmd_run(argv2[0],&argv2[0],out_str);
    
    //get the txID
#ifdef TESTNET
    strcpy(argv2[0],"bitcoin-tx");
    strcpy(argv2[1],"-testnet");
#else
    strcpy(argv2[0],"bitcoin-tx");
#endif
    strcpy(argv2[1+TT],"-txid");
    char *ss = strchr(out_str+10,'\"');
    char *ss2 = strchr(ss+1,'\"');
    ss2[0] = 0;
    broadcasttx(ss+1);              //broadcast transaction
    strcpy(argv2[2+TT],ss+1);
    argv2[3+TT] = 0;
    cmd_run(argv2[0],&argv2[0],out_str);
    argv2[3+TT] = argv[3+TT];
    out_str[strlen(out_str)-1] = 0;
    strcpy(txid_dst,out_str);
}

//----------------------------------------------------------------------------------
// generate file Head TX with tx_num inputs and 2 outputs,
// first with OP_RETURN, second with residue_balance
// combine_branch_tx(address,scriptPubKey,privatekey,txid_data[j],MAXTX,txid_branch_head[j],last_tx_in_balance,&branch_out_balance[j]);
//----------------------------------------------------------------------------------
void combine_branch_tx(const char* address, const char* scriptPubKey, char* privatekey, char txid_src[][65], int tx_num, char* txid_dst, int* in_balance, int* out_balance) {
    int i;
    char hex_str[20];
    char buf_data[20]="BCHFB";
    char str_tmp[200];
    char argv[4+TT][1000000];       //1MB
#ifdef BTC
    char sighashtype[20] = "ALL";
#else
    char sighashtype[20] = "ALL|FORKID";
#endif
    char privatekey_str[65] = "[\"";
    strcat(privatekey_str,privatekey);
    strcat(privatekey_str,"\"");
    strcat(privatekey_str,"]");
#ifdef TESTNET
    char *argv2[8]={argv[0],argv[1],argv[2],argv[3],argv[4],privatekey_str,sighashtype,0};
#else
    char *argv2[7]={argv[0],argv[1],argv[2],argv[3],privatekey_str,sighashtype,0};
#endif
    int total_in_balance = 0;
    
    //create tx
#ifdef TESTNET
    strcpy(argv2[0],"bitcoin-tx");
    strcpy(argv2[1],"-testnet");
#else
    strcpy(argv2[0],"bitcoin-tx");
#endif
    strcpy(argv2[1+TT],"-create");
    argv2[2+TT] = 0;
    cmd_run(argv2[0],&argv2[0],out_str);
    argv2[2+TT] = argv[2+TT];
    
    //add inputs
    argv2[3+TT] = 0;
    for (i=0; i<tx_num; i++) {
        out_str[strlen(out_str)-1] = 0;
        strcpy(argv2[1+TT],out_str);
        sprintf(argv2[2+TT],"in=%s:%d",txid_src[i],1);
        cmd_run(argv2[0],&argv2[0],out_str);
    }
    argv2[3+TT] = argv[3+TT];
    
    //add op_return outputs
    printf("combine_branch_tx tx_num = %d\n",tx_num);
    out_str[strlen(out_str)-1] = 0;
    strcpy(argv2[1+TT],out_str);
    *(unsigned short*)(buf_data+5) = tx_num;
    HexToStr(hex_str,buf_data,7);
    sprintf(argv2[2+TT],"outdata=0:%s",hex_str);
    argv2[3+TT] = 0;
    cmd_run(argv2[0],&argv2[0],out_str);
    argv2[3+TT] = argv[3+TT];
    
    //add outputs
    for (i=0; i<tx_num; i++)
        total_in_balance += in_balance[i];
    *out_balance = total_in_balance - tx_num*150*FEE - (69*FEE);    // 148  264
    printf ("combine_branch total_in_balance = %d, out_balance = %d, strlen(out_str) = %d\n", total_in_balance, *out_balance, strlen(out_str));
    out_str[strlen(out_str)-1] = 0;
    strcpy(argv2[1+TT],out_str);
    sprintf(argv2[2+TT],"outaddr=%.8lf:%s",*out_balance*0.00000001,address);
    argv2[3+TT] = 0;
    cmd_run(argv2[0],&argv2[0],out_str);
    argv2[3+TT] = argv[3+TT];

    //sign the TX
#ifdef TESTNET
    strcpy(argv2[0],"bitcoin-cli");
    strcpy(argv2[1],"-testnet");
#else
    strcpy(argv2[0],"bitcoin-cli");
#endif
    strcpy(argv2[1+TT],"signrawtransaction");
    out_str[strlen(out_str)-1] = 0;
    strcpy(argv2[2+TT],out_str);
    sprintf(argv2[3+TT],"[{\"txid\":\"%s\",\"vout\":%d,\"scriptPubKey\":\"%s\",\"amount\":%.8lf}",txid_src[0],1,scriptPubKey,in_balance[0]*0.00000001);
    for (i=1; i<tx_num; i++) {
        sprintf(str_tmp,",{\"txid\":\"%s\",\"vout\":%d,\"scriptPubKey\":\"%s\",\"amount\":%.8lf}",txid_src[i],1,scriptPubKey,in_balance[i]*0.00000001);
        strcat(argv2[3+TT],str_tmp);
    }
    strcat(argv2[3+TT],"]");
    cmd_run(argv2[0],&argv2[0],out_str);
    
    //get the txID
#ifdef TESTNET
    strcpy(argv2[0],"bitcoin-tx");
    strcpy(argv2[1],"-testnet");
#else
    strcpy(argv2[0],"bitcoin-tx");
#endif
    strcpy(argv2[1+TT],"-txid");
    char *ss = strchr(out_str+10,'\"');
    char *ss2 = strchr(ss+1,'\"');
    ss2[0] = 0;
    broadcasttx(ss+1);              //broadcast transaction
    strcpy(argv2[2+TT],ss+1);
    argv2[3+TT] = 0;
    cmd_run(argv2[0],&argv2[0],out_str);
    argv2[3+TT] = argv[3+TT];
    out_str[strlen(out_str)-1] = 0;
    strcpy(txid_dst,out_str);
}

//----------------------------------------------------------------------------------
// generate file Head TX with tx_num inputs and 2 outputs,
// first with OP_RETURN, second with residue_balance
//----------------------------------------------------------------------------------
void combine_tx(const char* address, const char* scriptPubKey, char* privatekey, char txid_src[][65], int tx_num, char* txid_dst, int* in_balance, char* digest, size_t filesize, char* filename) {
    int i;
    char hex_str[430];
    char buf_data[223]="BCHFM";
    char str_tmp[200];
    char argv[4+TT][1000000];       //1MB
#ifdef BTC
    char sighashtype[20] = "ALL";
#else
    char sighashtype[20] = "ALL|FORKID";
#endif
    char privatekey_str[65] = "[\"";
    strcat(privatekey_str,privatekey);
    strcat(privatekey_str,"\"");
    strcat(privatekey_str,"]");
#ifdef TESTNET
    char *argv2[8]={argv[0],argv[1],argv[2],argv[3],argv[4],privatekey_str,sighashtype,0};
#else
    char *argv2[7]={argv[0],argv[1],argv[2],argv[3],privatekey_str,sighashtype,0};
#endif
    int out_balance;
    int total_in_balance = 0;
    
    //create tx
#ifdef TESTNET
    strcpy(argv2[0],"bitcoin-tx");
    strcpy(argv2[1],"-testnet");
#else
    strcpy(argv2[0],"bitcoin-tx");
#endif
    strcpy(argv2[1+TT],"-create");
    argv2[2+TT] = 0;
    cmd_run(argv2[0],&argv2[0],out_str);
    argv2[2+TT] = argv[2+TT];
    
    //add inputs
    argv2[3+TT] = 0;
    for (i=0; i<tx_num; i++) {
        out_str[strlen(out_str)-1] = 0;
        strcpy(argv2[1+TT],out_str);
        sprintf(argv2[2+TT],"in=%s:%d",txid_src[i],1);
        cmd_run(argv2[0],&argv2[0],out_str);
    }
    argv2[3+TT] = argv[3+TT];
    
    //add op_return outputs
    printf("combine_tx tx_num = %d\n",tx_num);
    out_str[strlen(out_str)-1] = 0;
    strcpy(argv2[1+TT],out_str);
    *(unsigned short*)(buf_data+5) = tx_num;
    memcpy(buf_data+7,digest,32);
    *(unsigned int*)(buf_data+39) = tx_num;
    *(size_t*)(buf_data+43) = filesize;
    memcpy(buf_data+48,filename,160);
    HexToStr(hex_str,buf_data,208);
    sprintf(argv2[2+TT],"outdata=0:%s",hex_str);
    argv2[3+TT] = 0;
    cmd_run(argv2[0],&argv2[0],out_str);
    argv2[3+TT] = argv[3+TT];
    
    //add outputs
    for (i=0; i<tx_num; i++)
        total_in_balance += in_balance[i];
    out_balance = total_in_balance - tx_num*150*FEE - (270*FEE);    // 148  264
    printf ("total_in_balance = %d, out_balance = %d, strlen(out_str) = %d\n", total_in_balance, out_balance, strlen(out_str));
    out_str[strlen(out_str)-1] = 0;
    strcpy(argv2[1+TT],out_str);
    sprintf(argv2[2+TT],"outaddr=%.8lf:%s",out_balance*0.00000001,address);
    argv2[3+TT] = 0;
    cmd_run(argv2[0],&argv2[0],out_str);
    argv2[3+TT] = argv[3+TT];

    //sign the TX
#ifdef TESTNET
    strcpy(argv2[0],"bitcoin-cli");
    strcpy(argv2[1],"-testnet");
#else
    strcpy(argv2[0],"bitcoin-cli");
#endif
    strcpy(argv2[1+TT],"signrawtransaction");
    out_str[strlen(out_str)-1] = 0;
    strcpy(argv2[2+TT],out_str);
    sprintf(argv2[3+TT],"[{\"txid\":\"%s\",\"vout\":%d,\"scriptPubKey\":\"%s\",\"amount\":%.8lf}",txid_src[0],1,scriptPubKey,in_balance[0]*0.00000001);
    for (i=1; i<tx_num; i++) {
        sprintf(str_tmp,",{\"txid\":\"%s\",\"vout\":%d,\"scriptPubKey\":\"%s\",\"amount\":%.8lf}",txid_src[i],1,scriptPubKey,in_balance[i]*0.00000001);
        strcat(argv2[3+TT],str_tmp);
    }
    strcat(argv2[3+TT],"]");
    cmd_run(argv2[0],&argv2[0],out_str);
    
    //get the txID
#ifdef TESTNET
    strcpy(argv2[0],"bitcoin-tx");
    strcpy(argv2[1],"-testnet");
#else
    strcpy(argv2[0],"bitcoin-tx");
#endif
    strcpy(argv2[1+TT],"-txid");
    char *ss = strchr(out_str+10,'\"');
    char *ss2 = strchr(ss+1,'\"');
    ss2[0] = 0;
    broadcasttx(ss+1);              //broadcast transaction
    strcpy(argv2[2+TT],ss+1);
    argv2[3+TT] = 0;
    cmd_run(argv2[0],&argv2[0],out_str);
    argv2[3+TT] = argv[3+TT];
    out_str[strlen(out_str)-1] = 0;
    strcpy(txid_dst,out_str);
}

void get_scriptpubKey(const char* address, char* scriptPubKey) {
    char argv[4][100];
    char *argv2[5]={argv[0],argv[1],argv[2],argv[3],0};
#ifdef TESTNET
    strcpy(argv2[0],"bitcoin-tx");
    strcpy(argv2[1],"-testnet");
    strcpy(argv2[2],"-create");
#else
    strcpy(argv2[0],"bitcoin-tx");
    strcpy(argv2[1],"-create");
#endif
    sprintf(argv2[2+TT],"outaddr=%.8lf:%s",0.0,address);
    argv2[3+TT] = 0;
    cmd_run(argv2[0],&argv2[0],out_str);
    strncpy(scriptPubKey, out_str+30, 50);
    scriptPubKey[50] = 0;
    printf("scriptPubKey=%s\n",scriptPubKey);
}

char file_buf[21000000];    //21MB
char txid_dst[MAXBRANCH][65];
char txid_data[MAXBRANCH][MAXTX][65];
char txid_branch_head[MAXBRANCH][65];
char txid_filehead[65];

int main( int argc, char *argv[] ) {
    
    FILE * fp;
    size_t read_len;

    printf("How to Use: bchfile filename txid_src vout_src balance_in\n");
    
    char fname[200] = "bchfile_key";
    fp = fopen (fname, "r");
    if (!fp) {
        printf("Cannot open bchfile_key!\nbchfile_key format:\naddress\nprivatekey\n)");
        exit(0);
    }
    char address[50];
    char privatekey[60];
    fgets(address, 50, fp);
    int i;
    for (i=30; i<50; i++) {
        if((address[i] == '\n') || (address[i] == '\r')) {
            address[i] = 0;
            break;
        }
    }
    fgets(privatekey, 60, fp);
    for (i=45; i<60; i++) {
        if((privatekey[i] == '\n') || (privatekey[i] == '\r')) {
            privatekey[i] = 0;
            break;
        }
    }
    fclose(fp);
    
    if(argc != 5) {
        printf("Args Error!\n");
        exit(0);
    }
    stpcpy(fname, argv[1]);
    
    char txid_src[65];
    int vout_src;
    int balance_in;
    stpcpy(txid_src, argv[2]);
    sscanf(argv[3], "%d", &vout_src);
    sscanf(argv[4], "%d", &balance_in);
    
    fp = fopen (fname, "r");
    read_len = fread(file_buf, 1, 21000000, fp);
    fclose(fp);
    
    printf("file length = %d\n",read_len);
    char filename[161];
    memset(filename,0,161);
    
    stpcpy(filename,fname);
    char scriptPubKey[55];      //pubkeyhash
    get_scriptpubKey(address, scriptPubKey);
    
    size_t utxo_num = (read_len-1)/MAXDATA;
    if (utxo_num*MAXDATA < read_len)
        utxo_num ++;
    int utxo_first = (read_len-1)/(MAXTX*MAXDATA) + 1;  //first layer utxo numbers
    int data_res = (read_len-1)%(MAXTX*MAXDATA) + 1;        //first layer, last utxo refered data

    int utxo_second_res = (data_res-1)/MAXDATA + 1;     //second layer, last utxos numbers, from 1 to MAXTX
                                                    //second layer, else utxos numbers = MAXTX
    int data_second_res = (data_res-1)%MAXDATA + 1;     //second layer, residue of last of last utxos refered data, from 1 to 210
    
    printf("utxo_first = %d, data_res = %d, utxo_second_res = %d, data_second_res = %d\n", utxo_first, data_res, utxo_second_res, data_second_res);

    int last_utxo_balance;
    int last_tx_in_balance[MAXTX] = {0};
    
    char txid_branch_src[65];
    int balance_branch_in[MAXBRANCH] = {0};
    int branch_out_balance[MAXBRANCH] = {0};
    
    char argv1[2][200];
    char *argv2[3]={argv1[0],argv1[1],0};
    strcpy(argv1[0],"sha256sum");
    strcpy(argv1[1],fname);
    cmd_run(argv2[0],&argv2[0],out_str);
    printf("sha256sum = %s",out_str);
    char hex_digest[65];
    hex_digest[64] = 0;
    memcpy(hex_digest,out_str,64);
    char digest[33];
    StrToHex(digest,hex_digest,32);
    
    fp_alltx = fopen("upload_TXs","w+");
    
    printf ("gen utxo...\n");
    if (utxo_first == 1) {  //read_len <= MAXDATA*MAXTX
        int i;
        //generate tx with utxo_second_res outputs, each with 1000 satoshis
        gen_utxos(address,scriptPubKey,privatekey,txid_src,vout_src,utxo_second_res,txid_dst[0],balance_in,TX_DATA_SATOSHI,&last_utxo_balance);
        for (i=0; i<utxo_second_res-1; i++) {
            printf ("gen data tx, i = %d\n", i);
            gen_data_tx(address,scriptPubKey,privatekey,txid_dst[0],i,txid_data[0][i],file_buf+i*MAXDATA,MAXDATA,TX_DATA_SATOSHI,&last_tx_in_balance[i],i+1);
        }
        printf ("last gen data tx, i = %d\n", i);
        gen_data_tx(address,scriptPubKey,privatekey,txid_dst[0],i,txid_data[0][i],file_buf+i*MAXDATA,data_second_res,last_utxo_balance,&last_tx_in_balance[i],i+1); //last data tx
        
        printf ("combinetx\n");
        combine_tx(address,scriptPubKey,privatekey,txid_data[0],utxo_second_res,txid_filehead,last_tx_in_balance,digest,read_len,filename);
        printf ("txid_filehead = %s\n", txid_filehead);
    }
    else {
        int i, j;
        gen_utxos(address,scriptPubKey,privatekey,txid_src,vout_src,utxo_first,txid_branch_src,balance_in,MAXTX*((36*FEE)+TX_DATA_SATOSHI)+(164*FEE),&last_utxo_balance);
        for (j=0; j<utxo_first-1; j++) {
            balance_branch_in[j] = MAXTX*((36*FEE)+TX_DATA_SATOSHI)+(164*FEE);
        }
        balance_branch_in[j] = last_utxo_balance;
        for (j=0; j<utxo_first-1; j++) {
            printf ("gen utxo...\n");
            gen_utxos(address,scriptPubKey,privatekey,txid_branch_src,j,MAXTX,txid_dst[j],balance_branch_in[j],TX_DATA_SATOSHI,&last_utxo_balance); //generate tx with MAXTX outputs, each with 1000 satoshis
            for (i=0; i<MAXTX-1; i++) {
                printf ("gen data tx, i = %d, j = %d\n", i, j);
                gen_data_tx(address,scriptPubKey,privatekey,txid_dst[j],i,txid_data[j][i],file_buf+i*MAXDATA+j*(MAXTX*MAXDATA),MAXDATA,TX_DATA_SATOSHI,&last_tx_in_balance[i],i+1+j*MAXTX);
            }
            printf ("last gen data tx, i = %d, j = %d\n", i, j);
            gen_data_tx(address,scriptPubKey,privatekey,txid_dst[j],i,txid_data[j][i],file_buf+i*MAXDATA+j*(MAXTX*MAXDATA),MAXDATA,last_utxo_balance,&last_tx_in_balance[i],i+1+j*MAXTX);   //last data tx
            combine_branch_tx(address,scriptPubKey,privatekey,txid_data[j],MAXTX,txid_branch_head[j],last_tx_in_balance,&branch_out_balance[j]);
        }
        printf ("gen last utxo...\n");
        //generate tx with utxo_second_res outputs, each with 1000 satoshis
        gen_utxos(address,scriptPubKey,privatekey,txid_branch_src,j,utxo_second_res,txid_dst[j],balance_branch_in[j],TX_DATA_SATOSHI,&last_utxo_balance);
        for (i=0; i<utxo_first; i++) printf ("txid_dst[j] = %s\n", txid_dst[i]);
        
        for (i=0; i<utxo_second_res-1; i++) {
            printf ("2 gen data tx, i = %d, j = %d\n", i, j);
            gen_data_tx(address,scriptPubKey,privatekey,txid_dst[j],i,txid_data[j][i],file_buf+i*MAXDATA+j*(MAXTX*MAXDATA),MAXDATA,TX_DATA_SATOSHI,&last_tx_in_balance[i],i+1+j*MAXTX);
        }
        printf ("2 last gen data tx, i = %d, j = %d\n", i, j);
        gen_data_tx(address,scriptPubKey,privatekey,txid_dst[j],i,txid_data[j][i],file_buf+i*MAXDATA+j*(MAXTX*MAXDATA),data_second_res,last_utxo_balance,&last_tx_in_balance[i],i+1+j*MAXTX);   //last data tx
        combine_branch_tx(address,scriptPubKey,privatekey,txid_data[j],utxo_second_res,txid_branch_head[j],last_tx_in_balance,&branch_out_balance[j]);
        
        printf ("last combine_tx\n");
        for (i=0; i<utxo_first; i++) printf ("txid_branch_head[i] = %s\n", txid_branch_head[i]);
        
        combine_tx(address,scriptPubKey,privatekey,txid_branch_head,utxo_first,txid_filehead,branch_out_balance,digest,read_len,filename);
        printf ("txid_filehead = %s\n", txid_filehead);
    }
    
    fclose(fp_alltx);
    return 0;
}
