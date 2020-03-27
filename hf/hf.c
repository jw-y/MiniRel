#include <math.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "../h/minirel.h"
#include "../h/pf.h"
#include "../h/hf.h"
#include "hfinternal.h"
#include "hfHash.h"
/*#include "hfScanHash.h"*/

/*#define HF_PAGE_NUM 1*/

int HF_PAGE_NUM;

int HFerrno = 1;

HFftab_ele *HFftab;
HFftab_ele *freeHFHead;
int numHFftab_ele = 0;

HFScantab_ele *HFScantab;
HFScantab_ele *freeHFScanHead;
int numHFScan_ele = 0;

static int setErr(int err){
    return (HFerrno = err);
}
static RECID recSetErr(int err){
    RECID tmp;
    tmp.pagenum = setErr(err);
    tmp.recnum = -1;
    return tmp;
}

void HF_Init(void){
    int i;
    PF_Init();

    /*HF file table*/
    HFftab = (HFftab_ele*)malloc(sizeof(HFftab_ele)*HF_FTAB_SIZE);

    for(i=0; i<HF_FTAB_SIZE-1;i++){
        HFftab[i].next_ele = &HFftab[i+1];
        HFftab[i].valid = FALSE;
    }
    HFftab[HF_FTAB_SIZE-1].next_ele = NULL;
    HFftab[HF_FTAB_SIZE-1].valid = FALSE;

    freeHFHead = &HFftab[0];

    /*HF Scan table*/
    HFScantab = (HFScantab_ele*)malloc(sizeof(HFScantab_ele)*MAXSCANS);

    for(i=0; i<MAXSCANS-1; i++){
        HFScantab[i].next_ele = &HFScantab[i+1];
        HFScantab[i].valid = FALSE;
    }
    HFScantab[MAXSCANS-1].next_ele = NULL;
    HFScantab[MAXSCANS-1].valid = FALSE;

    freeHFScanHead = &HFScantab[0];

    HFhashInit();
    /*HFScanHashInit();*/
}
int HF_CreateFile(const char *fileName, int RecSize){
    int msg, fd;
    int pageNum, numRec;
    char *pagebuf;
    HFhdr_str *newHdr;

    if((msg = PF_CreateFile(fileName))<0)
        return setErr(HFE_PF);
    
    if((fd = PF_OpenFile(fileName))<0)
        return setErr(HFE_PF);
    
    if((msg = PF_AllocPage(fd, &pageNum, &pagebuf))<0)
        return setErr(HFE_PF);
    
    HF_PAGE_NUM = pageNum;

    numRec = (PAGE_SIZE-sizeof(PageHdr))/RecSize;
    while ( ((PAGE_SIZE - sizeof(PageHdr)) - RecSize*numRec) < ceil((double)numRec/BYTE_SIZE))
        numRec--;

    newHdr = (HFhdr_str*)pagebuf;
    newHdr->RecSize = RecSize;
    newHdr->numRecPerPage = numRec;
    newHdr->numDataPages = 0;
    newHdr->totalNumRec = 0;
    newHdr->freePagePtr = NULLPAGE;
    newHdr->fullPagePtr = NULLPAGE;

    if((msg = PF_UnpinPage(fd, pageNum, TRUE))<0)
        return setErr(HFE_PF);

    if((msg = PF_CloseFile(fd))<0)
        return setErr(HFE_PF);
    
    return HFE_OK;
}
int HF_DestroyFile(const char *fileName){
    int msg;
    if((msg = PF_DestroyFile(fileName))<0)
        return setErr(HFE_PF);
    return PFE_OK;
}
int HF_OpenFile(const char *fileName){
    int msg, fd;
    char *pagebuf;
    HFftab_ele *newEle;

    if(numHFftab_ele >= HF_FTAB_SIZE)
        return setErr(HFE_FTABFULL);
    
    if(isInHFHash(fileName))
        return setErr(HFE_FILEOPEN);
    
    if((fd=PF_OpenFile(fileName))<0)
        return setErr(HFE_PF);

    if((msg=PF_GetThisPage(fd, HF_PAGE_NUM, &pagebuf))<0)
        return setErr(HFE_PF);

    newEle = freeHFHead;
    freeHFHead = freeHFHead->next_ele;

    newEle->valid = TRUE;
    newEle->hdrchanged = FALSE;
    newEle->next_ele = NULL;
    newEle->PFfd = fd;
    memcpy(&(newEle->HFhdr), pagebuf, sizeof(HFhdr_str));
    /*newEle->scanHead = NULL;*/
    newEle->numScan = 0;
    strcpy(newEle->fileName,fileName);

    numHFftab_ele++;

    if((msg=PF_UnpinPage(fd, HF_PAGE_NUM, FALSE))<0)
        return setErr(HFE_PF);

    /*add to hash*/
    HFaddToHash(newEle);

    return newEle - HFftab;
}
int HF_CloseFile(int fileDesc){
    int msg;
    int PFfd;
    char *pagebuf;
    
    if(!HFftab[fileDesc].valid)
        return setErr(HFE_FD);

    PFfd = HFftab[fileDesc].PFfd;

    /*check if active scan*/
    /*not implemented yet*/
    if (HFftab[fileDesc].numScan > 0)
        return setErr(HFE_SCANOPEN);

    if (HFftab[fileDesc].hdrchanged){
        if ((msg = PF_GetThisPage(PFfd, HF_PAGE_NUM, &pagebuf)) < 0)
            return setErr(HFE_PF);
        memcpy(pagebuf, &(HFftab[fileDesc].HFhdr), sizeof(HFhdr_str));
        if ((msg = PF_UnpinPage(PFfd, HF_PAGE_NUM, TRUE)) < 0)
            return setErr(HFE_PF);
    }

    if((msg=PF_CloseFile(PFfd))<0)
        return setErr(HFE_PF);

    /*remove from hash*/
    HFremoveFromHash(&HFftab[fileDesc]);

    HFftab[fileDesc].valid = FALSE;
    HFftab[fileDesc].next_ele = freeHFHead;
    freeHFHead = &(HFftab[fileDesc]);

    numHFftab_ele--;

    return HFE_OK;
}
RECID HF_InsertRec(int fileDesc, char *record){
    int msg;
    RECID recID;
    int curPage;
    char *pagebuf;
    PageHdr *pageHdr;
    int emptySlotNum;
    int bitmapSize;
    /*bool_t newPage = FALSE;*/

    char *prevbuf, *nextbuf;
    PageHdr *prevHdr, *nextHdr;

    if (!HFftab[fileDesc].valid) 
        return recSetErr(HFE_FD);
    
    curPage = HFftab[fileDesc].HFhdr.freePagePtr;
    bitmapSize = (int)ceil((double)HFftab[fileDesc].HFhdr.numRecPerPage / BYTE_SIZE);

    if(curPage == NULLPAGE ){ /* no freepage*/
        if((msg=PF_AllocPage(HFftab[fileDesc].PFfd, &curPage, &pagebuf))<0)
            return recSetErr(HFE_PF);
        pageHdr = (PageHdr*)pagebuf;
        pageHdr->numRec = 0;
        pageHdr->prevPage = NULLPAGE;
        pageHdr->nextPage = NULLPAGE;
        memset(pagebuf+PAGE_SIZE - bitmapSize, 0x00, bitmapSize);

        HFftab[fileDesc].HFhdr.numDataPages++;
        /*newPage = TRUE;*/

        HFftab[fileDesc].HFhdr.freePagePtr = curPage;
    }
    else {  /*freepage found*/
        if((msg=PF_GetThisPage(HFftab[fileDesc].PFfd, curPage, &pagebuf))<0)
            return recSetErr(HFE_PF);
        pageHdr = (PageHdr*)pagebuf;
    }

    emptySlotNum = HFgetEmptySlot(pagebuf, HFftab[fileDesc].HFhdr.numRecPerPage);
    
    assert(emptySlotNum >= 0);

    /*write record to buf*/
    memcpy(HFrecLoc(pagebuf, emptySlotNum, HFftab[fileDesc].HFhdr.RecSize), record, HFftab[fileDesc].HFhdr.RecSize);

    /*write pageheader*/
    pageHdr->numRec++;
    /*check if page full*/
    if (pageHdr->numRec >= HFftab[fileDesc].HFhdr.numRecPerPage) {
        if(pageHdr->prevPage != NULLPAGE){
            if ((msg = PF_GetThisPage(HFftab[fileDesc].PFfd, pageHdr->prevPage, &prevbuf)) < 0)
                return recSetErr(HFE_PF);
            prevHdr = (PageHdr *)prevbuf;
            prevHdr->nextPage = pageHdr->nextPage;
            if ((msg = PF_UnpinPage(HFftab[fileDesc].PFfd, pageHdr->prevPage, TRUE)) < 0)
                return recSetErr(HFE_PF);
        }
        if(pageHdr->nextPage != NULLPAGE){
            if ((msg = PF_GetThisPage(HFftab[fileDesc].PFfd, pageHdr->nextPage, &nextbuf)) < 0)
                return recSetErr(HFE_PF);
            nextHdr = (PageHdr*)nextbuf;
            nextHdr->prevPage = pageHdr->prevPage;
            if((msg=PF_UnpinPage(HFftab[fileDesc].PFfd, pageHdr->nextPage, TRUE))<0)
                return recSetErr(HFE_PF);
        }

        if(HFftab[fileDesc].HFhdr.freePagePtr == curPage){
            HFftab[fileDesc].HFhdr.freePagePtr = pageHdr->nextPage;
        }

        if (HFftab[fileDesc].HFhdr.fullPagePtr != NULLPAGE) {
            if ((msg = PF_GetThisPage(HFftab[fileDesc].PFfd, HFftab[fileDesc].HFhdr.fullPagePtr, &nextbuf)) < 0)
                return recSetErr(HFE_PF);
            nextHdr = (PageHdr *)nextbuf;
            nextHdr->prevPage = curPage;
            if ((msg = PF_UnpinPage(HFftab[fileDesc].PFfd, HFftab[fileDesc].HFhdr.fullPagePtr, TRUE)) < 0)
                return recSetErr(HFE_PF);
        }

        pageHdr->prevPage = NULLPAGE;
        pageHdr->nextPage = HFftab[fileDesc].HFhdr.fullPagePtr;
        HFftab[fileDesc].HFhdr.fullPagePtr = curPage;
    }
    /*
    if(newPage){
        if(HFftab[fileDesc].HFhdr.freePagePtr != NULLPAGE){
            if((msg = PF_GetThisPage(HFftab[fileDesc].PFfd, HFftab[fileDesc].HFhdr.freePagePtr, &nextbuf))<0)
                return recSetErr(HFE_PF);
            nextHdr = (PageHdr *)nextbuf;
            nextHdr->prevPage = curPage;
            if ((msg = PF_UnpinPage(HFftab[fileDesc].PFfd, HFftab[fileDesc].HFhdr.freePagePtr, TRUE)) < 0)
                return recSetErr(HFE_PF);
        }

        pageHdr->nextPage = HFftab[fileDesc].HFhdr.freePagePtr;
        HFftab[fileDesc].HFhdr.freePagePtr = curPage;
    }*/

    /*set bitmap*/
    setBitmap(pagebuf, emptySlotNum);

    HFftab[fileDesc].HFhdr.totalNumRec++;
    HFftab[fileDesc].hdrchanged = TRUE;

    if((msg=PF_UnpinPage(HFftab[fileDesc].PFfd, curPage, TRUE))<0)
        return recSetErr(HFE_PF);

    recID.pagenum = curPage;
    recID.recnum = emptySlotNum;
    return recID;
}
int HF_DeleteRec(int fileDesc, RECID recId){
    int msg;
    char *pagebuf;
    PageHdr *pageHdr;

    char *prevbuf, *nextbuf;
    PageHdr *prevHdr, *nextHdr;

    if(!HFftab[fileDesc].valid)
        return setErr(HFE_FD);
    
    if((msg=PF_GetThisPage(HFftab[fileDesc].PFfd, recId.pagenum, &pagebuf))<0)
        return setErr(HFE_PF);
    
    /*check if rec valid*/
    if(!HF_ValidRecId(fileDesc, recId))
        return setErr(HFE_INVALIDRECORD);

    pageHdr = (PageHdr *)pagebuf;

    if(!ifRecPresent(pagebuf, recId.recnum, HFftab[fileDesc].HFhdr.numRecPerPage))
        return setErr(HFE_INVALIDRECORD);

    /*if in fullpage move to freepage*/
    if(pageHdr->numRec >=HFftab[fileDesc].HFhdr.numRecPerPage){
        if (pageHdr->prevPage != NULLPAGE){
            if ((msg = PF_GetThisPage(HFftab[fileDesc].PFfd, pageHdr->prevPage, &prevbuf)) < 0)
                return setErr(HFE_PF);
            prevHdr = (PageHdr *)prevbuf;
            prevHdr->nextPage = pageHdr->nextPage;
            if ((msg = PF_UnpinPage(HFftab[fileDesc].PFfd, pageHdr->prevPage, TRUE)) < 0)
                return setErr(HFE_PF);
        }
        if (pageHdr->nextPage != NULLPAGE)
        {
            if ((msg = PF_GetThisPage(HFftab[fileDesc].PFfd, pageHdr->nextPage, &nextbuf)) < 0)
                return setErr(HFE_PF);
            nextHdr = (PageHdr *)nextbuf;
            nextHdr->prevPage = pageHdr->prevPage;
            if ((msg = PF_UnpinPage(HFftab[fileDesc].PFfd, pageHdr->nextPage, TRUE)) < 0)
                return setErr(HFE_PF);
        }

        if (HFftab[fileDesc].HFhdr.fullPagePtr == recId.pagenum)
            HFftab[fileDesc].HFhdr.fullPagePtr = pageHdr->nextPage;
        
        if (HFftab[fileDesc].HFhdr.freePagePtr != NULLPAGE){
            if ((msg = PF_GetThisPage(HFftab[fileDesc].PFfd, HFftab[fileDesc].HFhdr.freePagePtr, &nextbuf)) < 0)
                return setErr(HFE_PF);
            nextHdr = (PageHdr *)nextbuf;
            nextHdr->prevPage = recId.pagenum;
            if ((msg = PF_UnpinPage(HFftab[fileDesc].PFfd, HFftab[fileDesc].HFhdr.freePagePtr, TRUE)) < 0)
                return setErr(HFE_PF);
        }

        pageHdr->prevPage = NULLPAGE;
        pageHdr->nextPage = HFftab[fileDesc].HFhdr.freePagePtr;
        HFftab[fileDesc].HFhdr.freePagePtr = recId.pagenum;
    }

    /*read page header*/
    pageHdr->numRec--;

    /*unset bitmap*/
    unsetBitmap(pagebuf, recId.recnum);

    if((msg=PF_UnpinPage(HFftab[fileDesc].PFfd, recId.pagenum, TRUE))<0)
        return setErr(HFE_PF);

    return HFE_OK;
}

RECID HF_GetFirstRec(int fileDesc, char *record){
    int msg;
    char *pagebuf;
    RECID recID = {-1, -1};
    HFftab_ele *curEle;
    PageHdr *curHdr;
    int curPagenum;

    /*Check to see if file exists*/
    if (!HFftab[fileDesc].valid)
        return recSetErr(HFE_FD);
    /*Check to see if file has pages and records*/
    if ((HFftab[fileDesc].HFhdr.numDataPages == 0) || (HFftab[fileDesc].HFhdr.totalNumRec == 0))
        return recSetErr(HFE_EOF);

    curEle = &(HFftab[fileDesc]);
    curPagenum = HF_PAGE_NUM+1;
    while(curPagenum <=HF_PAGE_NUM+curEle->HFhdr.numDataPages){
        if ((msg = PF_GetThisPage(curEle->PFfd, curPagenum, &pagebuf)) < 0)
            return recSetErr(HFE_PF);
        if ((msg = PF_UnpinPage(curEle->PFfd, curPagenum, FALSE)) < 0)
            return recSetErr(HFE_PF);
        curHdr = (PageHdr *)pagebuf;
        if(curHdr->numRec != 0 )
            break;
        curPagenum++;
    }
    recID.pagenum = curPagenum;
    recID.recnum = HFnextFullSlot(pagebuf, -1, curEle->HFhdr.numRecPerPage);
    memcpy(record, HFrecLoc(pagebuf, recID.recnum, curEle->HFhdr.RecSize), curEle->HFhdr.RecSize);
    
    return recID;
}

RECID HF_GetNextRec(int fileDesc, RECID recId, char *record){
    int msg;
    HFftab_ele *curEle;
    char *pagebuf;
    int nextSlot, curPagenum;
    RECID retID;
    PageHdr *curHdr;

    if(!HFftab[fileDesc].valid)
        return recSetErr(HFE_FD);

    curEle = &(HFftab[fileDesc]);

    if (!HF_ValidRecId(fileDesc, recId))
        return recSetErr(HFE_INVALIDRECORD);

    curPagenum = recId.pagenum;

    if((msg=PF_GetThisPage(curEle->PFfd, curPagenum, &pagebuf))<0)
        return recSetErr(HFE_PF);
    if ((msg = PF_UnpinPage(curEle->PFfd, curPagenum, FALSE)) < 0)
        return recSetErr(HFE_PF);
    curHdr = (PageHdr*)pagebuf;

    if(curHdr->numRec==0 || 
        (nextSlot=HFnextFullSlot(pagebuf, recId.recnum, curEle->HFhdr.numRecPerPage))<0){

        while(1){
            curPagenum++;

            if(curPagenum > HF_PAGE_NUM + curEle->HFhdr.numDataPages)
                return recSetErr(EOF);

            if((msg=PF_GetThisPage(curEle->PFfd, curPagenum, &pagebuf))<0)
                return recSetErr(HFE_PF);
            if ((msg = PF_UnpinPage(curEle->PFfd, curPagenum, FALSE)) < 0)
                return recSetErr(HFE_PF);
            curHdr = (PageHdr*)pagebuf;

            if(curHdr->numRec!=0)
                break;
        }
        retID.pagenum = curPagenum;
        retID.recnum = HFnextFullSlot(pagebuf, -1, curEle->HFhdr.numRecPerPage);
    } else {
        retID.pagenum = curPagenum;
        retID.recnum = nextSlot;
    }

    memcpy(record, HFrecLoc(pagebuf, retID.recnum, curEle->HFhdr.RecSize), curEle->HFhdr.RecSize);

    return retID;
}

int HF_GetThisRec(int fileDesc, RECID recId, char *record){
    int msg;
    HFftab_ele *curEle = &(HFftab[fileDesc]);
    char *pagebuf;
    PageHdr *curHdr;

    if(!HF_ValidRecId(fileDesc, recId))
        return setErr(HFE_INVALIDRECORD);

    if((msg=PF_GetThisPage(curEle->PFfd, recId.pagenum, &pagebuf))<0)
        return setErr(HFE_PF);

    curHdr = (PageHdr *)pagebuf;

    /*if valid*/
    if (!ifRecPresent((char*)curHdr, recId.recnum, curEle->HFhdr.numRecPerPage))
        return setErr(HFE_EOF);

    memcpy(record, HFrecLoc((char*)curHdr, recId.recnum, curEle->HFhdr.RecSize), curEle->HFhdr.RecSize);

    if((msg=PF_UnpinPage(curEle->PFfd, recId.pagenum, FALSE))<0)
        return setErr(HFE_PF);
    return HFE_OK;
}
int HF_OpenFileScan(int fileDesc, char attrType, int attrLength,
                    int attrOffset, int op, const char *value){
    HFScantab_ele *newScanEle;

    if(numHFScan_ele >= MAXSCANS)
        return setErr(HFE_STABFULL);
    
    /*if(isInHFScanHash(fileDesc))
        return setErr(HFE_SCANOPEN);*/
    
    if(!HFftab[fileDesc].valid)
        return setErr(HFE_FILENOTOPEN);

    if (HFftab[fileDesc].HFhdr.RecSize <= attrOffset || attrOffset < 0)
        return setErr(HFE_ATTROFFSET);

    if(op<1 || op>6)
        return setErr(HFE_OPERATOR);

    if(attrType=='c'){
        if(attrLength < 1 || attrLength > 255)
            return setErr(HFE_ATTRLENGTH);
    } else if(attrType=='i'){
        if(attrLength != 4)
            return setErr(HFE_ATTRLENGTH);
    } else if(attrType=='f'){
        if(attrLength != 4)
            return setErr(HFE_ATTRLENGTH);
    } else
        return setErr(HFE_ATTRTYPE);

    if(value==NULL)
        op = ALL_OP;

    newScanEle = freeHFScanHead;
    freeHFScanHead = freeHFScanHead->next_ele; 

    newScanEle->valid = TRUE;
    newScanEle->HFfd = fileDesc;
    newScanEle->attrType = attrType;
    newScanEle->attrLength = attrLength;
    newScanEle->attrOffset = attrOffset;
    newScanEle->op = op;
    if(value!=NULL)
        memcpy(newScanEle->value, value, attrLength);

    newScanEle->lastRet.pagenum = HF_PAGE_NUM+1;
    newScanEle->lastRet.recnum = -1;

    HFftab[fileDesc].numScan++;

    numHFScan_ele++;

    return newScanEle - HFScantab;
}

RECID HF_FindNextRec(int scanDesc, char *record){
    int msg;
    HFScantab_ele *curScanEle;
    int HFfd;

    if (!HFScantab[scanDesc].valid) 
        return recSetErr(HFE_SD);

    HFfd = HFScantab[scanDesc].HFfd;

    curScanEle = &HFScantab[scanDesc];

    if(curScanEle->lastRet.pagenum > HF_PAGE_NUM + HFftab[curScanEle->HFfd].HFhdr.numDataPages)
        return recSetErr(HFE_EOF);

    if((msg=HFfindNextRecInner(HFfd, &(curScanEle->lastRet), curScanEle, record))<0)
        return recSetErr(msg);

    return curScanEle->lastRet;
}

int HF_CloseFileScan(int scanDesc){
    HFScantab_ele *curScanEle = &(HFScantab[scanDesc]);

    if(!curScanEle->valid)
        return setErr(HFE_SD);

    /*HFScanRemoveFromHash(curScanEle);*/
    
    curScanEle->valid = FALSE;
    curScanEle->next_ele = freeHFScanHead;
    freeHFScanHead = curScanEle;

    HFftab[curScanEle->HFfd].numScan--;
    numHFScan_ele--;
    return HFE_OK;
}
bool_t HF_ValidRecId(int fileDesc, RECID recid){
    if(!HFftab[fileDesc].valid)
        return FALSE;
    if(recid.pagenum <=HF_PAGE_NUM || (recid.pagenum > HF_PAGE_NUM+HFftab[fileDesc].HFhdr.numDataPages) )
        return FALSE;
    if(recid.recnum<0 || (recid.recnum >= HFftab[fileDesc].HFhdr.numRecPerPage))
        return FALSE;
    return TRUE;
}
void HF_PrintError(const char *errString){
    fputs(errString, stderr);
    fputs(": ", stderr);
    switch(HFerrno){
        case 1: /* not initialized*/
            break;
        case 0: /*HFE_OK*/
            break;
        case -1:
            fputs("HFE_PF\n", stderr);
            PF_PrintError("PF_ERR");
            return;
            break;
        case -2:
            fputs("HFE_FTABFULL", stderr);
            break;
        case -3:
            fputs("HFE_STABFULL", stderr);
            break;
        case -4:
            fputs("HFE_FD", stderr);
            break;
        case -5:
            fputs("HFE_SD", stderr);
            break;
        case -6:
            fputs("HFE_INVALIDRECORD", stderr);
            break;
        case -7:
            fputs("HFE_EOF", stderr);
            break;
        case -8:
            fputs("HFE_RECSIZE", stderr);
            break;
        case -9:
            fputs("HFE_FILEOPEN", stderr);
            break;
        case -10:
            fputs("HFE_SCANOPEN", stderr);
            break;
        case -11:
            fputs("HFE_FILENOTOPEN", stderr);
            break;
        case -12:
            fputs("HFE_SCANNOTOPEN", stderr);
            break;
        case -13:
            fputs("HFE_ATTRTYPE", stderr);
            break;
        case -14:
            fputs("HFE_ATTRLENGTH", stderr);
            break;
        case -15:
            fputs("HFE_ATTROFFSET", stderr);
            break;
        case -16:
            fputs("HFE_OPERATOR", stderr);
            break;
        case -17:
            fputs("HFE_FILE", stderr);
            break;
        case -18:
            fputs("HFE_INTERNAL", stderr);
            break;
        case -19:
            fputs("HFE_PAGE", stderr);
            break;
        case -20:
            fputs("HFE_INVALIDSTATS", stderr);
            break;
    }
    fputs("\n", stderr);
}

