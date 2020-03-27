#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

#include "../h/minirel.h"
#include "../h/bf.h"
#include "../h/pf.h"

#include "pfinternal.h"

#define FILE_CREATE_MASK (S_IRUSR|S_IWUSR|S_IRGRP)

int PFerrno = 1;

PFftab_ele *freePFHead;
int numPFftab_ele = 0;

static char header[PAGE_SIZE];
PFhdr_str hdrStr;
PFftab_ele *PFftab;

static int setErr(int err){
    return PFerrno = err;
}

void PF_Init		(void){
    int i;

    BF_Init();

    PFftab = (PFftab_ele*)malloc(sizeof(PFftab_ele) * PF_FTAB_SIZE);
    for(i=0; i<PF_FTAB_SIZE-1; i++){
        PFftab[i].next_ele = &PFftab[i+1];
        PFftab[i].valid = FALSE;
    }
    PFftab[PF_FTAB_SIZE-1].next_ele = NULL;
    PFftab[PF_FTAB_SIZE-1].valid = FALSE;

    freePFHead = PFftab;
}
int  PF_CreateFile	(const char *filename){ 
    int unixfd;
    if ((unixfd = open(filename, O_RDWR|O_CREAT|O_EXCL, FILE_CREATE_MASK))<0){
        /*file open fail*/
        return setErr(PFE_UNIX);
    }

    memset(header, 0x00, PAGE_SIZE);
    hdrStr.numpages = 0;
    memcpy(&header, &hdrStr, sizeof(hdrStr));

    if(write(unixfd, header, PAGE_SIZE) != PAGE_SIZE)
        return setErr(PFE_UNIX);
    
    if(close(unixfd)<0)
        return setErr(PFE_UNIX);

    return PFE_OK;
}
int  PF_DestroyFile	(const char *filename){
    int error; 
    int i;

    if((error =access(filename, F_OK))<0)   
        return setErr(PFE_FD);
    
    for(i=0; i<MAXOPENFILES; i++){
        if(PFftab[i].valid){
            if(strcmp(PFftab[i].fname, filename)==0){
                return setErr(PFE_FILEOPEN);
            }
        }
    }
    if((error=unlink(filename))<0)
        return setErr(PFE_UNIX);

    return PFE_OK;
}
int  PF_OpenFile	(const char *filename){
    int unixfd;
    PFftab_ele *newEle;
    struct stat file_stat;
    int error;
    int nbytes;
    PFhdr_str newHdr;

    if(numPFftab_ele >= MAXOPENFILES)
        return setErr(PFE_FTABFULL);
    
    if((unixfd = open(filename, O_RDWR))<0)
        return setErr(PFE_UNIX);
    
    newEle = freePFHead;
    freePFHead = freePFHead->next_ele;

    if((error = fstat(unixfd, &file_stat))<0)
        return setErr(PFE_UNIX);

    newEle->valid = TRUE;
    newEle->inode = file_stat.st_ino;
    strcpy(newEle->fname, filename);
    newEle->unixfd = unixfd;
    newEle->hdrchanged = FALSE;
    newEle->next_ele = NULL;

    lseek(unixfd, 0, SEEK_SET);
    nbytes = read(unixfd, &newHdr, sizeof(PFhdr_str));

    if(nbytes != sizeof(PFhdr_str))
        return setErr(PFE_UNIX);

    newEle->hdr = newHdr;
    return newEle - PFftab;
}
int  PF_CloseFile	(int fd){
    int err;
    int nbytes;

    if(fd <0 ||!PFftab[fd].valid)
        return setErr(PFE_FILENOTOPEN);
    
    if((err = BF_FlushBuf(fd))<0){
        BF_PrintError("cannot flush");
        return setErr(PFE_MSGERR);
    }
    
    if(PFftab[fd].hdrchanged){
        lseek(PFftab[fd].unixfd, 0, SEEK_SET);
        if((nbytes = write(PFftab[fd].unixfd, &(PFftab[fd].hdr), sizeof(PFhdr_str))) != sizeof(PFhdr_str) )
            return setErr(PFE_UNIX);
    }
    PFftab[fd].valid = FALSE;
    PFftab[fd].next_ele = freePFHead;
    freePFHead = &(PFftab[fd]);

    numPFftab_ele--;

    return PFE_OK;
}
int  PF_AllocPage	(int fd, int *pagenum, char **pagebuf){
    int msg;
    BFreq new_page_req;
    PFpage *new_page_data;

    if(fd<0 || fd>=MAXOPENFILES ||!PFftab[fd].valid)
        return setErr(PFE_FILENOTOPEN);

    *pagenum = PFftab[fd].hdr.numpages;
    PFftab[fd].hdr.numpages++;
    PFftab[fd].hdrchanged = TRUE;

    new_page_req.fd = fd;
    new_page_req.unixfd = PFftab[fd].unixfd;
    new_page_req.pagenum = *pagenum;
    new_page_req.dirty = TRUE;
    
    if((msg = BF_AllocBuf(new_page_req, &new_page_data))<0)
        return setErr(PFE_INVALIDPAGE);
    
    *pagebuf = new_page_data->pagebuf;
    return PFE_OK;
}
int  PF_GetFirstPage	(int fd, int *pagenum, char **pagebuf){
    *pagenum = -1;
    return PF_GetNextPage(fd, pagenum, pagebuf);
}
int  PF_GetNextPage	(int fd, int *pagenum, char **pagebuf){
    int msg;
    BFreq req;

    if(fd<0 || fd>=MAXOPENFILES || !PFftab[fd].valid)
        return setErr(PFE_FILENOTOPEN);
    
    if(PFftab[fd].hdr.numpages <= *pagenum +1 )
        return setErr(PFE_EOF);
    
    req.fd = fd;
    req.pagenum = *pagenum + 1;
    req.unixfd = PFftab[fd].unixfd;
    req.dirty = FALSE;

    if((msg = BF_GetBuf(req, (PFpage **)pagebuf))<0)
        return setErr(PFE_INVALIDPAGE);
    
    *pagenum = *pagenum+1;
    return PFE_OK;
}
int  PF_GetThisPage	(int fd, int pagenum, char **pagebuf){
    BFreq req;

    if(fd<0 || fd>=MAXOPENFILES || !PFftab[fd].valid)
        return setErr(PFE_FILENOTOPEN);
    
    if(pagenum <0 || PFftab[fd].hdr.numpages <= pagenum)
        return setErr(PFE_INVALIDPAGE);

    req.fd = fd;
    req.pagenum = pagenum;
    req.unixfd = PFftab[fd].unixfd;
    req.dirty = FALSE;

    if(BF_GetBuf(req, (PFpage **) pagebuf)<0)
        return setErr(PFE_INVALIDPAGE);
    
    return PFE_OK;
}
int  PF_DirtyPage	(int fd, int pagenum){
    BFreq req;
    if(fd<0 || fd >= MAXOPENFILES || !PFftab[fd].valid)
        return setErr(PFE_FILENOTOPEN);
    
    if(PFftab[fd].hdr.numpages <= pagenum)
        return setErr(PFE_INVALIDPAGE);
    
    req.fd = fd;
    req.pagenum = pagenum;
    req.unixfd = PFftab[fd].unixfd;
    req.dirty = TRUE;

    if(BF_TouchBuf(req)<0)
        return setErr(PFE_INVALIDPAGE);
    
    return PFE_OK;
}
int  PF_UnpinPage	(int fd, int pagenum, int dirty){
    BFreq req;

    if(fd<0 || fd>=MAXOPENFILES || !PFftab[fd].valid)
        return setErr(PFE_FILENOTOPEN);

    if(PFftab[fd].hdr.numpages <= pagenum)
        return setErr(PFE_INVALIDPAGE);
    
    req.fd = fd;
    req.pagenum = pagenum;
    req.unixfd = PFftab[fd].unixfd;
    req.dirty = dirty;

    if(dirty)
        if(BF_TouchBuf(req)<0)
            return setErr(PFE_INVALIDPAGE);
        
    if(BF_UnpinBuf(req)<0)
        return setErr(PFE_INVALIDPAGE);
    
    return PFE_OK;
}
void PF_PrintError(const char *s){
    fputs(s, stderr);
    fputs("\n", stderr);
    switch(PFerrno){
        case 1:
            /*not initialized*/
            break;
        case 0:
            /*PFE_OK**/
            break;
        case -1:
            fputs("PFE_INVALIDPAGE",stderr);
            break;
        case -2:
            fputs("PFE_FTABFULL",stderr);
            break;
        case -3:
            fputs("PFE_FD",stderr);
            break;
        case -4:
            fputs("PFE_EOF",stderr);
            break;
        case -5:
            fputs("PFE_FILEOPEN",stderr);
            break;
        case -6:
            fputs("PFE_FILENOTOPEN",stderr);
            break;
        case -7:
            fputs("PFE_HDRREAD",stderr);
            break;
        case -8:
            fputs("PFE_HDRWRITE",stderr);
            break;
        case -9:
            fputs("PFE_PAGEFREE",stderr);
            break;
        case -10:
            fputs("PFE_NOUSERS",stderr);
            break;
        case -11:
            fputs("PFE_MSGERR",stderr);
            break;
    }
    fputs("\n",stderr);
}
