#include <string.h>
#include <stdio.h>

#include "../h/pf.h"
#include "aminternal.h"

bool_t isNodeFull(Nodehdr *curHdr, int fileDesc){
    if((curHdr->isLeaf && curHdr->numKeys >= AMitab[fileDesc].AMhdr.maxFanoutLeaf)
        ||  (!curHdr->isLeaf && curHdr->numKeys >= (AMitab[fileDesc].AMhdr.maxFanoutInter-1)))
        return TRUE;
    else
        return FALSE;
}

int AMcmp(const char *value1,const char *value2,const char attrType,const int attrLength){
    /* <0 if value 1 is smaller
    0 if same
    >0 if value 2 is bigger*/
    if(attrType=='c')
        return strncmp(value1, value2, attrLength);
    else if(attrType=='i')
        return *((int*)value1) - *((int*)value2);
    else /*f*/
        return *((float*)value1)-*((float*)value2);
}
char* keyLoc(char *pagebuf, const int fileDesc, bool_t isLeaf){
    if(isLeaf)
        return pagebuf+sizeof(LeafHdr);
    else
        return pagebuf+sizeof(InterHdr);
}
char* childLoc(char *pagebuf,const int fileDesc, const int attrLength, bool_t isLeaf){
    if(isLeaf)
        return pagebuf+sizeof(LeafHdr)+attrLength*AMitab[fileDesc].AMhdr.maxFanoutLeaf;
    else
        return pagebuf+sizeof(InterHdr)+attrLength*(AMitab[fileDesc].AMhdr.maxFanoutInter-1);
}

bool_t ifRetrived(AMsearchPtr *searchPtr,RECID curId){
    if(searchPtr->lastRetrived.pagenum==curId.pagenum && searchPtr->lastRetrived.recnum==curId.recnum)
        return TRUE;
    else
        return FALSE;
}

int splitChild(Nodehdr *parentHdr, const int fileDesc, const int childPagenum, 
                const int childInd, const int parentPagenum,const char *value){
    /*childind index of the child not key*/
    /*parent should be pinned and have to unpin after*/
    /*check which child to goto after*/
    int msg;
    int newPagenum;
    char *curChildPagebuf, *newChildPagebuf;
    char *keyPtr;
    Nodehdr *curChildHdr, *newChildHdr;
    LeafHdr *curLeafHdr, *newLeafHdr;

    char *parentKeyPtr, *parentChildPtr;

    const char attrType = AMitab[fileDesc].AMhdr.attrType;
    const int attrLength = AMitab[fileDesc].AMhdr.attrLength;

    if((msg=PF_GetThisPage(AMitab[fileDesc].PFfd, childPagenum, &curChildPagebuf))<0)
        return AME_PF;

    if((msg=PF_AllocPage(AMitab[fileDesc].PFfd, &newPagenum, &newChildPagebuf))<0)
        return AME_PF;

    curChildHdr = (Nodehdr*) curChildPagebuf;
    newChildHdr = (Nodehdr*) newChildPagebuf;
    newChildHdr->isLeaf = curChildHdr->isLeaf;
    if(curChildHdr->isLeaf){ /*child is leaf*/

        curChildHdr->numKeys = AMitab[fileDesc].AMhdr.maxFanoutLeaf / 2;

        keyPtr = keyLoc((char*)curChildHdr, fileDesc, TRUE);

        /*duplicate keys should be in the same leaf*/
        while(curChildHdr->numKeys > 0 &&
            AMcmp(keyPtr+attrLength*curChildHdr->numKeys, keyPtr+attrLength*(curChildHdr->numKeys-1), attrType, attrLength)==0)
            curChildHdr->numKeys--;

        if(curChildHdr->numKeys==0){
            curChildHdr->numKeys = AMitab[fileDesc].AMhdr.maxFanoutLeaf / 2;
            while (curChildHdr->numKeys < AMitab[fileDesc].AMhdr.maxFanoutLeaf &&
                   AMcmp(keyPtr + attrLength * (curChildHdr->numKeys-1), keyPtr + attrLength * curChildHdr->numKeys, attrType, attrLength) == 0)
                curChildHdr->numKeys++;

            if(curChildHdr->numKeys>=AMitab[fileDesc].AMhdr.maxFanoutLeaf){
                if(AMcmp(keyPtr, value, attrType, attrLength)<0){
                    curChildHdr->numKeys = AMitab[fileDesc].AMhdr.maxFanoutLeaf;
                }else{
                    curChildHdr->numKeys = 0;
                }
            }
        }

        newChildHdr->numKeys = AMitab[fileDesc].AMhdr.maxFanoutLeaf - curChildHdr->numKeys;

        /*copy keys and children*/
        memcpy(keyLoc((char*)newChildHdr, fileDesc, TRUE), 
                keyLoc((char*)curChildHdr, fileDesc, TRUE)+attrLength*curChildHdr->numKeys, 
                attrLength*newChildHdr->numKeys);
        memcpy(childLoc((char*)newChildHdr, fileDesc, attrLength, TRUE), 
                childLoc((char*)curChildHdr, fileDesc, attrLength, TRUE)+sizeof(RECID)*curChildHdr->numKeys,
                sizeof(RECID)*newChildHdr->numKeys);
        
        curLeafHdr = (LeafHdr*) curChildHdr;
        newLeafHdr = (LeafHdr*) newChildHdr;
        newLeafHdr->nextLeaf = curLeafHdr->nextLeaf;
        newLeafHdr->prevLeaf = childPagenum;
        curLeafHdr->nextLeaf = newPagenum;

    } else { /*child is internal*/
        newChildHdr->numKeys = (AMitab[fileDesc].AMhdr.maxFanoutInter-1) /2;
        curChildHdr->numKeys = (AMitab[fileDesc].AMhdr.maxFanoutInter-1) - newChildHdr->numKeys - 1;

        /*copy keys and children*/
        memcpy(keyLoc((char*)newChildHdr, fileDesc, FALSE),
                keyLoc((char*)curChildHdr, fileDesc, FALSE)+attrLength*(curChildHdr->numKeys+1),
                attrLength*newChildHdr->numKeys);
        memcpy(childLoc((char*)newChildHdr, fileDesc, attrLength, FALSE),
                childLoc((char*)curChildHdr, fileDesc, attrLength, FALSE)+sizeof(int)*(curChildHdr->numKeys+1),
                sizeof(int)*(newChildHdr->numKeys+1));
    }

    parentKeyPtr = keyLoc((char *)parentHdr, fileDesc, FALSE);
    parentChildPtr = childLoc((char *)parentHdr, fileDesc, attrLength, FALSE);

    memmove(parentKeyPtr + attrLength * (childInd + 1),
            parentKeyPtr + attrLength * childInd,
            attrLength * (parentHdr->numKeys - childInd));

    if(curChildHdr->isLeaf && curChildHdr->numKeys >= AMitab[fileDesc].AMhdr.maxFanoutLeaf){
        memcpy(parentKeyPtr + attrLength * childInd, value, attrLength);
    } else {
        memcpy(parentKeyPtr + attrLength * childInd,
                keyLoc((char*)curChildHdr, fileDesc, curChildHdr->isLeaf) + attrLength * curChildHdr->numKeys, 
                attrLength);
    }

    memmove(parentChildPtr + sizeof(int) * (childInd + 1 + 1),
            parentChildPtr + sizeof(int) * (childInd + 1),
            sizeof(int) * (parentHdr->numKeys - childInd));
    ((int *)parentChildPtr)[childInd + 1] = newPagenum;

    parentHdr->numKeys++;

    if ((msg = PF_UnpinPage(AMitab[fileDesc].PFfd, childPagenum, TRUE)) < 0)
        return msg;
    if ((msg = PF_UnpinPage(AMitab[fileDesc].PFfd, newPagenum, TRUE)) < 0)
        return msg;

    return AME_OK;
}

int BtreeInsNonFull(int pagenum, const int fileDesc, const char attrType, 
                    const int attrLength, const char *value, const RECID recID){
    int msg;
    int keyInd, childInd;
    char *keyPtr, *childPtr;
    char *pagebuf, *childPagebuf;
    Nodehdr *curHdr, *childHdr;

    if((msg=PF_GetThisPage(AMitab[fileDesc].PFfd, pagenum, &pagebuf))<0)
        return msg;

    curHdr = (Nodehdr*) pagebuf;

    keyInd = curHdr->numKeys-1;

    if(curHdr->isLeaf){ /*leaf*/
        keyPtr = keyLoc((char*)curHdr, fileDesc, TRUE);
        childPtr = childLoc((char*)curHdr, fileDesc, attrLength, TRUE);

        while(keyInd>=0 && AMcmp(value, keyPtr+attrLength*keyInd, attrType, attrLength) < 0){
            keyInd--;
        }
        keyInd++;

        memmove(keyPtr+attrLength*(keyInd+1), keyPtr+attrLength*keyInd, attrLength*(curHdr->numKeys-keyInd));
        memmove(childPtr+sizeof(RECID)*(keyInd+1), childPtr+sizeof(RECID)*keyInd,sizeof(RECID)*(curHdr->numKeys-keyInd));

        memcpy(keyPtr+attrLength*keyInd, value, attrLength);
        memcpy(childPtr+sizeof(RECID)*keyInd, &recID, sizeof(RECID));
        curHdr->numKeys++;

        if((msg=PF_UnpinPage(AMitab[fileDesc].PFfd, pagenum, TRUE))<0)
            return AME_PF;

    } else { /*internal*/
        keyPtr = keyLoc((char*)curHdr, fileDesc, FALSE);
        childPtr = childLoc((char*)curHdr, fileDesc, attrLength, FALSE);

        while(keyInd>=0 && AMcmp(value, keyPtr+attrLength*keyInd, attrType, attrLength)<0)
            keyInd--;
        /*keyInd++;*/
        childInd = keyInd+1;

        if((msg=PF_GetThisPage(AMitab[fileDesc].PFfd, ((int*)childPtr)[childInd], &childPagebuf))<0)
            return AME_PF;
        
        if ((msg = PF_UnpinPage(AMitab[fileDesc].PFfd, ((int *)childPtr)[childInd], FALSE)) < 0)
            return AME_PF;

        childHdr = (Nodehdr*) childPagebuf;
        /*if child full*/
        if( isNodeFull(childHdr, fileDesc)){
            if((msg=splitChild(curHdr, fileDesc, ((int*)childPtr)[childInd], childInd, pagenum, value))<0)
                return msg;
            if((msg=PF_UnpinPage(AMitab[fileDesc].PFfd, pagenum, TRUE))<0)
                return AME_PF;
            if(AMcmp(value, keyPtr+attrLength*(keyInd+1), attrType, attrLength)>=0)
                keyInd++;
        } else {
            if ((msg = PF_UnpinPage(AMitab[fileDesc].PFfd, pagenum, FALSE)) < 0)
                return AME_PF;
        } 

        if((msg=BtreeInsNonFull(((int*)childPtr)[keyInd+1],fileDesc, attrType, attrLength,
                                value, recID ))<0)
            return msg;
    }
    return AME_OK;
}

int BtreeIns(const int fileDesc, const char attrType,
        const int attrLength, const char *value, const RECID recID){
    int msg;
    int origRootnum, newRootnum;
    char *rootPagebuf, *newRootPagebuf;
    Nodehdr *curHdr;
    InterHdr *newRoothdr;
    int nextPage;

    origRootnum = AMitab[fileDesc].AMhdr.rootPtr;

    if((msg=PF_GetThisPage(AMitab[fileDesc].PFfd, origRootnum, &rootPagebuf))<0)
        return AME_PF;
    
    curHdr = (Nodehdr*)rootPagebuf;
    if(isNodeFull(curHdr, fileDesc)){
        /*root full*/
        if((msg=PF_AllocPage(AMitab[fileDesc].PFfd, &newRootnum, &newRootPagebuf))<0)
            return AME_PF;

        newRoothdr = (InterHdr*) newRootPagebuf;
        newRoothdr->hdr.isLeaf = FALSE;
        newRoothdr->hdr.numKeys = 0;
        AMitab[fileDesc].AMhdr.rootPtr = newRootnum;
        AMitab[fileDesc].hdrchanged = TRUE;

        /*first child orig root*/
        ((int *)childLoc((char*)newRoothdr, fileDesc, attrLength, FALSE))[0] = origRootnum;

        if ((msg = PF_UnpinPage(AMitab[fileDesc].PFfd, origRootnum, FALSE)) < 0)
            return AME_PF;

        /*split child 0*/
        if ((msg = splitChild((Nodehdr*)newRoothdr, fileDesc, origRootnum, 0, newRootnum, value))<0)
            return msg;
        if ((msg = PF_UnpinPage(AMitab[fileDesc].PFfd, newRootnum, TRUE)) < 0)
            return AME_PF;

        nextPage = origRootnum;
        if(AMcmp(value, keyLoc((char*)newRoothdr, fileDesc, FALSE), attrType, attrLength)>=0)
            nextPage = ((int*)childLoc((char*)newRoothdr, fileDesc, attrLength, FALSE))[1];

        if((msg=BtreeInsNonFull(nextPage, fileDesc, attrType, attrLength, value, recID))<0)
            return msg;
    } else { /* root not full*/
        if((msg=PF_UnpinPage(AMitab[fileDesc].PFfd, origRootnum, FALSE))<0)
            return msg;
        if((msg=BtreeInsNonFull(origRootnum, fileDesc, attrType, attrLength, value, recID))<0)
            return msg;
    }
    return AME_OK;
}

int BtreeDelete(const int fileDesc,const int pagenum,const char attrType,const int attrLength,const char *value,const RECID recID){
    int msg;
    int keyInd;
    char *keyPtr, *childPtr;
    char *pagebuf;
    /*LeafHdr *curLeafHdr;*/
    Nodehdr *curHdr;
    AMsearchPtr curSearch;

    curSearch.pagenum = pagenum;

    if((msg=BtreeSearchOp(EQ_OP, fileDesc, curSearch.pagenum, value, &curSearch))<0)
        return msg;
    
    while(1){
        if ((msg = PF_GetThisPage(AMitab[fileDesc].PFfd, curSearch.pagenum, &pagebuf)) < 0)
            return AME_PF;
        
        curHdr = (Nodehdr *)pagebuf;
        keyPtr = keyLoc((char *)curHdr, fileDesc, curHdr->isLeaf);
        childPtr = childLoc((char *)curHdr, fileDesc, attrLength, TRUE);

        if(recID.pagenum==((RECID*)childPtr)[curSearch.index].pagenum 
            && recID.recnum==((RECID*)childPtr)[curSearch.index].recnum)
            break;

        if ((msg = PF_UnpinPage(AMitab[fileDesc].PFfd, curSearch.pagenum, FALSE)) < 0)
            return AME_PF;
        
        if((msg=findNext(EQ_OP, fileDesc, curSearch.pagenum, value, &curSearch))<0)
            return msg;
    }
    keyInd = curSearch.index;
    memmove(keyPtr + attrLength * (keyInd), 
            keyPtr + attrLength * (keyInd + 1), 
            attrLength * (curHdr->numKeys - keyInd - 1));
    memmove(childPtr + sizeof(RECID) * keyInd, 
            childPtr + sizeof(RECID) * (keyInd + 1), 
            sizeof(RECID) * (curHdr->numKeys - keyInd - 1));

    curHdr->numKeys--;
    if ((msg = PF_UnpinPage(AMitab[fileDesc].PFfd, curSearch.pagenum, TRUE)) < 0)
        return AME_PF;
    return AME_OK;
}
bool_t ifSatisOp(const int op,const char *value1,const char *value2,const char attrType,const int attrLength){
    switch(op){
        case EQ_OP:
            if(AMcmp(value1, value2, attrType, attrLength)==0)
                return TRUE;
            return FALSE;
            break;
        case LT_OP:
            if(AMcmp(value1, value2, attrType, attrLength)>0)
                return TRUE;
            return FALSE;
            break;
        case GT_OP:
            if(AMcmp(value1, value2, attrType, attrLength)<0)
                return TRUE;
            return FALSE;
            break;
        case LE_OP:
            if(AMcmp(value1, value2, attrType, attrLength)>=0)
                return TRUE;
            return FALSE;
            break;  
        case GE_OP:
            if(AMcmp(value1, value2, attrType, attrLength)<=0)
                return TRUE;
            return FALSE;
            break;
        case NE_OP:
            if(AMcmp(value1, value2,  attrType, attrLength)!=0)
                return TRUE;
            return FALSE;
            break;
        case ALL_OP:
            return TRUE;
            break;
    }
    return FALSE;
}
bool_t ifContinue(const int op){
    switch(op){
        case GT_OP:
        case GE_OP:
        case NE_OP:
        case ALL_OP:
            return TRUE;
        case EQ_OP:
        case LT_OP:
        case LE_OP:
            return FALSE;
    }
    return FALSE;
}
int findNext(const int op, const int fileDesc, const int pagenum, const char *value, AMsearchPtr *searchPtr){
    int msg;
    char *pagebuf;
    char *keyPtr;
    LeafHdr *curHdr;

    char attrType = AMitab[fileDesc].AMhdr.attrType;
    int attrLength = AMitab[fileDesc].AMhdr.attrLength;

    if ((msg = PF_GetThisPage(AMitab[fileDesc].PFfd, searchPtr->pagenum, &pagebuf)) < 0)
        return AME_PF;
    if ((msg = PF_UnpinPage(AMitab[fileDesc].PFfd, searchPtr->pagenum, FALSE)) < 0)
        return AME_PF;

    keyPtr = keyLoc(pagebuf, fileDesc, TRUE);
    curHdr = (LeafHdr *)pagebuf;

    if(searchPtr->index <curHdr->hdr.numKeys)
        searchPtr->index++;

    while (1){
        if (searchPtr->index < curHdr->hdr.numKeys){
            if (ifSatisOp(op, value, keyPtr + attrLength * searchPtr->index, attrType, attrLength)){
                return AME_OK;
            }
            if(!ifContinue(op))
                return AME_EOF;
            searchPtr->index++;
        }else{
            if (curHdr->nextLeaf != NULL_PAGE){
                searchPtr->index = -1;
                searchPtr->pagenum = curHdr->nextLeaf;
                return findNext(op, fileDesc, searchPtr->pagenum, value, searchPtr);
            }
            else{
                return AME_EOF;
            }
        }
    }
    return AME_OK;
}

int BtreeSearchOp(const int op, const int fileDesc, const int pagenum,const char *value, AMsearchPtr *searchRet){
    int msg;
    char *pagebuf;
    int keyInd;
    char *keyPtr, *childPtr;
    Nodehdr *curHdr;
    LeafHdr *curLeafHdr;
    
    char attrType = AMitab[fileDesc].AMhdr.attrType;
    int attrLength = AMitab[fileDesc].AMhdr.attrLength;

    if ((msg = PF_GetThisPage(AMitab[fileDesc].PFfd, pagenum, &pagebuf)) < 0)
        return AME_PF;
    if ((msg = PF_UnpinPage(AMitab[fileDesc].PFfd, pagenum, FALSE)) < 0)
        return AME_PF;

    curHdr = (Nodehdr *)pagebuf;
    keyPtr = keyLoc((char*)curHdr, fileDesc, curHdr->isLeaf);

    keyInd = 0;
    while (keyInd < curHdr->numKeys && AMcmp(value, keyPtr + attrLength * keyInd, attrType, attrLength) > 0){
        keyInd++;
    }

    if(curHdr->isLeaf){
        childPtr = childLoc((char*)curHdr, fileDesc, attrLength, TRUE);
        curLeafHdr = (LeafHdr*)pagebuf;
     
        if (keyInd >= curHdr->numKeys){
            if(curLeafHdr->nextLeaf==NULL_PAGE)
                return AME_EOF;
            searchRet->pagenum = curLeafHdr->nextLeaf;
            searchRet->index = -1;
            return findNext(op, fileDesc, curLeafHdr->nextLeaf, value, searchRet);
        }

        searchRet->index = keyInd;
        searchRet->pagenum = pagenum;
       
        return AME_OK;
    } else { /*leaf*/
        if (keyInd < curHdr->numKeys && AMcmp(value, keyPtr + attrLength * keyInd, attrType, attrLength) == 0)
            keyInd++;
        childPtr=childLoc((char*)curHdr, fileDesc, attrLength, FALSE);

        return BtreeSearchOp(op, fileDesc, ((int*)childPtr)[keyInd], value, searchRet);
    }
}
int BtreeLeftMost(const int fileDesc, const int pagenum, AMsearchPtr *searchRet){
    int msg;
    char *pagebuf;
    char *childPtr;
    Nodehdr *curHdr;
    LeafHdr *curLeafHdr;
    const int attrLength = AMitab[fileDesc].AMhdr.attrLength;

    if ((msg = PF_GetThisPage(AMitab[fileDesc].PFfd, pagenum, &pagebuf)) < 0)
        return AME_PF;
    if ((msg = PF_UnpinPage(AMitab[fileDesc].PFfd, pagenum, FALSE)) < 0)
        return AME_PF;
    curHdr = (Nodehdr *)pagebuf;

    if(curHdr->isLeaf){
        if(curHdr->numKeys==0){
            curLeafHdr = (LeafHdr*)pagebuf;
            if(curLeafHdr->nextLeaf == NULL_PAGE)
                return AME_EOF;
            searchRet->pagenum = curLeafHdr->nextLeaf;
            searchRet->index = -1;
            return findNext(ALL_OP, fileDesc, searchRet->pagenum, NULL, searchRet);
        }

        searchRet->index = 0;
        searchRet->pagenum = pagenum;
        return AME_OK;
    } else {
        childPtr = childLoc((char*)curHdr, fileDesc, attrLength, FALSE);
        return BtreeLeftMost(fileDesc, ((int*)childPtr)[0], searchRet);
    }
}
