#ifndef BFINTERNAL_H
#define BFINTERNAL_H

#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#include "../h/minirel.h"
#include "../h/bf.h"

typedef struct BFpage{
    PFpage fpage;            /* page data from the file                 */
    struct BFpage *nextpage; /* next in the linked list of buffer pages */
    struct BFpage *prevpage; /* prev in the linked list of buffer pages */
    bool_t dirty;            /* TRUE if page is dirty                   */
    short count;             /* pin count associated with the page      */
    int pageNum;             /* page number of this page                */
    int fd;                  /* PF file descriptor of this page         */
    int unixfd;              /* Unix file descriptor of this page       */
} BFpage;

typedef struct BFhash_entry{
    struct BFhash_entry *nextentry; /* next hash table element or NULL */
    struct BFhash_entry *preventry; /* prev hash table element or NULL */
    int fd;                         /* file descriptor                 */
    int pageNum;                    /* page number                     */
    struct BFpage *bpage;           /* ptr to buffer holding this page */
} BFhash_entry;

extern BFpage *LRUhead;
extern BFpage *freeListHead;
extern BFpage BFentry[BF_MAX_BUFS];
extern int numLRU;

extern BFhash_entry *hashT[BF_HASH_TBL_SIZE];
extern BFhash_entry *freeHashHead;
extern BFhash_entry freeHash[BF_MAX_BUFS];

bool_t isPagePresent(BFreq bq, BFpage **BFptr);
int freePage();
void copyBuff(BFreq bq, BFpage *BFptr);
void addToHash(BFpage *BFptr);
void readPage(BFpage *BFptr);
void updateLRU(BFpage *BFptr);
int freeBufferEntry(BFpage *BFptr);
void freeBufferEntryFromHash(BFhash_entry *hashPtr, int ind);

void printBuff(BFpage *BFptr);

void writePage(BFpage *BFptr);
void freeBFPtr(BFpage *BFptr);
void insertEnd(BFpage *BFptr);
void delete(BFpage *BFptr);
void insertFree(BFpage *BFptr);


#endif
