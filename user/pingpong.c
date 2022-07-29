#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
int
main(int argc,char *argv[]){
    if(argc!=1){
        exit(1);
    }
    int p[2],c[2];
    if(pipe(p)==-1||pipe(c)==-1){
       exit(2);
    }
    int pid = fork();
    if(pid<0){
        exit(3);
    }
    else if(pid>0){
        int p_pid = getpid();
        char msg[1];
        write(p[1],"H",1);
        read(c[0],msg,1);
        fprintf(1,"%d: received pong\n",p_pid);
    }
    else{
        int c_pid = getpid();
        char msg[1];
        read(p[0],msg,1);
        fprintf(1,"%d: received ping\n",c_pid);
        write(c[1],"H",1);
    }
    close(p[0]);
    close(p[1]);
    close(c[0]);
    close(c[1]);
    exit(0);
}