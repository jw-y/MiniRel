#include "bfinternal.h"
#include <assert.h>


int hashF(const int fd, int pageNum){
    return (fd+pageNum) % BF_HASH_TBL_SIZE;
}

void writePage(BFpage *BFptr){
    lseek(BFptr->unixfd, (BFptr->pageNum+1)*PAGE_SIZE, SEEK_SET);
    write(BFptr->unixfd, BFptr->fpage.pagebuf, PAGE_SIZE);
}
void readPage(BFpage *BFptr){
    lseek(BFptr->unixfd, (BFptr->pageNum+1)*PAGE_SIZE, SEEK_SET);
    read(BFptr->unixfd, BFptr->fpage.pagebuf, PAGE_SIZE);
}

bool_t isPagePresent(BFreq bq, BFpage **BFptr){
    int fd = bq.fd;
    int pageNum = bq.pagenum;
    BFhash_entry *hashPtr = hashT[hashF(fd, pageNum)];
    while(hashPtr != NULL){
        if(hashPtr->fd == fd && hashPtr->pageNum == pageNum){
            *BFptr = hashPtr->bpage;
            return TRUE;
        }
        hashPtr = hashPtr->nextentry;
    }
    return FALSE;
}

void updateLRU(BFpage *BFptr){
    if(BFptr == LRUhead->prevpage)
        return;

    delete(BFptr);
    insertEnd(BFptr);
}

void addToHash(BFpage *BFptr){
    int hashInd = hashF(BFptr->fd, BFptr->pageNum);
    BFhash_entry *newEntry;

    newEntry = freeHashHead;
    freeHashHead = freeHashHead->nextentry;
    if(freeHashHead != NULL) freeHashHead->preventry =NULL;

    newEntry->bpage = BFptr;
    newEntry->fd = BFptr->fd;
    newEntry->pageNum = BFptr->pageNum;
    newEntry->preventry = NULL;
    newEntry->nextentry = NULL;

    newEntry->nextentry = hashT[hashInd];
    if(hashT[hashInd] != NULL)
        hashT[hashInd]->preventry = newEntry;

    hashT[hashInd] = newEntry;
}

int freePage(){
    BFpage *tmp = LRUhead;
    int numRead = 0;

    if(freeListHead != NULL)
        return BFE_OK;

    while(numRead<BF_MAX_BUFS){
        if(tmp->count<=0)
            return freeBufferEntry(tmp);
        tmp = tmp->nextpage;
        numRead++;
    }
    return BFE_NOBUF;
}
void freeHashPtr(BFhash_entry *hashPtr, int hashInd){
    if(hashPtr == hashT[hashInd]){
        hashT[hashInd] = hashPtr->nextentry;
    }
    if(hashPtr->preventry != NULL)
        hashPtr->preventry->nextentry = hashPtr->nextentry;
    if(hashPtr->nextentry != NULL)
        hashPtr->nextentry->preventry = hashPtr->preventry;

    hashPtr->nextentry = freeHashHead;
    hashPtr->preventry = NULL;
    freeHashHead = hashPtr;
}
void freeBFPtr(BFpage *BFptr){
    delete(BFptr);
    insertFree(BFptr);
}
int freeBufferEntry(BFpage *BFptr){
    int fd, pageNum, hashInd;
    BFhash_entry *hashPtr;

    if(BFptr->dirty)
        writePage(BFptr);

    fd = BFptr->fd;
    pageNum = BFptr->pageNum;

    hashInd = hashF(fd, pageNum);
    hashPtr = hashT[hashInd];
    while(1){
        if(hashPtr->fd == fd && hashPtr->pageNum ==pageNum){
            freeHashPtr(hashPtr, hashInd);
            break;
        }
        hashPtr = hashPtr->nextentry;
    }
    freeBFPtr(BFptr);
    return BFE_OK;
}

void copyBuff(BFreq bq, BFpage *BFptr){
    BFptr->dirty = FALSE;
    BFptr->count = 1;
    BFptr->pageNum = bq.pagenum;
    BFptr->fd = bq.fd;
    BFptr->unixfd = bq.unixfd;
    BFptr->prevpage = BFptr;
    BFptr->nextpage = BFptr;
}

void freeBufferEntryFromHash(BFhash_entry *hashPtr, int ind){
    BFpage *BFptr;

    freeHashPtr(hashPtr, ind);

    BFptr = hashPtr->bpage;
    if(BFptr->dirty)
        writePage(BFptr);
    freeBFPtr(BFptr);
}

void printBuff(BFpage *BFptr){
    printf("%-d\t%-d\t%-d\t%-d\t%-d\n", BFptr->pageNum, BFptr->fd, BFptr->unixfd, BFptr->count, (BFptr->dirty)?1:0);
}


/*
void insertEnd(BFpage *BFptr){
    BFpage *last;

    if(LRUhead==NULL){
        BFptr->prevpage =  BFptr->nextpage = BFptr;
        LRUhead = BFptr;
        numLRU++;
        return;
    }
    last = LRUhead->prevpage;

    BFptr->nextpage = LRUhead;
    LRUhead->prevpage = BFptr;

    BFptr->prevpage = last;
    last->nextpage = BFptr;

    numLRU++;
}*/
void insertEnd(BFpage *BFptr){
    if(LRUhead==NULL){
        BFptr->prevpage = BFptr->nextpage=BFptr;
        LRUhead = BFptr;
        numLRU++;
        return;
    }
    BFptr->nextpage = LRUhead;
    BFptr->prevpage = LRUhead->prevpage;

    LRUhead->prevpage->nextpage = BFptr;
    LRUhead->prevpage = BFptr;
    numLRU++;
}

/*
void delete(BFpage *BFptr){
    BFpage *prev, *tmp;
    if(BFptr->nextpage == BFptr){
        LRUhead = NULL;
        numLRU--;
        return;
    }
    prev = BFptr->prevpage;
    if(BFptr == LRUhead){
        prev = LRUhead->prevpage;
        LRUhead = LRUhead->nextpage;

        prev->nextpage = LRUhead;
        LRUhead->prevpage = prev;
    } else if(BFptr->nextpage == LRUhead){
        prev->nextpage = LRUhead;
        LRUhead->prevpage = prev;
    } else {
        tmp = BFptr->nextpage;

        prev->nextpage = tmp;
        tmp->prevpage = prev;
    }
    numLRU--;
}*/

void delete(BFpage *BFptr){
    if(BFptr->nextpage == BFptr){
        LRUhead = NULL;
        numLRU--;
        return;
    }
    if(BFptr == LRUhead)
        LRUhead = LRUhead->nextpage;

    BFptr->nextpage->prevpage = BFptr->prevpage;
    BFptr->prevpage->nextpage = BFptr->nextpage;
    
    numLRU--;
}

void insertFree(BFpage *BFptr){
    BFptr->nextpage = freeListHead;
    BFptr->prevpage = NULL;
    freeListHead = BFptr;
}