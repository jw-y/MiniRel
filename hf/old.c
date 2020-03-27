RECID HF_GetFirstRec(int fileDesc, char *record){
    int msg;
    char *pagebuf;
    RECID recID = {-1, -1};
    HFftab_ele *curEle;
    PageHdr *curHdr;
    int curPagenum;

    /*Check to see if file exists*/
    if (!HFftab[fileDesc].valid)
        return recSetErr(HFE_FD);
    /*Check to see if file has pages and records*/
    if ((HFftab[fileDesc].HFhdr.numDataPages == 0) || (HFftab[fileDesc].HFhdr.totalNumRec == 0))
        return recSetErr(HFE_EOF);

    curEle = &(HFftab[fileDesc]);

    if(curEle->HFhdr.fullPagePtr != NULLPAGE){
        printf("fullpage present!\n");

        curPagenum = curEle->HFhdr.fullPagePtr;
        if ((msg = PF_GetThisPage(curEle->PFfd, curPagenum, &pagebuf)) < 0)
            return recSetErr(HFE_PF);
        if ((msg = PF_UnpinPage(curEle->PFfd, curPagenum, FALSE)) < 0)
            return recSetErr(HFE_PF);
        recID.pagenum = curPagenum;
        recID.recnum = 0;
        memcpy(record, HFrecLoc(pagebuf, 0, curEle->HFhdr.RecSize), curEle->HFhdr.RecSize);
    } else {
        curPagenum = curEle->HFhdr.freePagePtr;
        while(1){
            if ((msg = PF_GetThisPage(curEle->PFfd, curPagenum, &pagebuf)) < 0)
                return recSetErr(HFE_PF);
            curHdr = (PageHdr *)pagebuf;
            if ((msg = PF_UnpinPage(curEle->PFfd, curPagenum, FALSE)) < 0)
                return recSetErr(HFE_PF);
            if (curHdr->numRec != 0)
                break;
            if (curHdr->nextPage == NULLPAGE)
                break;
            curPagenum = curHdr->nextPage;
        }
        recID.pagenum = curPagenum;
        recID.recnum = HFnextFullSlot(pagebuf, -1, curEle->HFhdr.numRecPerPage);
        memcpy(record, HFrecLoc(pagebuf, recID.recnum, curEle->HFhdr.RecSize), curEle->HFhdr.RecSize);
    }

    return recID;
}

RECID HF_GetNextRec(int fileDesc, RECID recId, char *record){
    int msg;
    HFftab_ele *curEle;
    char *pagebuf;
    int nextSlot, curPagenum;
    RECID retID;
    PageHdr *curHdr;
    bool_t ifFound = FALSE;

    //printf("next of p: %d, r: %d\n", recId.pagenum, recId.recnum);

    if(!HFftab[fileDesc].valid)
        return recSetErr(HFE_FD);

    curEle = &(HFftab[fileDesc]);

    if (!HF_ValidRecId(fileDesc, recId))
        return recSetErr(HFE_INVALIDRECORD);

    curPagenum = recId.pagenum;

    if((msg=PF_GetThisPage(curEle->PFfd, curPagenum, &pagebuf))<0)
        return recSetErr(HFE_PF);
    if ((msg = PF_UnpinPage(curEle->PFfd, curPagenum, FALSE)) < 0)
        return recSetErr(HFE_PF);
    curHdr = (PageHdr*)pagebuf;

    //printf("numrec: %d\n",curHdr->numRec);

    if(curHdr->numRec==0 || (nextSlot=HFnextFullSlot(pagebuf, recId.recnum, curEle->HFhdr.numRecPerPage))<0){
        if(curHdr->numRec >= curEle->HFhdr.numRecPerPage){ //if fullpage
            if(curHdr->nextPage != NULLPAGE){
                printf("next page full\n");
                ifFound = TRUE;
                curPagenum = curHdr->nextPage;
                if ((msg = PF_GetThisPage(curEle->PFfd, curPagenum, &pagebuf)) < 0)
                    return recSetErr(HFE_PF);
                if ((msg = PF_UnpinPage(curEle->PFfd, curPagenum, FALSE)) < 0)
                    return recSetErr(HFE_PF);
                curHdr = (PageHdr*)pagebuf;
                retID.pagenum = curPagenum;
                retID.recnum = 0;
            } else { //find in freepage
                ifFound = FALSE;
                if(curEle->HFhdr.freePagePtr == NULLPAGE)
                    return recSetErr(HFE_EOF);
                curPagenum = curEle->HFhdr.freePagePtr;
            }
        } else {
            ifFound = FALSE;
            if(curHdr->nextPage==NULLPAGE)
                return recSetErr(HFE_EOF);
            curPagenum = curHdr->nextPage;
        }
    } else {
        ifFound = TRUE;
        retID.pagenum = recId.pagenum;
        retID.recnum = nextSlot;
    }
    
    if(!ifFound){ //find in freepage
        printf("freepage\n");
        if ((msg = PF_GetThisPage(curEle->PFfd, curPagenum, &pagebuf)) < 0)
            return recSetErr(HFE_PF);
        if ((msg = PF_UnpinPage(curEle->PFfd, curPagenum, FALSE)) < 0)
            return recSetErr(HFE_PF);
            
        curHdr = (PageHdr*)pagebuf;

        while(1){
            if(curHdr->numRec!=0)
                break;
            if (curHdr->nextPage == NULLPAGE) //eof
                return recSetErr(HFE_EOF);
            curPagenum = curHdr->nextPage;
            printf("curPagenum: %d\n", curPagenum);

            if ((msg = PF_GetThisPage(curEle->PFfd, curPagenum, &pagebuf)) < 0)
                return recSetErr(HFE_PF);
            if ((msg = PF_UnpinPage(curEle->PFfd, curPagenum, FALSE)) < 0)
                return recSetErr(HFE_PF);
            curHdr = (PageHdr *)pagebuf;
        }

        nextSlot = HFnextFullSlot(pagebuf, -1, curEle->HFhdr.numRecPerPage);
        retID.pagenum = curPagenum;
        retID.recnum = nextSlot;
    }
        

    memcpy(record, HFrecLoc(pagebuf, retID.recnum, curEle->HFhdr.RecSize), curEle->HFhdr.RecSize);

    return retID;
}

int HF_OpenFileScan(int fileDesc, char attrType, int attrLength,
                    int attrOffset, int op, const char *value){
    HFScantab_ele *newScanEle;

    if(numHFScan_ele >= MAXSCANS)
        return setErr(HFE_STABFULL);

    printf("opening fileScan\n");
    
    /*if(isInHFScanHash(fileDesc))
        return setErr(HFE_SCANOPEN);*/
    
    if(!HFftab[fileDesc].valid)
        return setErr(HFE_FILENOTOPEN);

    if (HFftab[fileDesc].HFhdr.RecSize <= attrOffset || attrOffset < 0)
        return setErr(HFE_ATTROFFSET);

    if(op<1 || op>6)
        return setErr(HFE_OPERATOR);

    if(attrType=='c'){
        if(attrLength < 1 || attrLength > 255)
            return setErr(HFE_ATTRLENGTH);
    } else if(attrType=='i'){
        if(attrLength != 4)
            return setErr(HFE_ATTRLENGTH);
    } else if(attrType=='f'){
        if(attrLength != 4)
            return setErr(HFE_ATTRLENGTH);
    } else
        return setErr(HFE_ATTRTYPE);

    if(value==NULL)
        op = ALL_OP;

    newScanEle = freeHFScanHead;
    freeHFScanHead = freeHFScanHead->next_ele; 

    newScanEle->valid = TRUE;
    newScanEle->HFfd = fileDesc;
    newScanEle->attrType = attrType;
    newScanEle->attrLength = attrLength;
    newScanEle->attrOffset = attrOffset;
    newScanEle->op = op;
    /*newScanEle->value = value;*/
    if(value!=NULL)
        memcpy(newScanEle->value, value, attrLength);
    
    if(HFftab[fileDesc].HFhdr.fullPagePtr != NULLPAGE){
        newScanEle->searchPtr.pagePtr = HFftab[fileDesc].HFhdr.fullPagePtr;
        newScanEle->searchPtr.isFull = TRUE;
    }
    else {
        newScanEle->searchPtr.pagePtr = HFftab[fileDesc].HFhdr.freePagePtr;
        newScanEle->searchPtr.isFull = FALSE;
    }
    newScanEle->searchPtr.index = -1;
    
    HFftab[fileDesc].numScan++;

    numHFScan_ele++;

    /*HFScanAddToHash(newScanEle);*/

    return newScanEle - HFScantab;
}

RECID HF_FindNextRec(int scanDesc, char *record){
    int msg;
    HFScantab_ele *curScanEle;
    RECID retRec;
    int HFfd = HFScantab[scanDesc].HFfd;

    if (!HFScantab[scanDesc].valid) 
        return recSetErr(HFE_SD);

    curScanEle = &HFScantab[scanDesc];

    if(curScanEle->searchPtr.isFull){
        if((msg=HFfindnextInner(TRUE, curScanEle->HFfd, curScanEle->searchPtr.pagePtr, scanDesc, &(curScanEle->searchPtr),&retRec, record))<0){
            if(msg!=HFE_EOF)
                return recSetErr(msg);
            if(HFftab[HFfd].HFhdr.freePagePtr == NULLPAGE)
                return recSetErr(HFE_EOF);
            curScanEle->searchPtr.pagePtr = HFftab[HFScantab[scanDesc].HFfd].HFhdr.freePagePtr;
            curScanEle->searchPtr.isFull = FALSE;
            curScanEle->searchPtr.index = -1;
            if ((msg = HFfindnextInner(FALSE, curScanEle->HFfd, curScanEle->searchPtr.pagePtr, scanDesc, &(curScanEle->searchPtr), &retRec, record)) < 0)
                return recSetErr(msg);
        }
    } else {
        if ((msg = HFfindnextInner(FALSE, curScanEle->HFfd, curScanEle->searchPtr.pagePtr, scanDesc, &(curScanEle->searchPtr), &retRec, record)) < 0)
            return recSetErr(msg);
    }

    return retRec;
}

int HFfindnextInner(const bool_t isFull, const int HFfd, const int pagenum, const int HFScanfd, HFsearchPtr *searchPtr, RECID *retRec, char* record){
    int msg;
    char *pagebuf;
    PageHdr *curHdr;
    HFScantab_ele *curScanEle = &(HFScantab[HFScanfd]);
    HFftab_ele *curtabEle = &(HFftab[HFfd]);

    if((msg=PF_GetThisPage(curtabEle->PFfd, pagenum, &pagebuf))<0)
        return HFE_PF;
    if ((msg = PF_UnpinPage(curtabEle->PFfd, pagenum, FALSE)) < 0)
        return HFE_PF;

    curHdr = (PageHdr*) pagebuf;
    
    searchPtr->index = incInd(searchPtr->index, isFull, pagebuf, HFftab[HFfd].HFhdr.numRecPerPage);

    printf("index: %d, totalrec: %d\n", searchPtr->index, curtabEle->HFhdr.numRecPerPage);

    while((searchPtr->index >=0  && searchPtr->index<curtabEle->HFhdr.numRecPerPage) &&
         !isSatisOp(curScanEle->op, curScanEle->value, HFrecLoc(pagebuf, searchPtr->index, curtabEle->HFhdr.RecSize)+curScanEle->attrOffset, 
                             curScanEle->attrType, curScanEle->attrLength)){
        searchPtr->index = incInd(searchPtr->index, isFull, pagebuf, HFftab[HFfd].HFhdr.numRecPerPage);
    }

    if ((searchPtr->index >=0  && searchPtr->index<curtabEle->HFhdr.numRecPerPage) &&
        isSatisOp(curScanEle->op, curScanEle->value, HFrecLoc(pagebuf, searchPtr->index, curtabEle->HFhdr.RecSize) + curScanEle->attrOffset, 
                            curScanEle->attrType, curScanEle->attrLength)){
        memcpy(record, HFrecLoc(pagebuf, searchPtr->index, curtabEle->HFhdr.RecSize), curtabEle->HFhdr.RecSize);
        retRec->pagenum = pagenum;
        retRec->recnum = searchPtr->index;
        return HFE_OK;
    } else{
    /*if(searchPtr->index >= curtabEle->HFhdr.numRecPerPage){*/
        printf("doesn't satisfy... index: %d\n", searchPtr->index);
        if(curHdr->nextPage != NULLPAGE){
            searchPtr->index = -1;
            searchPtr->pagePtr = curHdr->nextPage;
            return HFfindnextInner(isFull, HFfd, curHdr->nextPage, HFScanfd, searchPtr, retRec, record);
        }
        return HFE_EOF;
    }
}