#define FUSE_USE_VERSION 28
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

static const char* source_dir;

static void get_full_path(char* full_path, const char* path){
    snprintf(full_path, 1024, "%s%s", source_dir, path);
}

static char* build_tujuan_content(){
    static char content[6000];
    char result[5000];
    result[0] = '\0';

for(int i = 1; i <= 7; i++){
    char filepath[500];
    snprintf(filepath, sizeof(filepath), "%s/%d.txt", source_dir, i);
    
    FILE* f = fopen(filepath, "r");
    if(!f) continue;

    char line [1000];
    while(fgets(line, sizeof(line), f)){
        if(strncmp(line, "KOORD:", 6) == 0){
            char* fragment = line + 6;
            while(* fragment == ' ')
            fragment++;
            int len = strlen(fragment);
            while(len > 0 && (fragment[len - 1] == '\n' || fragment[len - 1] == '\r')){
                fragment[len - 1] = '\0';
                len--;
            }
            strcat(result, fragment);
        }
    }
    fclose(f);
}
snprintf(content, sizeof(content), "Tujuan Mas Amba: %s\n", result);
return content;
}
static int kenz_getattr(const char* path, struct stat* stbuf){
    memset(stbuf, 0, sizeof(struct stat));

    if(strcmp(path, "/") == 0){
        stbuf -> st_mode = S_IFDIR | 0755;
        stbuf -> st_nlink = 2;
        return 0;
    }

    if(strcmp(path, "/tujuan.txt") == 0){
        stbuf -> st_mode = S_IFREG | 0444;
        stbuf -> st_nlink = 1;
        char* content = build_tujuan_content();
        stbuf -> st_size = strlen(content);
        return 0;
    }

    char full_path[1000];
    get_full_path(full_path, path);
    int res = lstat(full_path, stbuf);
    if(res == -1)
    return -errno;
    return 0;
}
static int kenz_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi) {
    (void) offset;
    (void) fi;

    if (strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    DIR* dp = opendir(source_dir);
    if (!dp) return -errno;

    struct dirent* de;
    while ((de = readdir(dp)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
        continue;
        filler(buf, de -> d_name, NULL, 0);
    }
    closedir(dp);
    filler(buf, "tujuan.txt", NULL, 0);
    return 0;
}
static int kenz_open(const char* path, struct fuse_file_info* fi) {
    if (strcmp(path, "/tujuan.txt") == 0) {
        if ((fi -> flags & O_ACCMODE) != O_RDONLY)
        return -EACCES;
        return 0;
    }
    char full_path[1000];
    get_full_path(full_path, path);
    int res = open(full_path, fi -> flags);
    if (res == -1) 
    return -errno;
    close(res);
    return 0;
}
static int kenz_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    (void) fi;
    if (strcmp(path, "/tujuan.txt") == 0) {
        char* content = build_tujuan_content();
        size_t len = strlen(content);
        if (offset >= (off_t)len) return 0;
        if (offset + size > len) size = len - offset;
        memcpy(buf, content + offset, size);
        return size;
    }
    char full_path[1000];
    get_full_path(full_path, path);
    int fd = open(full_path, O_RDONLY);
    if (fd == -1) return -errno;
    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;
    close(fd);
    return res;
}
static struct fuse_operations kenz_oper = {
    .getattr = kenz_getattr,
    .readdir = kenz_readdir,
    .open    = kenz_open,
    .read    = kenz_read,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_directory> <mount_directory>\n", argv[0]);
        return 1;
    }
    source_dir = argv[1];
    argv[1] = argv[2];
    argc--;
    return fuse_main(argc, argv, &kenz_oper, NULL);
}
