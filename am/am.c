#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "../h/minirel.h"
#include "../h/pf.h"
#include "../h/hf.h"
#include "../h/am.h"
#include "aminternal.h"
#include "amHash.h"

/*#define AM_PAGE_NUM 1*/

int AM_PAGE_NUM;
int AMerrno = 1;

AMitab_ele *AMitab;
AMitab_ele *freeAMHead;
int numAMitab_ele = 0;

AMScantab_ele *AMScantab;
AMScantab_ele *freeAMScanHead;
int numAMScan_ele=0;

static int setErr(int err){
    return (AMerrno = err);
}
static RECID recSetErr(int err){
    RECID tmp;
    tmp.pagenum = setErr(err);
    tmp.recnum = -1;
    return tmp;
}

void AM_Init(void){
    int i;
    PF_Init();
    HF_Init();

    /*AM file table*/
    AMitab = (AMitab_ele*)malloc(sizeof(AMitab_ele)*AM_ITAB_SIZE);
    for(i=0; i<AM_ITAB_SIZE-1; i++){
        AMitab[i].next_ele = &AMitab[i+1];
        AMitab[i].valid = FALSE;
    }
    AMitab[AM_ITAB_SIZE-1].next_ele = NULL;
    AMitab[AM_ITAB_SIZE-1].valid = FALSE;

    freeAMHead = &AMitab[0];

    /*AM Scan table*/
    AMScantab = (AMScantab_ele*)malloc(sizeof(AMScantab_ele)*MAXISCANS);
    for(i=0; i<MAXISCANS-1; i++){
        AMScantab[i].next_ele = &AMScantab[i+1];
        AMScantab[i].valid = FALSE;
    }
    AMScantab[MAXISCANS-1].next_ele = NULL;
    AMScantab[MAXISCANS-1].valid = FALSE;

    freeAMScanHead = &AMScantab[0];

    AMhashInit();
}
int AM_CreateIndex(const char *fileName, int indexNo, char attrType,
                   int attrLength, bool_t isUnique){
    int msg, fd;
    char buffer[100];
    int pageNum, rootNum;
    char *pagebuf, *rootbuf;
    AMhdr_str *newAMHdr;
    LeafHdr *newLeafHdr;

    if (attrType == 'c') {
        if (attrLength < 1 || attrLength > 255)
            return setErr(AME_INVALIDATTRLENGTH);
    } else if (attrType == 'i') {
        if (attrLength != 4)
            return setErr(AME_INVALIDATTRLENGTH);
    } else if (attrType == 'f') {
        if (attrLength != 4)
            return setErr(AME_INVALIDATTRLENGTH);
    } else
        return setErr(AME_INVALIDATTRTYPE);

    sprintf(buffer,"%s.%d", fileName, indexNo);

    if((msg=PF_CreateFile(buffer))<0)
        return setErr(AME_PF);

    if((fd=PF_OpenFile(buffer))<0)
        return setErr(AME_PF);

    if((msg = PF_AllocPage(fd, &pageNum, &pagebuf))<0)
        return setErr(AME_PF);
    AM_PAGE_NUM = pageNum;

    if((msg= PF_AllocPage(fd, &rootNum, &rootbuf))<0)
        return setErr(AME_PF);

    newLeafHdr = (LeafHdr*)rootbuf;
    newLeafHdr->hdr.isLeaf = TRUE;
    newLeafHdr->hdr.numKeys = 0;
    newLeafHdr->prevLeaf = NULL_PAGE;
    newLeafHdr->nextLeaf = NULL_PAGE;

    newAMHdr = (AMhdr_str *)pagebuf;
    newAMHdr->attrType = attrType;
    newAMHdr->attrLength = attrLength;
    newAMHdr->isUnique = isUnique;
    newAMHdr->rootPtr = rootNum;
    newAMHdr->maxFanoutInter = (PAGE_SIZE - sizeof(InterHdr) + attrLength) / (sizeof(int) + attrLength);
    newAMHdr->maxFanoutLeaf = (PAGE_SIZE - sizeof(LeafHdr)) / (sizeof(RECID) + attrLength);

    if((msg=PF_UnpinPage(fd, pageNum, TRUE))<0)
        return setErr(AME_PF);
    if((msg=PF_UnpinPage(fd, rootNum, TRUE))<0)
        return setErr(AME_PF);

    if((msg=PF_CloseFile(fd))<0)
        return setErr(AME_PF);

    return AME_OK;
}
int AM_DestroyIndex(const char *fileName, int indexNo){
    int msg;
    char buffer[100];
    sprintf(buffer, "%s.%d", fileName, indexNo);
    if((msg=PF_DestroyFile(buffer))<0)
        return setErr(AME_PF);
    return AME_OK;
}
int AM_OpenIndex(const char *fileName, int indexNo){
    int msg, fd;
    char buffer[100];
    char *pagebuf;
    AMitab_ele *newEle;

    if(numAMitab_ele >= AM_ITAB_SIZE)
        return setErr(AME_FULLTABLE);
    
    sprintf(buffer, "%s.%d", fileName, indexNo);

    if(isInAMHash(buffer))
        return setErr(AME_DUPLICATEOPEN);

    if((fd = PF_OpenFile(buffer))<0)
        return setErr(AME_PF);

    if((msg=PF_GetThisPage(fd, AM_PAGE_NUM, &pagebuf))<0)
        return setErr(AME_PF);
    
    newEle = freeAMHead;
    freeAMHead = freeAMHead->next_ele;

    newEle->valid = TRUE;
    newEle->hdrchanged = FALSE;
    newEle->next_ele = NULL;
    newEle->PFfd = fd;
    newEle->numScan = 0;
    memcpy(&newEle->AMhdr, pagebuf, sizeof(AMhdr_str));
    strcpy(newEle->fileName, buffer);

    numAMitab_ele++;

    if((msg=PF_UnpinPage(fd, AM_PAGE_NUM, FALSE))<0)
        return setErr(AME_PF);

    AMaddToHash(newEle);

    return newEle - AMitab;
}
int AM_CloseIndex(int fileDesc){
    int msg;
    const int PFfd = AMitab[fileDesc].PFfd;
    char *pagebuf;

    if(!AMitab[fileDesc].valid)
        return setErr(AME_FD);

    if (AMitab[fileDesc].numScan > 0)
        return setErr(AME_SCANOPEN);

    if(AMitab[fileDesc].hdrchanged){
        if((msg=PF_GetThisPage(PFfd, AM_PAGE_NUM, &pagebuf))<0)
            return setErr(AME_PF);
        memcpy(pagebuf, &AMitab[fileDesc].AMhdr, sizeof(AMhdr_str));
        if((msg=PF_UnpinPage(PFfd, AM_PAGE_NUM, TRUE))<0)
            return setErr(AME_PF);
    } 

    if((msg=PF_CloseFile(PFfd))<0)
        return setErr(AME_PF);

    AMremoveFromHash(&(AMitab[fileDesc]));

    AMitab[fileDesc].valid = FALSE;
    AMitab[fileDesc].next_ele = freeAMHead;
    freeAMHead = &(AMitab[fileDesc]);

    numAMitab_ele--;

    return AME_OK;
}
int AM_InsertEntry(int fileDesc, char *value, RECID recId){
    int msg;
    const char attrType = AMitab[fileDesc].AMhdr.attrType;
    const int attrLength = AMitab[fileDesc].AMhdr.attrLength;

    if(!AMitab[fileDesc].valid)
        return setErr(AME_PF);

    if((msg= BtreeIns(fileDesc, attrType, attrLength, value, recId))<0)
        return setErr(msg);

    return AME_OK;
}
int AM_DeleteEntry(int fileDesc, char *value, RECID recId){
    int msg;
    const char attrType = AMitab[fileDesc].AMhdr.attrType;
    const int attrLength = AMitab[fileDesc].AMhdr.attrLength;

    if (!AMitab[fileDesc].valid)
        return setErr(AME_PF);

    if((msg = BtreeDelete(fileDesc, AMitab[fileDesc].AMhdr.rootPtr, attrType, attrLength, value, recId))<0)
        return setErr(msg);
    
    return AME_OK;
}
int AM_OpenIndexScan(int fileDesc, int op, char *value){
    AMScantab_ele *newScanEle;

    if(numAMScan_ele >= MAXISCANS)
        return setErr(AME_SCANTABLEFULL);
    if(!AMitab[fileDesc].valid)
        return setErr(AME_FD);
    if(op<1 || op>6)
        return setErr(AME_INVALIDOP);
    
    if(value==NULL)
        op = ALL_OP;
    
    newScanEle = freeAMScanHead;
    freeAMScanHead = freeAMScanHead->next_ele;

    AMitab[fileDesc].numScan++;
    newScanEle->valid = TRUE;
    newScanEle->AMfd = fileDesc;
    if( value != NULL )
        memcpy(newScanEle->value, value, AMitab[fileDesc].AMhdr.attrLength);
    newScanEle->op = op;
    newScanEle->ifInit = FALSE;
    newScanEle->curSearch.pagenum = -1;
    newScanEle->curSearch.index = -1;
    newScanEle->curSearch.lastRetrived.pagenum = -1;
    newScanEle->curSearch.lastRetrived.recnum = -1;

    numAMScan_ele++;

    return newScanEle - AMScantab;
}
RECID AM_FindNextEntry(int scanDesc){
    int msg;
    char *pagebuf;
    LeafHdr *curHdr;
    AMsearchPtr *curSearch;
    char *value, *keyPtr, *childPtr;
    int fileDesc, attrLength, op;
    char attrType;
    AMScantab_ele* curScanEle;
    if(!AMScantab[scanDesc].valid)
        return recSetErr(AME_INVALIDSCANDESC);

    curScanEle = &(AMScantab[scanDesc]);
    curSearch = &(curScanEle->curSearch);

    value = curScanEle->value;
    op = curScanEle->op;
    fileDesc = curScanEle->AMfd;
    attrLength = AMitab[fileDesc].AMhdr.attrLength;
    attrType = AMitab[fileDesc].AMhdr.attrType;

    if(!curScanEle->ifInit){/*go down btree*/
        switch (op){
            case EQ_OP:
            case GT_OP:
            case GE_OP:
                if ((msg = BtreeSearchOp(op, fileDesc, AMitab[fileDesc].AMhdr.rootPtr, value, &(curScanEle->curSearch))) < 0)
                    return recSetErr(msg);
                break;
            case LT_OP:
            case LE_OP:
            case NE_OP:
            case ALL_OP:
                if ((msg = BtreeLeftMost(fileDesc, AMitab[fileDesc].AMhdr.rootPtr, &(curScanEle->curSearch))) < 0)
                    return recSetErr(msg);
                break;
        }
        curScanEle->ifInit = TRUE;
        curScanEle->curSearch.lastRetrived.pagenum = -1;
    }
    
    if ((msg = PF_GetThisPage(AMitab[AMScantab[scanDesc].AMfd].PFfd, curSearch->pagenum, &pagebuf)) < 0)
        return recSetErr(AME_PF);
    if ((msg = PF_UnpinPage(AMitab[fileDesc].PFfd, curSearch->pagenum, FALSE)) < 0)
        return recSetErr(AME_PF);

    curHdr = (LeafHdr *)pagebuf;
    keyPtr = keyLoc((char*)curHdr, fileDesc, TRUE);
    childPtr = childLoc((char *)curHdr, fileDesc, attrLength, TRUE);

    if(curHdr->hdr.numKeys>0 
        &&  curSearch->index >=0 && curSearch->index < curHdr->hdr.numKeys   
        && ifSatisOp(op, value, keyPtr+attrLength*curSearch->index, attrType, attrLength) 
        && !ifRetrived(curSearch, ((RECID*)childPtr)[curSearch->index]) ){
        
        curSearch->lastRetrived = ((RECID*)childPtr)[curSearch->index];
        return curSearch->lastRetrived;
    } else {
        if((msg=findNext(op, fileDesc, curSearch->pagenum, value, curSearch))<0)
            return recSetErr(msg);

        if ((msg = PF_GetThisPage(AMitab[AMScantab[scanDesc].AMfd].PFfd, curSearch->pagenum, &pagebuf)) < 0)
            return recSetErr(AME_PF);
        if ((msg = PF_UnpinPage(AMitab[fileDesc].PFfd, curSearch->pagenum, FALSE)) < 0)
            return recSetErr(AME_PF);

        curHdr = (LeafHdr *)pagebuf;
        keyPtr = keyLoc((char*)curHdr, fileDesc, TRUE);
        childPtr = childLoc((char *)curHdr, fileDesc, attrLength, TRUE);
        curSearch->lastRetrived = ((RECID *)childPtr)[curSearch->index];
        return curSearch->lastRetrived;
    }
}
int AM_CloseIndexScan(int scanDesc){
    AMScantab_ele *curScanEle;

    if(!AMScantab[scanDesc].valid)
        return setErr(AME_INVALIDSCANDESC);
    
    curScanEle = &(AMScantab[scanDesc]);

    curScanEle->valid = FALSE;
    curScanEle->next_ele = freeAMScanHead;
    freeAMScanHead = curScanEle;

    AMitab[curScanEle->AMfd].numScan--;

    numAMScan_ele--;
    return AME_OK;
}
void AM_PrintError(const char *errString){
    fputs(errString, stderr);
    fputs(": ", stderr);
    switch(AMerrno){
        case 1: /*not initialized*/
            break;
        case 0: /*AME_OK*/
            break;
        case -1:
            fputs("AME_PF\n", stderr);
            PF_PrintError("PF_ERR");
            return;
            break;
        case -2:
            fputs("AME_EOF", stderr);
            break;
        case -3:
            fputs("AME_FULLTABLE", stderr);
            break;
        case -4:
            fputs("AME_INVALIDATTRTYPE", stderr);
            break;
        case -5:
            fputs("AME_FD", stderr);
            break;
        case -6:
            fputs("AME_INVALIDOP", stderr);
            break;
        case -7:
            fputs("AME_INVALIDPARA", stderr);
            break;
        case -8:
            fputs("AME_DUPLICATEOPEN", stderr);
            break;
        case -9:
            fputs("AME_SCANTABLEFULL", stderr);
            break;
        case -10:
            fputs("AME_INVALIDSCANDESC", stderr);
            break;
        case -11:
            fputs("AME_UNABLETOSCAN", stderr);
            break;
        case -12:
            fputs("AME_RECNOTFOUND", stderr);
            break;
        case -13:
            fputs("AME_DUPLICATERECID", stderr);
            break;
        case -14:
            fputs("AME_SCANOPEN", stderr);
            break;
        case -15:
            fputs("AME_INVALIDATTRLENGTH", stderr);
            break;
        case -16:
            fputs("AME_UNIX", stderr);
            break;
        case -17:
            fputs("AME_ROOTNULL", stderr);
            break;
        case -18:
            fputs("AME_TREETOODEEP", stderr);
            break;
        case -19:
            fputs("AME_INVALIDATTR", stderr);
            break;
        case -20:
            fputs("AME_NOMEM", stderr);
            break;
        case -21:
            fputs("AME_INVALIDRECORD", stderr);
            break;
        case -22:
            fputs("AME_TOOMANYRECSPERKEY", stderr);
            break;
        case -23:
            fputs("AME_KEYNOTFOUND", stderr);
            break;
        case -24:
            fputs("AME_DUPLICATEKEY", stderr);
            break;
    }
    fputs("\n", stderr);
}