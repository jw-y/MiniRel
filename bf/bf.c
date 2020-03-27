#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#include "../h/minirel.h"
#include "../h/bf.h"

#include "bfinternal.h"

int BFerrno = 1;

BFpage *LRUhead = NULL;
BFpage BFentry[BF_MAX_BUFS];
BFpage *freeListHead;
int numLRU = 0;

BFhash_entry *hashT[BF_HASH_TBL_SIZE] = {NULL};
BFhash_entry *freeHashHead = NULL;
BFhash_entry freeHash[BF_MAX_BUFS];

static int setErr(int err){
    return (BFerrno = err);
}

void BF_Init(void){
    int i;

    for(i=0; i<BF_MAX_BUFS-1; i++)
        BFentry[i].nextpage= &BFentry[i+1];
    BFentry[BF_MAX_BUFS-1].nextpage = NULL;

    freeListHead =BFentry;

    for(i=0; i<BF_MAX_BUFS-1; i++)
        freeHash[i].nextentry = &freeHash[i+1];
    freeHash[BF_MAX_BUFS-1].nextentry = NULL;

    freeHashHead =freeHash;

}
int BF_AllocBuf(BFreq bq, PFpage **fpage){
    BFpage *BFptr = NULL;
    int msg;
    if(!isPagePresent(bq, &BFptr)){
        if((msg=freePage())<0)
            return setErr(msg);
        
        BFptr = freeListHead;
        freeListHead = freeListHead->nextpage;
        if(freeHashHead != NULL ) freeHashHead->preventry = NULL;
        copyBuff(bq, BFptr);
        insertEnd(BFptr);

        addToHash(BFptr);

        *fpage = &(BFptr->fpage);
        return BFE_OK;
    }else {
        return setErr(BFE_PAGEINBUF);
    }
}
int BF_GetBuf(BFreq bq, PFpage **fpage){
    BFpage *BFptr = NULL;
    if(!isPagePresent(bq, &BFptr)){
        int msg;
        if((msg = freePage())<0)
            return msg;
        
        BFptr = freeListHead;
        freeListHead  = freeListHead->nextpage;

        copyBuff(bq, BFptr);
        insertEnd(BFptr);

        readPage(BFptr);
        addToHash(BFptr);

        *fpage = &(BFptr->fpage);

        return BFE_OK;
    } else { /* found page*/
        BFptr->count++;
        *fpage = &(BFptr->fpage);

        updateLRU(BFptr);
        return BFE_OK;
    }
}
int BF_UnpinBuf(BFreq bq){
    BFpage *BFptr = NULL;
    if(!isPagePresent(bq, &BFptr))
        return setErr(BFE_PAGENOTINBUF);
    
    if(BFptr->count <=0)
        return setErr(BFE_PAGEUNPINNED);
    
    BFptr->count--;
    return BFE_OK;
}
int BF_TouchBuf(BFreq bq){
    BFpage *BFptr = NULL;
    if(!isPagePresent(bq, &BFptr))
        return setErr(BFE_PAGENOTINBUF);
    
    if(BFptr->count <= 0 )
        return setErr(BFE_PAGEUNPINNED);
    
    BFptr->dirty = TRUE;
    updateLRU(BFptr);
    return BFE_OK;
}
int BF_FlushBuf(int fd){
    BFhash_entry *hashPtr = NULL;
    BFhash_entry *tmpPtr = NULL;
    BFpage *BFptr;
    int i;

    for(i=0;i<BF_HASH_TBL_SIZE; i++){
        hashPtr= hashT[i];
        while(hashPtr!=NULL){
            if(hashPtr->fd == fd){
                if(hashPtr->bpage->count > 0)
                    return setErr(BFE_PAGEPINNED);
                
                tmpPtr = hashPtr->nextentry;

                if(hashPtr == hashT[i])
                    hashT[i] = hashPtr->nextentry;
                
                if(hashPtr->preventry != NULL)
                    hashPtr->preventry->nextentry = hashPtr->nextentry;
                if(hashPtr->nextentry != NULL)
                    hashPtr->nextentry->preventry = hashPtr->preventry;

                hashPtr->preventry = NULL;
                hashPtr->nextentry = freeHashHead;
                freeHashHead = hashPtr;

                BFptr = hashPtr->bpage;
                if(BFptr->dirty)
                    writePage(BFptr);

                freeBFPtr(BFptr);

                hashPtr = tmpPtr;
            } else 
                hashPtr = hashPtr->nextentry;
        }
    }
    return BFE_OK;
}
void BF_ShowBuf(void){
    BFpage *BFptr;
    int i;
    
    printf("\nThe buffer pool content:\n");
    if(numLRU==0)
        printf("empty\n");
    else {
        BFptr = LRUhead->prevpage;
        printf("%-s\t%-s\t%-s\t%-s\t%-s\n", "pageNum", "fd", "unixfd", "count", "dirty");
        for(i=0; i<numLRU; i++){
            printBuff(BFptr);
            BFptr = BFptr->prevpage;
        }
    }
}
void BF_PrintError(const char *s){
    fputs(s, stderr);
    fputs(": ", stderr);
    switch(BFerrno){
        case 1:
            /*not initialized*/
            break;
        case 0:
            /*BFE_OK*/
            break;
        case -1:
            fputs("BFE_NOMEM", stderr);
            break;
        case -2:
            fputs("BFE_NOBUF", stderr);
            break;
        case -3:
            fputs("BFE_PAGEPINNED", stderr);
            break;
        case -4:
            fputs("BFE_PAGEUNPINNED", stderr);
            break;
        case -5:
            fputs("BFE_PAGEINBUF", stderr);
            break;
        case -6:
            fputs("BFE_PAGENOTINBUF", stderr);
            break;
        case -7:
            fputs("BFE_INCOMPLETEWRITE", stderr);
            break;
        case -8:
            fputs("BFE_INCOMPLETEREAD", stderr);
            break;
        case -9:
            fputs("BFE_MISSDIRTY", stderr);
            break;
        case -10:
            fputs("BFE_INVALIDTID", stderr);
            break;
        case -11:
            fputs("BFE_MSGERR", stderr);
            break;
        case -12:
            fputs("BFE_HASHNOTFOUND", stderr);
            break;
        case -13:
            fputs("BFE_HASHPAGEEXIST", stderr);
            break;
        default:
            break;
    }
    fputs("\n", stderr);
}
