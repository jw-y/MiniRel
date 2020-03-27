#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "pfinternal.h"

int PFhashF(const char *filename){
    unsigned long hash;
    int c;
    hash = 5381;
    while(c= *filename++)
        hash = ((hash<<5)+hash)+c;
    
    return hash % PF_HASH_TBL_SIZE;
}

bool_t isInPFHash(const char *filename){
    int hashInd = PFhashF(filename);
    PFhash_entry *hashPtr = PFhashT[hashInd];

    while (hashPtr != NULL)
        if (strcmp(hashPtr->PF_ele->fname, filename) != 0)
            hashPtr = hashPtr->nextentry;
        else 
            return TRUE;
        
    return FALSE;
}

void PFaddToHash(PFftab_ele *PF_ele){
    int hashInd;
    PFhash_entry *hashPtr;
    PFhash_entry *newEntry;
    assert(PFfreeHashHead != NULL);

    hashInd = PFhashF(PF_ele->fname);
    hashPtr = PFhashT[hashInd];

    newEntry = PFfreeHashHead;
    PFfreeHashHead = PFfreeHashHead->nextentry;
    newEntry->PF_ele = PF_ele;
    newEntry->preventry = NULL;
    newEntry->nextentry = NULL;

    if(hashPtr!=NULL){
        newEntry->nextentry = hashPtr;
        hashPtr->preventry = newEntry;
    }
    PFhashT[hashInd] = newEntry;
}

void PFfreeHashPtr(PFhash_entry *hashPtr, int hashInd){
    if(hashPtr == PFhashT[hashInd]){
        PFhashT[hashInd] = hashPtr->nextentry;
        if(PFhashT[hashInd]!= NULL)
            PFhashT[hashInd]->preventry = NULL;
    }
    if(hashPtr->preventry != NULL)
        hashPtr->preventry->nextentry = hashPtr->nextentry;
    if(hashPtr->nextentry != NULL)
        hashPtr->nextentry->preventry = hashPtr->preventry;
    
    /*freeing hash entry*/
    hashPtr->nextentry = PFfreeHashHead;
    hashPtr->preventry = NULL;
    PFfreeHashHead =hashPtr;
}

void PFremoveFromHash(PFftab_ele *PF_ele){
    int hashInd = PFhashF(PF_ele->fname);
    PFhash_entry *hashPtr = PFhashT[hashInd];
    
    while(hashPtr !=NULL){
        if(strcmp(hashPtr->PF_ele->fname, PF_ele->fname)!=0)
            hashPtr = hashPtr->nextentry;
        else{
            PFfreeHashPtr(hashPtr, hashInd);
            return;
        }
    }
    assert(0 &&"hash not found");
}