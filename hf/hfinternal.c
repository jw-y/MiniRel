#include <string.h>
#include <stdio.h>

#include "hfinternal.h"
#include "../h/pf.h"

int HFcmp(char *value1, char *value2, char attrType, int attrLength)
{
    /* <0 if value 1 is smaller
    0 if same
    >0 if value 2 is bigger*/
    if (attrType == 'c')
        /*return memcmp(value1, value2, attrLength);*/
        return strncmp(value1, value2, attrLength);
    else if (attrType == 'i')
        return *((int *)value1) - *((int *)value2);
    else /*f*/{
        if(*((float *)value1) < *((float *)value2) )
            return -1;
        if(*((float *)value1) > *((float *)value2))
            return 1;
        return 0;
    }
}

int HFgetEmptySlot(char *pagebuf, int numRecPerPage){
    int slotNum = 0;
    char *bitmapPtr = pagebuf+PAGE_SIZE-1;
    int j;

    for(slotNum=0; slotNum<numRecPerPage; slotNum++){
        j = slotNum % BYTE_SIZE;
        if((*(bitmapPtr-slotNum/BYTE_SIZE) & (1 << j)) == 0)
            return slotNum;
    }
    return -1;
}
bool_t ifRecPresent(char *pagebuf, const int index, const int numRecPerPage){
    char *bitmapPtr = pagebuf + PAGE_SIZE - 1;
    if(index < 0 ||index >= numRecPerPage)
        return FALSE;
    if (*(bitmapPtr - index / BYTE_SIZE) & (1 << (index % BYTE_SIZE))) 
        return TRUE;
    return FALSE;
}
int HFnextFullSlot(char *pagebuf,const int index,const int numRecPerPage){
    int slotNum = index+1;
    char *bitmapPtr = pagebuf+PAGE_SIZE-1;
    int j;
    for(; slotNum<numRecPerPage; slotNum++){
        j = slotNum %BYTE_SIZE;
        if (*(bitmapPtr - slotNum / BYTE_SIZE) & (1 << j))
            return slotNum;
    }
    return -1;
}
int incInd(const int index, const bool_t isFull, char *pagebuf, const int numRecPerPage){
    if(isFull){
        return index+1;
    } else {
        return HFnextFullSlot(pagebuf, index, numRecPerPage);
    }
}

bool_t isSatisOp(int op, char *value1, char *value2, char attrType, int attrLength){
    switch(op){
        case EQ_OP: /*EQ*/
            if(HFcmp(value1, value2, attrType, attrLength)==0)
                return TRUE;
            return FALSE;
            break;
        case LT_OP: /*LT*/
            if (HFcmp(value1, value2, attrType, attrLength) > 0)
                return TRUE;
            return FALSE;
            break;
        case GT_OP: /*GT*/
            if (HFcmp(value1, value2, attrType, attrLength) < 0)
                return TRUE;
            return FALSE;
            break;
        case LE_OP: /*LE*/
            if (HFcmp(value1, value2, attrType, attrLength) >= 0)
                return TRUE;
            return FALSE;
            break;
        case GE_OP: /*GE*/
            if (HFcmp(value1, value2, attrType, attrLength) <= 0)
                return TRUE;
            return FALSE;
            break;
        case NE_OP: /*NE*/
            if (HFcmp(value1, value2, attrType, attrLength) != 0)
                return TRUE;
            return FALSE;
            break;
        case ALL_OP: 
            return TRUE;
            break;
    }
    return FALSE;
}
char* HFrecLoc(char* pagebuf,const int index, const int RecSize){
    return pagebuf+sizeof(PageHdr)+index*RecSize;
}

void setBitmap(char *pagebuf, const int index){
    *(pagebuf + PAGE_SIZE - 1 - (index / BYTE_SIZE)) |= (1 << (index % BYTE_SIZE));
}
void unsetBitmap(char *pagebuf, const int index){
    *(pagebuf+PAGE_SIZE-1-(index/BYTE_SIZE)) ^= (1 << (index%BYTE_SIZE));
}


int HFfindNextRecInner(const int HFfd, RECID *lastRet,
            HFScantab_ele* curScanEle, char* record){
    
    int msg;
    char *pagebuf;
    PageHdr *curHdr;
    HFftab_ele *curtabEle = &(HFftab[HFfd]);
    bool_t isFull;

    if((msg=PF_GetThisPage(curtabEle->PFfd, lastRet->pagenum, &pagebuf))<0)
        return HFE_PF;
    if ((msg = PF_UnpinPage(curtabEle->PFfd, lastRet->pagenum, FALSE)) < 0)
        return HFE_PF;

    curHdr = (PageHdr*) pagebuf;

    if(curHdr->numRec==0){
        lastRet->pagenum++;
        if(lastRet->pagenum > HF_PAGE_NUM+HFftab[HFfd].HFhdr.numDataPages)
            return HFE_EOF;
        lastRet->recnum = -1;
        return HFfindNextRecInner(HFfd, lastRet, curScanEle, record);
    }

    if(curHdr->numRec >= HFftab[HFfd].HFhdr.numRecPerPage)
        isFull = TRUE;
    else
        isFull = FALSE;
    
    lastRet->recnum = incInd(lastRet->recnum, isFull, pagebuf, HFftab[HFfd].HFhdr.numRecPerPage);

    while((lastRet->recnum >=0  && lastRet->recnum<curtabEle->HFhdr.numRecPerPage) &&
         !isSatisOp(curScanEle->op, curScanEle->value, HFrecLoc(pagebuf, lastRet->recnum, curtabEle->HFhdr.RecSize)+curScanEle->attrOffset, 
                             curScanEle->attrType, curScanEle->attrLength)){
        lastRet->recnum = incInd(lastRet->recnum, isFull, pagebuf, HFftab[HFfd].HFhdr.numRecPerPage);
    }

    if ((lastRet->recnum >=0  && lastRet->recnum<curtabEle->HFhdr.numRecPerPage) &&
        isSatisOp(curScanEle->op, curScanEle->value, HFrecLoc(pagebuf, lastRet->recnum, curtabEle->HFhdr.RecSize) + curScanEle->attrOffset, 
                            curScanEle->attrType, curScanEle->attrLength)){
        memcpy(record, HFrecLoc(pagebuf, lastRet->recnum, curtabEle->HFhdr.RecSize), curtabEle->HFhdr.RecSize);
        return HFE_OK;
    } else{
        lastRet->pagenum++;
        if(lastRet->pagenum > HF_PAGE_NUM+HFftab[HFfd].HFhdr.numDataPages)
            return HFE_EOF;
        lastRet->recnum = -1;
        return HFfindNextRecInner(HFfd, lastRet, curScanEle, record);
    }
}