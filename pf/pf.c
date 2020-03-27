#include <stddef.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>

#include <assert.h>


#include "../h/minirel.h"
#include "../h/bf.h"
#include "../h/pf.h"

#include "pfinternal.h"

#define FILE_CREATE_MASK (S_IRUSR|S_IWUSR|S_IRGRP)

int PFerrno = 1;

PFftab_ele *freePFHead;
int numPFftab_ele=0;

char header[PAGE_SIZE];
PFhdr_str hdrStr;
PFftab_ele *PFftab;

PFhash_entry *PFhashT[PF_HASH_TBL_SIZE]={NULL};
PFhash_entry PFfreeHash[MAXOPENFILES];
PFhash_entry *PFfreeHashHead;

static int setErr(int err){
    return (PFerrno = err);
}

void PF_Init(void){
    int i;
    PFftab = (PFftab_ele*)malloc(sizeof(PFftab_ele) * PF_FTAB_SIZE);

    BF_Init();

    /*make all free and invalid*/
    for(i=0; i<PF_FTAB_SIZE-1; i++){
        PFftab[i].next_ele = &PFftab[i+1];
        PFftab[i].valid = FALSE;
    }
    PFftab[PF_FTAB_SIZE-1].next_ele  = NULL;
    PFftab[PF_FTAB_SIZE-1].valid = FALSE;

    freePFHead = &PFftab[0];

    for(i=0; i<MAXOPENFILES-1; i++)
        PFfreeHash[i].nextentry = &PFfreeHash[i+1];
    PFfreeHash[MAXOPENFILES-1].nextentry = NULL;
    PFfreeHashHead = &PFfreeHash[0];
}
int PF_CreateFile(const char *filename){
    int fd, unixfd;
    if ((unixfd = open(filename, O_RDWR|O_CREAT|O_EXCL, FILE_CREATE_MASK))<0){
        /*file open fail*/
        return setErr(PFE_UNIX);
    }

    memset(header, 0x00, PAGE_SIZE);
    hdrStr.numpages = 1;
    memcpy(&header, &hdrStr, sizeof(hdrStr));

    if(write(unixfd, header, PAGE_SIZE) != PAGE_SIZE)
        return setErr(PFE_UNIX);

    if((close(unixfd))<0)
        return setErr(PFE_UNIX);

    return PFE_OK;
}
int PF_DestroyFile(const char *filename){
    int error;
    int i;

    /*file must exist*/
    if((error=access(filename, F_OK))<0)
        return setErr(PFE_FD);

    if(isInPFHash(filename)){ /*file is open*/
        return setErr(PFE_FILEOPEN);
    }

    /*
    for(i=0; i<MAXOPENFILES; i++)
        if(PFftab[i].valid)
            if(strcmp(PFftab[i].fname, filename)==0){
                printf("file is still open\n");
                return setErr(PFE_FILEOPEN);
            }
    */
    
    if((error = unlink(filename))<0)
        return setErr(PFE_UNIX);
    
    return PFE_OK;
}
int PF_OpenFile(const char *filename){
    int fd, unixfd;
    PFftab_ele *newEle;
    struct stat file_stat;
    int error;
    int nbytes;
    PFhdr_str newHdr;

    if(numPFftab_ele >= MAXOPENFILES){
        /*to many files*/
        return setErr(PFE_FTABFULL);
    }
    
    /*file must be closed*/
    if(isInPFHash(filename))
        return setErr(PFE_FILEOPEN);

    if((unixfd = open(filename, O_RDWR))<0){
        return setErr(PFE_UNIX);
    }

    newEle = freePFHead;
    freePFHead = freePFHead->next_ele;

    if((error = fstat(unixfd, &file_stat))<0){
        /*error*/
        return setErr(PFE_UNIX);
    }
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

    numPFftab_ele++;

    /*add to hash*/
    PFaddToHash(newEle);
    
    return newEle - PFftab;
}
int PF_CloseFile(int fd){
    int err;
    int nbytes;
    
    if(!PFftab[fd].valid)
        return setErr(PFE_FILENOTOPEN);

    if((err=BF_FlushBuf(fd)<0)){
        BF_PrintError("Cannot Flush");
        return setErr(PFE_MSGERR);
    }
    
    if(PFftab[fd].hdrchanged){
        lseek(PFftab[fd].unixfd, 0,SEEK_SET); /*go to the header*/
        if((nbytes = write(PFftab[fd].unixfd, &(PFftab[fd].hdr), sizeof(PFhdr_str))) != sizeof(PFhdr_str))
            return setErr(PFE_UNIX);
    }

    /*remove from hash*/
    PFremoveFromHash(&PFftab[fd]);

    PFftab[fd].valid = FALSE;
    PFftab[fd].next_ele = freePFHead;
    freePFHead = &(PFftab[fd]);

    numPFftab_ele--;

    if(close(PFftab[fd].unixfd)<0)
        return setErr(PFE_UNIX);

    return PFE_OK;
}
int PF_AllocPage(int fd, int *pagenum, char **pagebuf){
    int msg;
    BFreq new_page_req;
    PFpage *new_page_data;

    if(!PFftab[fd].valid)
        return setErr(PFE_FILENOTOPEN);

    /*Update hdr in PF file table*/
    PFftab[fd].hdr.numpages++;
    /*Update file header*/
    *pagenum = PFftab[fd].hdr.numpages - 1;
    PFftab[fd].hdrchanged = TRUE;
    /*Create a BFreq*/
    new_page_req.fd = fd;
    new_page_req.unixfd = PFftab[fd].unixfd;
    new_page_req.pagenum = *pagenum;
    new_page_req.dirty = TRUE;

    if((msg = BF_AllocBuf(new_page_req, &new_page_data)) < 0)
        return setErr(PFE_INVALIDPAGE);

    *pagebuf = new_page_data->pagebuf;
    return PFE_OK;
}
int PF_GetNextPage(int fd, int *pagenum, char **pagebuf){
    int msg;
    BFreq req;
    PFpage *req_fpage;

    if(!PFftab[fd].valid)
        return setErr(PFE_FILENOTOPEN);
  
    /*Check to see if the next page exists*/
    if(PFftab[fd].hdr.numpages <= (*pagenum + 1))
        return setErr(PFE_EOF);

    /*Buffer Request*/
    req.fd = fd;
    req.pagenum = *pagenum + 1;
    req.unixfd = PFftab[fd].unixfd;
    req.dirty = FALSE;

    /*Set pointer *pagebuf to point to start of page data*/
    if((msg = BF_GetBuf(req, &req_fpage)) < 0)
        return setErr(PFE_INVALIDPAGE);

    *pagebuf = req_fpage->pagebuf;
    *pagenum = *pagenum +1;
    return PFE_OK;
}
int PF_GetFirstPage(int fd, int *pagenum, char **pagebuf){
    *pagenum = -1;
    return PF_GetNextPage(fd, pagenum, pagebuf);
}
int PF_GetThisPage(int fd, int pagenum, char **pagebuf){
    int msg;
    BFreq req;
    PFpage *req_fpage;
    
    if(!PFftab[fd].valid)
        return setErr(PFE_FILENOTOPEN);

    /*Check page exists and is valid*/
    if(pagenum < 0 ||PFftab[fd].hdr.numpages <= pagenum)
        return setErr(PFE_INVALIDPAGE);

    /*Buffer Request*/
    req.fd = fd;
    req.pagenum = pagenum;
    req.unixfd = PFftab[fd].unixfd;
    req.dirty = FALSE;
    
    if(BF_GetBuf(req, &req_fpage) < 0)
        return setErr(PFE_INVALIDPAGE);

    *pagebuf = req_fpage->pagebuf;
    return PFE_OK;
}
int PF_DirtyPage(int fd, int pagenum){
    int msg;
    BFreq req;

    if(!PFftab[fd].valid)
        return setErr(PFE_FILENOTOPEN);

    /*Check page exists and is valid*/
    if(PFftab[fd].hdr.numpages <= pagenum)
        return setErr(PFE_INVALIDPAGE);

    req.fd = fd;
    req.pagenum = pagenum;
    req.unixfd = PFftab[fd].unixfd;
    req.dirty = TRUE;

    if((msg = BF_TouchBuf(req))<0)
        return setErr(PFE_INVALIDPAGE);

    return PFE_OK;
}
int PF_UnpinPage(int fd, int pagenum, int dirty){
    BFreq req;
    int msg;

    if(!PFftab[fd].valid)
        return setErr(PFE_FILENOTOPEN);

    /*Check page exists and is valid*/
    if(PFftab[fd].hdr.numpages <= pagenum)
        return setErr(PFE_INVALIDPAGE);
    
    req.fd = fd;
    req.pagenum = pagenum;
    req.unixfd = PFftab[fd].unixfd;
    req.dirty = dirty;

    if(dirty)
        if((msg=BF_TouchBuf(req))<0)
            return setErr(PFE_INVALIDPAGE);

    if((msg=BF_UnpinBuf(req))<0)
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
        case -100:
            fputs("PFE_UNIX", stderr);
            break;
    }
    fputs("\n",stderr);
}
