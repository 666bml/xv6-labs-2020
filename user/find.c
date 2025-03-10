//
// Created by bml on 25-3-4.
//
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// 递归查找函数（在某个路径下，寻找某个文件）
void find(const char *path, const char *filename) {
    int fd;
    struct stat st; //用于存储文件状态信息的结构体
    struct dirent de;  //用于存储目录项信息的结构体
    char buf[512], *p; //缓冲区，用于存储路径字符串

    // 获取path对应的文件信息，path路径是文件（T_FILE）或文件夹（即目录T_DIR）
    if(stat(path, &st) < 0){
        fprintf(2, "find: cannot stat %s\n", path);
        return;
    }

    // 如果是普通文件，检查文件名是否匹配
    if(st.type == T_FILE){
//        printf("2");
        // 从路径中提取最后的文件名部分
        // 找到最后一个'/'
        const char *last_slash = path;
        for(const char *t = path; *t; t++){
            if(*t == '/'){
                last_slash = t + 1;  // 指向 '/' 后的第一个字符
            }
        }
        // 比较与目标文件名
        if(strcmp(last_slash, filename) == 0){
            // 如果匹配则打印
            printf("%s\n", path);
        }
        return;
    }

    // 如果是目录，则递归遍历
    if(st.type == T_DIR){
//      printf("1");
        // 打开目录
        if((fd = open(path, 0)) < 0){
            fprintf(2, "find: cannot open %s\n", path);
            return;
        }

        // 将buf初始化为当前目录路径，加上'/'
        strcpy(buf, path);
        p = buf + strlen(buf);
        // 如果路径末尾不是'/'，加一个'/'
        if(p > buf && *(p-1) != '/'){
            *p = '/';
            p++;
        }

        // 逐条读取目录项（即该路径下的文件和文件夹）
        while(read(fd, &de, sizeof(de)) == sizeof(de)){
            // 如果 inum 为 0，说明该目录项为空（文件或者空文件夹）
            if(de.inum == 0)
                continue;
            // 跳过 "." 和 ".."，防止死循环
            if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue;

            // 将目录项名字拼接到 buf 后面形成新的路径
            memmove(p, de.name, strlen(de.name));
            p[strlen(de.name)] = 0; // 以 '\0' 结尾

            // 递归调用 find
            find(buf, filename);
        }
        close(fd);
    }
}

int main(int argc, char *argv[])
{
    if(argc < 3){
        fprintf(2, "Usage: find <path> <filename>\n");
        exit(1);
    }

    find(argv[1], argv[2]);
    exit(0);
}

