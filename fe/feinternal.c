#include "feinternal.h"

int findRelcat(const char *relName, RELDESCTYPE *relStrPtr, RECID *relrecidPtr){
    int hfsd;

    if((hfsd = HF_OpenFileScan(relcatFd, STRING_TYPE, strlen(relName), relCatOffset(relname),
                    EQ_OP, (char *)relName))<0)
        return FEE_HF;
    
    *relrecidPtr = HF_FindNextRec(hfsd, (char *)relStrPtr);

    if (HF_CloseFileScan(hfsd) < 0)
        return FEE_HF;

    if(!HF_ValidRecId(relcatFd, *relrecidPtr))
        return FEE_NOSUCHREL;

    return FEE_OK;
}

int findAttrcat(const char *relName, const char *attrName, ATTRDESCTYPE *attrStrPtr, RECID *attrrecidPtr){
    int hfsd;

    if((hfsd = HF_OpenFileScan(attrcatFd, STRING_TYPE, strlen(relName), attrCatOffset(relname), 
                    EQ_OP, (char*)relName))<0)
        return FEE_HF;

    *attrrecidPtr = HF_FindNextRec(hfsd, (char*)attrStrPtr);
    while(HF_ValidRecId(attrcatFd, *attrrecidPtr)){
        if(strcmp(attrStrPtr->attrname, attrName)==0)/*found*/
            break;

        *attrrecidPtr = HF_FindNextRec(hfsd, (char*)attrStrPtr);
    }
    if(HF_CloseFileScan(hfsd)<0)
        return FEE_HF;

    if(!HF_ValidRecId(attrcatFd, *attrrecidPtr))
        return FEE_NOSUCHATTR;

    return FEE_OK;
}
    
int getAttrcat(const char *relName, ATTRDESCTYPE **attrStrArrPtr, int attrcnt){
    /*must free memory after*/
    int i, hfsd;
    RECID attrrecid;
    ATTRDESCTYPE attrStr;

    *attrStrArrPtr = (ATTRDESCTYPE *) malloc(ATTRDESCSIZE * attrcnt);

    if( (hfsd = HF_OpenFileScan(attrcatFd, STRING_TYPE, strlen(relName), attrCatOffset(relname),
                                EQ_OP, (char*)relName )) < 0 )
        return FEE_HF;

    for(i=0; i<attrcnt; i++){
        attrrecid = HF_FindNextRec(hfsd, (char *)&attrStr);
        if(!HF_ValidRecId(attrcatFd, attrrecid))
            return FEE_NOSUCHATTR;
        memcpy((*attrStrArrPtr)+attrStr.attrno, &attrStr, ATTRDESCSIZE);
    }
    if(HF_CloseFileScan(hfsd)<0)
        return FEE_HF;
    
    return FEE_OK;
}
int findAttrVal(const char *attrName,const ATTR_VAL values[], const int numAttrs){
    int i;
    for(i=0; i<numAttrs; i++)
        if(strcmp(attrName, values[i].attrName)==0)
            return i;
    return -1;
}
int findAttrName(const char *attrName, const ATTRDESCTYPE *attrStrArr, const int attrcnt){
    int i;
    for(i=0; i<attrcnt; i++){
        if(strcmp(attrName, attrStrArr[i].attrname)==0)
            return i;
    }
    return -1;
}

int openHFAM(const char *relName, int *hffd, int **amfd, const int attrcnt, const ATTRDESCTYPE *attrStrArr){
    int i;
    *amfd = malloc(sizeof(int)*attrcnt);

    if((*hffd=HF_OpenFile(relName))<0)
        return FEE_HF;
    
    for(i=0; i<attrcnt; i++){
        if(attrStrArr[i].indexed)
            if(((*amfd)[i] = AM_OpenIndex(relName, attrStrArr[i].attrno))<0)
                return FEE_AM;
    }
    return FEE_OK;
}

int closeHFAM(const int hffd, int *amfd, const int attrcnt, const ATTRDESCTYPE *attrStrArr){
    int i;
    if(HF_CloseFile(hffd)<0)
        return FEE_HF;
    
    for(i=0; i<attrcnt; i++){
        if(attrStrArr[i].indexed)
            if(AM_CloseIndex(amfd[i])<0)
                return FEE_AM;
    }

    free(amfd);
    return FEE_OK;
}

int openHFAMScan(int *hfsd, int **amsd, const int hffd, const int *amfd, const int attrcnt, const ATTRDESCTYPE *attrStrArr,
                 const int valType, const int valLength, const int offset, const int op, char *value){
    int i;
    *amsd = malloc(sizeof(int)*attrcnt);

    if((*hfsd = HF_OpenFileScan(hffd, valType, valLength, offset, op, value))<0)
        return FEE_HF;
    
    for(i=0; i<attrcnt; i++){
        if(attrStrArr[i].indexed)
            if(((*amsd)[i] = AM_OpenIndexScan(amfd[i], op, value))<0)
                return FEE_AM;
    }
    return FEE_OK;
}

int closeHFAMScan(const int hfsd, int *amsd, const int attrcnt, const ATTRDESCTYPE *attrStrArr){
    int i;
    if(HF_CloseFileScan(hfsd)<0)
        return FEE_HF;

    for(i=0; i<attrcnt; i++){
        if(attrStrArr[i].indexed)
            if(AM_CloseIndex(amsd[i])<0)
                return FEE_HF;
    }
    free(amsd);
    return FEE_OK;
}

void updateAttrVal(ATTR_VAL *attrVal, const int numProjAttrs, char *projAttrs[], const int *projInd, 
                    const ATTRDESCTYPE *attrStrArr, char *record){
    int i;
    for(i=0; i<numProjAttrs; i++){
        attrVal[i].attrName = projAttrs[i];
        attrVal[i].valType = attrStrArr[projInd[i]].attrtype;
        attrVal[i].valLength = attrStrArr[projInd[i]].attrlen;
        attrVal[i].value = record + attrStrArr[projInd[i]].offset;
    }
}
void updateAttrValJoin(ATTR_VAL *attrVal, const int numPorjAttrs, REL_ATTR projAttrs[], const pii *projInd, 
                        const ATTRDESCTYPE *attrStrArr1, const ATTRDESCTYPE *attrStrArr2, char *record1, char *record2){
    int i, j;
    for(i=0; i<numPorjAttrs; i++){
        j = projInd[i].second;
        if(projInd[i].front==0){ /* attr 1*/
            attrVal[i].attrName = projAttrs[i].attrName;
            attrVal[i].valType = attrStrArr1[j].attrtype;
            attrVal[i].valLength = attrStrArr1[j].attrlen;
            attrVal[i].value = record1 + attrStrArr1[j].offset;
        } else { /*attr 2*/
            attrVal[i].attrName = projAttrs[i].attrName;
            attrVal[i].valType = attrStrArr2[j].attrtype;
            attrVal[i].valLength = attrStrArr2[j].attrlen;
            attrVal[i].value = record2 + attrStrArr2[j].offset;
        }
    }
}

void printRecord(const int numProjAttrs, char *projAttrs[], const int *projInd, const ATTRDESCTYPE *attrStrArr, const char *record){
    int i, j;
    int attrtype;
    for(i=0; i<numProjAttrs; i++){
        j = projInd[i];
        if(strcmp(attrStrArr[j].attrname, "type")==0){
            attrtype = *(int*)(record + attrStrArr[j].offset);
            printf("| %-12s ", attrtype == 'c' ? "string": (attrtype == 'f' ? "real" :
                        (attrtype=='i'? "integer" : "boolean")) );
        }else if(attrStrArr[j].attrtype==INT_TYPE){
            printf("| %12d ", *(int*)(record + attrStrArr[j].offset));
        }else if(attrStrArr[j].attrtype==REAL_TYPE){
            printf("| %12f ", *(float*)(record + attrStrArr[j].offset));
        }else if(attrStrArr[j].attrtype==STRING_TYPE){ /*string type*/
            printf("| %-*s ", max(12, attrStrArr[j].attrlen),record + attrStrArr[j].offset);
        }else { /*bool type*/
            printf("| %-12s ", *(bool_t*)(record + attrStrArr[j].offset) ? "yes": "no");
        }
    }
    printf("|\n");
}

void printRecordAttr(const ATTR_VAL *attrVal, const int numPorjAttrs){
    int i, attrtype;
    for(i=0; i<numPorjAttrs; i++){
        if(strcmp(attrVal[i].attrName, "type")==0){
            attrtype = *(int*)(attrVal[i].value);
            printf("| %-12s ", attrtype == 'c' ? "string": (attrtype == 'f' ? "real" :
                        (attrtype=='i'? "integer" : "boolean")) );
        }else if(attrVal[i].valType==INT_TYPE){
            printf("| %12d ", *(int*)(attrVal[i].value));
        }else if(attrVal[i].valType==REAL_TYPE){
            printf("| %12f ", *(float*)(attrVal[i].value));
        }else if(attrVal[i].valType==STRING_TYPE){ /*string type*/
            printf("| %-*s ", max(12, attrVal[i].valLength), attrVal[i].value);
        }else { /*bool type*/
            printf("| %-12s ", *(bool_t*)(attrVal[i].value) ? "yes": "no");
        }
    }
    printf("|\n");
}
