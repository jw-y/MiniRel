/*old code*/
int BtreeInsert(const int fileDesc, int pagenum, const char attrType, const int attrLength, const char *value, const RECID recID, char *buf)
{
    int msg, tempPagenum;
    char *pagebuf;
    Nodehdr *curHdr;
    InterHdr *curIntHdr;
    LeafHdr *curLeafHdr;
    char *keyPtr, *childPtr;
    int keyInd, childInd;

    int newPagenum, insertPage;

    bool_t ifSplit = FALSE;

    /*get page*/
    if ((msg = PF_GetThisPage(fileDesc, pagenum, &pagebuf)) < 0)
        return AME_PF;
    curHdr = (Nodehdr *)pagebuf;

    /*points to last key*/
    keyInd = curHdr->numKeys - 1;

    if (curHdr->isLeaf)
    { /* leaf*/

        if (curHdr->numKeys >= AMitab[fileDesc].AMhdr.maxFanoutLeaf)
        {
            /*leaf full --> split*/
            tempPagenum = pagenum;
            if ((newPagenum = splitNode(&curHdr, &pagebuf, fileDesc, &pagenum, attrType, attrLength, value, buf)) < 0)
                return newPagenum;

            /*is root*/
            if (tempPagenum == AMitab[fileDesc].AMhdr.rootPtr)
            {
                if ((msg = makeNewRoot(fileDesc, attrLength, tempPagenum, newPagenum, buf)) < 0)
                    return msg;
            }

            ifSplit = TRUE;
        }

        keyPtr = keyLoc(pagebuf, fileDesc, TRUE);
        childPtr = childLoc(pagebuf, fileDesc, attrLength, TRUE);

        /*leaf should not be full*/
        keyInd = curHdr->numKeys - 1;
        while (keyInd >= 0 && (AMcmp(value, keyPtr + attrLength * keyInd, attrType, attrLength) < 0))
        {
            memcpy(keyPtr + attrLength * (keyInd + 1), keyPtr + attrLength * keyInd, attrLength);
            memcpy(childPtr + sizeof(RECID) * (keyInd + 1), childPtr + sizeof(RECID) * keyInd, sizeof(RECID));
            keyInd--;
        }
        keyInd++;
        memcpy(keyPtr + attrLength * keyInd, value, attrLength);
        memcpy(childPtr + sizeof(RECID) * keyInd, &recID, sizeof(RECID));
        curHdr->numKeys++;

        if ((msg = PF_UnpinPage(fileDesc, pagenum, TRUE)) < 0)
            return AME_PF;

        if (ifSplit)
            return newPagenum;
        else
            return AME_OK;
    }
    else
    { /* internal node*/

        keyPtr = keyLoc(pagebuf, fileDesc, FALSE);
        childPtr = childLoc(pagebuf, fileDesc, attrLength, FALSE);
        while (keyInd >= 0 && (AMcmp(value, keyPtr + attrLength * keyInd, attrType, attrLength) < 0))
            keyInd--;
        keyInd++;
        /*keyIndex is the child index*/
        if ((insertPage = BtreeInsert(fileDesc, *((int *)(childPtr + sizeof(int) * keyInd)), attrType, attrLength, value, recID, buf)) < 0)
            return insertPage; /*err*/
        else if (insertPage > 0)
        { /*insert key*/
            if (curHdr->numKeys >= AMitab[fileDesc].AMhdr.maxFanoutInter - 1)
            {
                tempPagenum = pagenum;
                /*full*/
                if ((newPagenum = splitNode(&curHdr, &pagebuf, fileDesc, &pagenum, attrType, attrLength, value, buf)) < 0)
                    return newPagenum;

                /*is root*/
                if (tempPagenum == AMitab[fileDesc].AMhdr.rootPtr)
                    if ((msg = makeNewRoot(fileDesc, attrLength, tempPagenum, newPagenum, buf)) < 0)
                        return msg;

                ifSplit = TRUE;
            }
            keyPtr = keyLoc(pagebuf, fileDesc, FALSE);
            childPtr = childLoc(pagebuf, fileDesc, attrLength, FALSE);

            /*internal node should not be full*/
            keyInd = curHdr->numKeys - 1;
            while (keyInd >= 0 && (AMcmp(value, keyPtr + attrLength * keyInd, attrType, attrLength) < 0))
            {
                memcpy(keyPtr + attrLength * (keyInd + 1), keyPtr + attrLength * keyInd, attrLength);
                *(((int *)childPtr) + keyInd + 1 + 1) = *((int *)(childPtr) + keyInd + 1);
                keyInd--;
            }
            memcpy(keyPtr + attrLength * (keyInd + 1), buf, attrLength);
            *(((int *)childPtr) + (keyInd + 1 + 1)) = newPagenum;
            curHdr->numKeys++;

            if ((msg = PF_UnpinPage(fileDesc, pagenum, TRUE)) < 0)
                return AME_PF;

            if (ifSplit)
                return newPagenum;
            else
                return AME_OK;
        }
        else
        { /*no pushed key*/
            if ((msg = PF_UnpinPage(fileDesc, pagenum, FALSE)) < 0)
                return msg;
            return AME_OK;
        }
    }
}

static int makeNewRoot(const int fileDesc, const int attrLength, const int pagenum0, const int pagenum1, char *buf)
{
    int msg;
    int newRootPage;
    char *rootPagebuf;
    InterHdr *rootHdr;

    if ((msg = PF_AllocPage(fileDesc, &newRootPage, &rootPagebuf)) < 0)
        return AME_PF;
    rootHdr = (InterHdr *)rootPagebuf;
    rootHdr->hdr.isLeaf = FALSE;
    rootHdr->hdr.numKeys = 1;

    memcpy(keyLoc(rootPagebuf, fileDesc, FALSE), buf, attrLength);
    ((int *)childLoc(rootPagebuf, fileDesc, attrLength, FALSE))[0] = pagenum0;
    ((int *)childLoc(rootPagebuf, fileDesc, attrLength, FALSE))[1] = pagenum1;

    AMitab[fileDesc].AMhdr.rootPtr = newRootPage;
    AMitab[fileDesc].hdrchanged = TRUE;

    if ((msg = PF_UnpinPage(fileDesc, newRootPage, TRUE)) < 0)
        return AME_PF;

    return AME_OK;
}

static int splitNode(Nodehdr **curHdrPtr, char **pagebuf, const int fileDesc, int *pagenum, const char attrType,
                     const int attrLength, const char *value, char *const buf)
{
    /*buf returns the pushed/copied key*/
    int msg;
    char *keyPtr, *childPtr;

    int newPagenum;
    char *newPagebuf;
    Nodehdr *curHdr = *curHdrPtr;
    Nodehdr *newHdr;
    LeafHdr *curLeafHdr, *newLeafHdr;
    char *newKeyPtr, *newChildPtr;

    if ((msg = PF_AllocPage(fileDesc, &newPagenum, &newPagebuf)) < 0)
        return AME_PF;
    newHdr = (Nodehdr *)newPagebuf;
    newHdr->isLeaf = curHdr->isLeaf;
    if (curHdr->isLeaf)
    { /*leaf*/
        newHdr->numKeys = AMitab[fileDesc].AMhdr.maxFanoutLeaf - AMitab[fileDesc].AMhdr.maxFanoutLeaf / 2;
        keyPtr = keyLoc(*pagebuf, fileDesc, TRUE);
        newKeyPtr = keyLoc(newPagebuf, fileDesc, TRUE);

        childPtr = childLoc(keyPtr, fileDesc, attrLength, TRUE);
        newChildPtr = childLoc(newKeyPtr, fileDesc, attrLength, TRUE);

        curHdr->numKeys = curHdr->numKeys - newHdr->numKeys;
        /*copy keys and children*/
        memcpy(newKeyPtr, keyPtr + attrLength * curHdr->numKeys, attrLength * newHdr->numKeys);
        memcpy(newChildPtr, keyPtr + sizeof(RECID) * curHdr->numKeys, sizeof(RECID) * newHdr->numKeys);

        curLeafHdr = (LeafHdr *)curHdr;
        newLeafHdr = (LeafHdr *)newHdr;
        newLeafHdr->nextLeaf = curLeafHdr->nextLeaf;
        newLeafHdr->prevLeaf = *pagenum;
        curLeafHdr->nextLeaf = newPagenum;

        if (AMcmp(value, newKeyPtr, attrType, attrLength) >= 0)
        {
            /*go to new leaf*/
            if ((msg = PF_UnpinPage(fileDesc, *pagenum, TRUE)) < 0)
                return AME_PF;
            *curHdrPtr = newHdr;
            *pagenum = newPagenum;
            *pagebuf = newPagebuf;
        }
        else
        {
            if ((msg = PF_UnpinPage(fileDesc, newPagenum, TRUE)) < 0)
                return AME_PF;
        }

        /*key to copy up*/
        memcpy(buf, newKeyPtr, attrLength);
    }
    else
    {
        newHdr->numKeys = AMitab[fileDesc].AMhdr.maxFanoutInter / 2;
        keyPtr = keyLoc(*pagebuf, fileDesc, FALSE);
        newKeyPtr = keyLoc(newPagebuf, fileDesc, FALSE);

        childPtr = childLoc(keyPtr, fileDesc, attrLength, FALSE);
        newChildPtr = childLoc(newKeyPtr, fileDesc, attrLength, FALSE);

        curHdr->numKeys = curHdr->numKeys - newHdr->numKeys - 1;
        /*copy keys and children*/
        memcpy(newKeyPtr, keyPtr + attrLength * (curHdr->numKeys + 1), attrLength * newHdr->numKeys);
        memcpy(newChildPtr, keyPtr + sizeof(int) * (curHdr->numKeys + 1), sizeof(int) * (newHdr->numKeys + 1));

        if (AMcmp(value, keyPtr + attrLength * (curHdr->numKeys), attrType, attrLength) >= 0)
        {
            /*insert to new internal node*/
            if ((msg = PF_UnpinPage(fileDesc, *pagenum, TRUE)) < 0)
                return AME_PF;
        }
        else
        {
            if ((msg = PF_UnpinPage(fileDesc, newPagenum, TRUE)) < 0)
                return AME_PF;
        }

        /*push up key*/
        memcpy(buf, keyPtr + attrLength * (curHdr->numKeys), attrLength);
    }

    return newPagenum;
}

int AMsearchPage(const int fileDesc, const int pagenum, const char attrType,
                 const int attrLength, const char *value, const bool_t eql, const bool_t Left, AMsearchPtr *searchRet)
{
    int msg;
    int keyInd;
    char *keyPtr, *childPtr;
    LeafHdr *curHdr;
    char *pagebuf;

    if ((msg = PF_GetThisPage(AMitab[fileDesc].PFfd, pagenum, &pagebuf)) < 0)
        return AME_PF;

    curHdr = (LeafHdr *)pagebuf;
    keyInd = 0;
    keyPtr = keyLoc((char *)curHdr, fileDesc, curHdr->hdr.isLeaf);

    if (Left)
    {
        keyInd = 0;
        while (keyInd < curHdr->hdr.numKeys && AMcmp(value, keyPtr + attrLength * keyInd, attrType, attrLength) <= 0)
            keyInd++;
        if (keyInd >= curHdr->hdr.numKeys)
        {
            if ((msg = PF_UnpinPage(AMitab[fileDesc].PFfd, pagenum, FALSE)) < 0)
                return AME_PF;
            if (curHdr->nextLeaf != NULL_PAGE)
                AMsearchPage(fileDesc, curHdr->nextLeaf, attrType, attrLength, value, eql, Left, searchRet);

            return AME_INVALIDSCANDESC;
        }
        else
        {
            if ((msg = PF_UnpinPage(AMitab[fileDesc].PFfd, pagenum, FALSE)) < 0)
                return AME_PF;
            searchRet->pagenum = pagenum;
            searchRet->index = keyInd;
            return AME_OK;
        }
    }
    else
    { /* right*/
        keyInd = curHdr->hdr.numKeys - 1;
        while (keyInd >= 0 && AMcmp(value, keyPtr + attrLength * keyInd, attrType, attrLength) >= 0)
            keyInd--;

        if (keyInd < 0)
        {
            if ((msg = PF_UnpinPage(AMitab[fileDesc].PFfd, pagenum, FALSE)) < 0)
                return AME_PF;
            if (curHdr->nextLeaf != NULL_PAGE)
                AMsearchPage(fileDesc, curHdr->nextLeaf, attrType, attrLength, value, eql, Left, searchRet);
            return AME_INVALIDSCANDESC;
        }
        else
        {
            if ((msg = PF_UnpinPage(AMitab[fileDesc].PFfd, pagenum, FALSE)) < 0)
                return AME_PF;
            searchRet->pagenum = pagenum;
            searchRet->index = keyInd;
            return AME_OK;
        }
    }
}

int BtreeDelete(const int fileDesc, const int pagenum, const char attrType, const int attrLength, const char *value, const RECID recID)
{
    /*finds and mark as deleted the left most equal value*/
    int msg;
    int keyInd;
    char *keyPtr, *childPtr;
    char *pagebuf;
    Nodehdr *curHdr;
    LeafHdr *curLeafHdr;
    AMsearchPtr curSearch;

    if ((msg = PF_GetThisPage(AMitab[fileDesc].PFfd, pagenum, &pagebuf)) < 0)
        return AME_PF;

    curHdr = (Nodehdr *)pagebuf;
    keyInd = 0;
    keyPtr = keyLoc((char *)curHdr, fileDesc, curHdr->isLeaf);

    if (curHdr->numKeys != 0)
        while (keyInd < curHdr->numKeys && AMcmp(value, keyPtr + attrLength * keyInd, attrType, attrLength) > 0)
            keyInd++;

    if (curHdr->isLeaf)
    { /*isLeaf*/
        if (curHdr->numKeys == 0)
        {
            if ((msg = PF_UnpinPage(AMitab[fileDesc].PFfd, pagenum, TRUE)) < 0)
                return AME_PF;
            return AME_OK;

            curLeafHdr = (LeafHdr *)pagebuf;
            if (curLeafHdr->nextLeaf == NULL_PAGE)
                return AME_KEYNOTFOUND;
            curSearch.pagenum = curLeafHdr->nextLeaf;
            curSearch.index = -1;
            while ((msg = findNext(EQ_OP, fileDesc, curSearch.pagenum, value, &curSearch)) > 0)
            {
                if ((msg = PF_GetThisPage(AMitab[fileDesc].PFfd, curSearch.pagenum, &pagebuf)) < 0)
                    return AME_PF;
                curHdr = (Nodehdr *)pagebuf;
                keyPtr = keyLoc((char *)curHdr, fileDesc, curHdr->isLeaf);
                childPtr = childLoc((char *)curHdr, fileDesc, attrLength, TRUE);
                keyInd = curSearch.index;
                if (recID.pagenum == ((RECID *)childPtr)[curSearch.index].pagenum && recID.recnum == ((RECID *)childPtr)[curSearch.index].recnum)
                    break;
                if ((msg = PF_UnpinPage(AMitab[fileDesc].PFfd, curSearch.pagenum, FALSE)) < 0)
                    return AME_PF;
            }
            if (msg < 0)
                return msg;
        }

        childPtr = childLoc((char *)curHdr, fileDesc, attrLength, TRUE);
        printf("value1: %.*s\n", attrLength, value);
        printf("value2: %.*s\n", attrLength, keyPtr + attrLength * keyInd);

        while (keyInd < curHdr->numKeys && AMcmp(value, keyPtr + attrLength * keyInd, attrType, attrLength) == 0)
        {
            printf("found value\n");
            printf("recid: %d %d\n", recID.pagenum, recID.recnum);
            printf("curid: %d %d\n", ((RECID *)childPtr)[keyInd].pagenum, ((RECID *)childPtr)[keyInd].recnum);

            if (recID.pagenum == ((RECID *)childPtr)[keyInd].pagenum && recID.recnum == ((RECID *)childPtr)[keyInd].recnum)
            {
                printf("found rec match\n");
                printf("index: %d\n", keyInd);
                memmove(keyPtr + attrLength * (keyInd), keyPtr + attrLength * (keyInd + 1), attrLength * (curHdr->numKeys - keyInd - 1));
                memmove(childPtr + sizeof(RECID) * keyInd, childPtr + sizeof(RECID) * (keyInd + 1), sizeof(RECID) * (curHdr->numKeys - keyInd - 1));

                /*((RECID*)childPtr)[keyInd].pagenum = -1;*/
                curHdr->numKeys--;
                if ((msg = PF_UnpinPage(AMitab[fileDesc].PFfd, pagenum, TRUE)) < 0)
                    return AME_PF;
                return AME_OK;
            }
            keyInd++;
        }

        printf("didn't find value\n");

        if ((msg = PF_UnpinPage(AMitab[fileDesc].PFfd, pagenum, FALSE)) < 0)
            return AME_PF;
        return AME_RECNOTFOUND;
    }
    else
    {
        if (keyInd < curHdr->numKeys && AMcmp(value, keyPtr + attrLength * keyInd, attrType, attrLength) == 0)
            keyInd++;

        /*
        if (keyInd < curHdr->numKeys && AMcmp(value, keyPtr + attrLength * keyInd, attrType, attrLength) == 0)
            keyInd++;
        */
        childPtr = childLoc((char *)curHdr, fileDesc, attrLength, FALSE);

        if ((msg = PF_UnpinPage(AMitab[fileDesc].PFfd, pagenum, FALSE)) < 0)
            return AME_PF;

        if ((msg = BtreeDelete(fileDesc, ((int *)childPtr)[keyInd], attrType, attrLength, value, recID)) < 0)
            return msg;

        return AME_OK;
    }
}
