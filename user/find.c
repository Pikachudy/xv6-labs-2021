#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include <stddef.h>
//返回字符在字符串中最后出现的位置的指针
char* strrchr( char *str, char c){
    char *p_pos=NULL;
    for(char* p=str;*p!='\0';p++){
        if(*p==c){
            p_pos=p;
        }
    }
    return p_pos;
}
void find(char* path,char* filename){
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;//存储文件信息的数据结构
    if((fd = open(path, 0)) < 0){
      fprintf(2, "find: cannot open %s\n", path);
      return;
     }

    if(fstat(fd, &st) < 0){
       fprintf(2, "find: cannot stat %s\n", path);
       close(fd);
       return;
    }

    switch(st.type){
        //若为文件则判断后输出
        case T_FILE:{
            //printf("%s %s\n", path, filename);
            char* p_pos=strrchr(path,'/');//'/'最后一次出现的位置加一，则为文件名起始位置
            p_pos++;
            //printf("%c\n", *p_pos);
            //提取文件名称
            char *p_name=(char*)malloc(sizeof(char)*strlen(p_pos));
            char *p_begin=p_name;
            for(char* p=p_pos;*p!='\0';p++){
                *(p_begin++)=*p;
            }
            if(strcmp(p_name,filename)==0){
                fprintf(1,"%s\n", path);
            }
            break;
        }
        //若为文件夹则在ls处理的基础上，忽略./..后在原先打印处进行递归
        case T_DIR:
            if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
                printf("find: path too long\n");
                break;
            }
            strcpy(buf, path);
            p = buf+strlen(buf);
            *p++ = '/';
            while(read(fd, &de, sizeof(de)) == sizeof(de)){
                if(de.inum == 0)
                    continue;
                //在ls代码中添加这两项，为了确保忽略掉./..
                if(de.name[0] == '.' && de.name[1] == 0) 
                    continue;
                if(de.name[0] == '.' && de.name[1] == '.' && de.name[2] == 0) 
                    continue;
                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = 0;
                if(stat(buf, &st) < 0){
                    printf("find: cannot stat %s\n", buf);
                    continue;
                }
                //printf("%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
                find(buf,filename);//在此处文件夹递归，而非只是打印文件夹信息
            }
            break;
    }
    close(fd);
}
int 
main(int argc,char* argv[]){
    if(argc<3){
        fprintf(2,"Please input the path and filename\n");
        exit(-1);
    }
    else{
        find(argv[1],argv[2]);
    }
    exit(0);
}