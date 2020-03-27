#ifndef FEINTERNAL_H
#define FEINTERNAL_H

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "../h/minirel.h"
#include "../h/fe.h"
#include "../h/am.h"
#include "../h/hf.h"
#include "../h/catalog.h"

#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))

typedef struct pii{
    int front; int second;
} pii;

int findRelcat(const char *relName, RELDESCTYPE *relStrPtr, RECID *relrecidPtr);
int findAttrcat(const char *relName, const char *attrName, ATTRDESCTYPE *attrStrPtr, RECID *attrrecid);
int getAttrcat(const char *relName, ATTRDESCTYPE **attrStrArrPtr, int attrcnt);
int findAttrVal(const char *attrName, const ATTR_VAL values[], const int numAttrs);
int findAttrName(const char *attrName, const ATTRDESCTYPE *attrStrArr, const int attrcnt);
int openHFAM(const char *relName, int *hffd, int **amfd, const int attrcnt, const ATTRDESCTYPE *attrStrArr);
int closeHFAM(const int hffd, int *amfd, const int attrcnt, const ATTRDESCTYPE *attrStrArr);
int openHFAMScan(int *hfsd, int **amsd, const int hffd, const int *amfd, const int attrcnt, const ATTRDESCTYPE *attrStrArr,
                 const int valType, const int valLength, const int offset, const int op, char *value);
int closeHFAMScan(const int hfsd, int *amsd, const int attrcnt, const ATTRDESCTYPE *attrStrArr);
void updateAttrVal(ATTR_VAL *attrVal, const int numProjAttrs, char *projAttrs[], 
                    const int *projInd, const ATTRDESCTYPE *attrStrArr, char *record);
void printRecord(const int numProjAttrs, char *projAttrs[], const int *projInd, 
                            const ATTRDESCTYPE *attrStrArr, const char *record);
void updateAttrValJoin(ATTR_VAL *attrVal, const int numPorjAttrs, REL_ATTR projAttrs[], const pii *projInd, 
                        const ATTRDESCTYPE *attrStrArr1, const ATTRDESCTYPE *attrStrArr2, char *record1, char *record2);
void printRecordAttr(const ATTR_VAL *attrVal, const int numPorjAttrs);

#endif
