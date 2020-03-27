#include <stddef.h>
#include <assert.h>
#include <string.h>

#include "../h/minirel.h"
#include "hfHash.h"

HFhash_entry *HFhashT[HF_HASH_TBL_SIZE]={NULL};
HFhash_entry HFfreeHash[HF_FTAB_SIZE];
HFhash_entry *HFfreeHashHead;

void HFhashInit(){
    int i;

    for(i=0; i<HF_FTAB_SIZE-1; i++)
        HFfreeHash[i].nextentry = &HFfreeHash[i+1];
    HFfreeHash[HF_FTAB_SIZE-1].nextentry = NULL;
    HFfreeHashHead = &(HFfreeHash[0]);
}

int HFhashF(const char *filename){
    unsigned long hash;
    int c;
    hash = 5381;
    while(c= *filename++)
        hash = ((hash<<5)+hash)+c;
    
    return hash % HF_HASH_TBL_SIZE;
}

bool_t isInHFHash(const char *filename){
    int hashInd = HFhashF(filename);
    HFhash_entry *hashPtr = HFhashT[hashInd];

    while (hashPtr != NULL)
        if (strcmp(hashPtr->HF_ele->fileName, filename) != 0)
            hashPtr = hashPtr->nextentry;
        else 
            return TRUE;
        
    return FALSE;
}

void HFaddToHash(HFftab_ele *HF_ele){
    int hashInd;
    HFhash_entry *hashPtr;
    HFhash_entry *newEntry;
    assert(HFfreeHashHead != NULL);

    hashInd = HFhashF(HF_ele->fileName);
    hashPtr = HFhashT[hashInd];

    newEntry = HFfreeHashHead;
    HFfreeHashHead = HFfreeHashHead->nextentry;
    newEntry->HF_ele = HF_ele;
    newEntry->preventry = NULL;
    newEntry->nextentry = NULL;

    if(hashPtr!=NULL){
        newEntry->nextentry = hashPtr;
        hashPtr->preventry = newEntry;
    }
    HFhashT[hashInd] = newEntry;
}

void HFfreeHashPtr(HFhash_entry *hashPtr, int hashInd){
    if(hashPtr == HFhashT[hashInd]){
        HFhashT[hashInd] = hashPtr->nextentry;
        if(HFhashT[hashInd]!= NULL)
            HFhashT[hashInd]->preventry = NULL;
    }
    if(hashPtr->preventry != NULL)
        hashPtr->preventry->nextentry = hashPtr->nextentry;
    if(hashPtr->nextentry != NULL)
        hashPtr->nextentry->preventry = hashPtr->preventry;
    
    /*freeing hash entry*/
    hashPtr->nextentry = HFfreeHashHead;
    hashPtr->preventry = NULL;
    HFfreeHashHead =hashPtr;
}

void HFremoveFromHash(HFftab_ele *HF_ele){
    int hashInd = HFhashF(HF_ele->fileName);
    HFhash_entry *hashPtr = HFhashT[hashInd];
    
    while(hashPtr !=NULL){
        if(strcmp(hashPtr->HF_ele->fileName, HF_ele->fileName)!=0)
            hashPtr = hashPtr->nextentry;
        else{
            HFfreeHashPtr(hashPtr, hashInd);
            return;
        }
    }
    assert(0 &&"hash not found");
}