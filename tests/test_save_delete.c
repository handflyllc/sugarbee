#define _POSIX_C_SOURCE 200809L
#include "sb_save.h"
#include "vcart.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Line %d: %s\n",__LINE__,#x); return 1; } } while(0)
int __real_fsync(int fd);
int __real_close(int fd);
int __real_unlinkat(int fd,const char *name,int flags);
static int fault, injected, unlinks, syncs;
int __wrap_fsync(int fd) {
    ++syncs;
    if (fault==2 || (fault==4 && !injected)) { ++injected; errno=fault==4?EINTR:EIO; return -1; }
    return __real_fsync(fd);
}
int __wrap_close(int fd) {
    int result=__real_close(fd);
    if(fault==3) { ++injected; errno=EIO; return -1; }
    return result;
}
int __wrap_unlinkat(int fd,const char *name,int flags) {
    ++unlinks;
    if(fault==1) { ++injected; errno=EACCES; return -1; }
    return __real_unlinkat(fd,name,flags);
}
int main(void) {
    char root[]="/tmp/sbee-delete-XXXXXX"; CHECK(mkdtemp(root));
    uint8_t a[16]={1}, b[16]={2};
    char uuid[37], path[256], missing[256];
    vcart_uuid_to_string(a,uuid);
    snprintf(path,sizeof(path),"%s/%s.sav",root,uuid);
    snprintf(missing,sizeof(missing),"%s/missing/nested",root);
    CHECK(sb_save_delete(NULL,a)==SB_SAVE_INVALID_ARGUMENT);
    CHECK(sb_save_delete(root,NULL)==SB_SAVE_INVALID_ARGUMENT);
    CHECK(sb_save_delete(missing,a)==SB_SAVE_OK && access(missing,F_OK)!=0);
    CHECK(sb_save_delete(root,a)==SB_SAVE_OK && unlinks==0 && syncs==1);
    CHECK(sb_save_write(root,b,(const uint8_t *)"other",5)==SB_SAVE_OK);
    for(int f=0;f<=4;++f) {
        CHECK(sb_save_write(root,a,(const uint8_t *)"data",4)==SB_SAVE_OK);
        fault=f; injected=unlinks=syncs=0;
        sb_save_result_t result=sb_save_delete(root,a);
        CHECK(unlinks==1);
        CHECK(f==0 || injected==1);
        CHECK(result==(f==1?SB_SAVE_IO_ERROR:(f==2||f==3)?SB_SAVE_DURABILITY_UNCERTAIN:SB_SAVE_OK));
        CHECK((access(path,F_OK)==0)==(f==1));
        if(f==4) CHECK(syncs==2);
        fault=0;
        CHECK(sb_save_delete(root,a)==SB_SAVE_OK);
        CHECK(sb_save_delete(root,a)==SB_SAVE_OK);
        uint8_t *data=NULL; size_t length=0;
        CHECK(sb_save_read(root,a,&data,&length)==SB_SAVE_NOT_FOUND);
        CHECK(sb_save_read(root,b,&data,&length)==SB_SAVE_OK && length==5 && !memcmp(data,"other",5));
        free(data);
    }
    CHECK(symlink("missing-target",path)==0);
    CHECK(sb_save_delete(root,a)==SB_SAVE_IO_ERROR);
    struct stat st; CHECK(lstat(path,&st)==0 && S_ISLNK(st.st_mode)); CHECK(unlink(path)==0);
    CHECK(mkfifo(path,0600)==0); CHECK(sb_save_delete(root,a)==SB_SAVE_IO_ERROR); CHECK(unlink(path)==0);
    CHECK(mkdir(path,0700)==0); CHECK(sb_save_delete(root,a)==SB_SAVE_IO_ERROR); CHECK(rmdir(path)==0);
    /* Oversized or corrupt save bytes must still be erasable. */
    int fd=open(path,O_WRONLY|O_CREAT,0600); CHECK(fd>=0);
    CHECK(ftruncate(fd,SB_SAVE_MAX_BYTES+1)==0 && close(fd)==0);
    CHECK(sb_save_delete(root,a)==SB_SAVE_OK);
    CHECK(sb_save_delete(root,b)==SB_SAVE_OK && rmdir(root)==0);
    return 0;
}
