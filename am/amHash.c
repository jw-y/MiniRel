#include <stddef.h>
#include <assert.h>
#include <string.h>

#include "../h/minirel.h"
#include "amHash.h"

AMhash_entry *AMhashT[AM_HASH_TBL_SIZE]={NULL};
AMhash_entry AMfreeHash[AM_ITAB_SIZE];
AMhash_entry *AMfreeHashHead;

void AMhashInit(){
    int i;

    for (i = 0; i < AM_ITAB_SIZE - 1; i++)
        AMfreeHash[i].nextentry = &AMfreeHash[i+1];
    AMfreeHash[AM_ITAB_SIZE - 1].nextentry = NULL;
    AMfreeHashHead = &(AMfreeHash[0]);
}

int AMhashF(const char *filename){
    unsigned long hash;
    int c;
    hash = 5381;
    while(c= *filename++)
        hash = ((hash<<5)+hash)+c;
    
    return hash % AM_HASH_TBL_SIZE;
}

bool_t isInAMHash(const char *filename){
    int hashInd = AMhashF(filename);
    AMhash_entry *hashPtr = AMhashT[hashInd];

    while (hashPtr != NULL)
        if (strcmp(hashPtr->AM_ele->fileName, filename) != 0)
            hashPtr = hashPtr->nextentry;
        else 
            return TRUE;
        
    return FALSE;
}

void AMaddToHash(AMitab_ele *AM_ele){
    int hashInd;
    AMhash_entry *hashPtr;
    AMhash_entry *newEntry;
    assert(AMfreeHashHead != NULL);

    hashInd = AMhashF(AM_ele->fileName);
    hashPtr = AMhashT[hashInd];

    newEntry = AMfreeHashHead;
    AMfreeHashHead = AMfreeHashHead->nextentry;
    newEntry->AM_ele = AM_ele;
    newEntry->preventry = NULL;
    newEntry->nextentry = NULL;

    if(hashPtr!=NULL){
        newEntry->nextentry = hashPtr;
        hashPtr->preventry = newEntry;
    }
    AMhashT[hashInd] = newEntry;
}

void AMfreeHashPtr(AMhash_entry *hashPtr, int hashInd){
    if(hashPtr == AMhashT[hashInd]){
        AMhashT[hashInd] = hashPtr->nextentry;
        if(AMhashT[hashInd]!= NULL)
            AMhashT[hashInd]->preventry = NULL;
    }
    if(hashPtr->preventry != NULL)
        hashPtr->preventry->nextentry = hashPtr->nextentry;
    if(hashPtr->nextentry != NULL)
        hashPtr->nextentry->preventry = hashPtr->preventry;
    
    /*freeing hash entry*/
    hashPtr->nextentry = AMfreeHashHead;
    hashPtr->preventry = NULL;
    AMfreeHashHead =hashPtr;
}

void AMremoveFromHash(AMitab_ele *AM_ele){
    int hashInd = AMhashF(AM_ele->fileName);
    AMhash_entry *hashPtr = AMhashT[hashInd];
    
    while(hashPtr !=NULL){
        if(strcmp(hashPtr->AM_ele->fileName, AM_ele->fileName)!=0)
            hashPtr = hashPtr->nextentry;
        else{
            AMfreeHashPtr(hashPtr, hashInd);
            return;
        }
    }
    assert(0 &&"hash not found");
}