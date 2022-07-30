#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
//读取shell中一行命令
char* readline() {
    char* buf = malloc(100);
    char* p = buf;
    while(read(0, p, 1) != 0){
        if(*p == '\n' || *p == '\0'){
            *p = '\0';
            return buf;
        }
        p++;
    }
    if(p != buf) return buf;
    free(buf);
    return 0;
}
int
main(int argc, char *argv[]){
    if(argc < 2) {
        fprintf(2,"Please input the option\n");
        exit(-1);
    }
    //复制参数值
    char* argvs[32];//存储多个命令参
    char** p_argvs = argvs;
    char** p_argv = argv+1;//除去命令名称自身值
    while(*p_argv != 0){
        *(p_argvs++) = *(p_argv++);
    }

  
    while(1){
        char* p_line= readline();
        if(p_line==0){
            break;
        }

        char* buf = malloc(36);
        char* bh = buf;
        int nargc = argc - 1;//除去命令名称自身
        while(*p_line != 0){
            //取参
            if(*p_line == ' ' && buf != bh){
                //若到达末尾且长度不为0
                *bh = 0;
                argvs[nargc] = buf;
                buf = malloc(36);
                bh = buf;
                nargc++;
            }else{
                *bh = *p_line;
                bh++;
            }
            p_line++;
        }
        if(buf != bh){
            argvs[nargc] = buf;
            nargc++;
        }
        argvs[nargc] = 0;
        int pid = fork();
        if(pid == 0){
            // printf("%s %s\n", argvs[0], argvs[1]);
            exec(argvs[0], argvs);
        }else{
            wait(0);
        }
    }
    exit(0);
}