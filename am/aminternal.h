#ifndef AMINTERNAL_H
#define AMINTERNAL_H

#include "../h/am.h"

#define NULL_PAGE 0
#define INDEX_END -2

typedef struct AMhdr_str{
    char     attrType;
    int     attrLength;
    bool_t  isUnique;
    int     totalNumRec;
    int     rootPtr;
    int     maxFanoutInter; /*max num of children*/
    int     maxFanoutLeaf;  /*max num of children*/
} AMhdr_str;

typedef struct Nodehdr {
    bool_t  isLeaf;
    int     numKeys;
    /*int     parent;*/
    /*int     nextLeaf;   only for leaf*/
} Nodehdr;

typedef struct InterHdr{
    Nodehdr hdr;
} InterHdr;

typedef struct LeafHdr{
    Nodehdr hdr;
    int prevLeaf;
    int nextLeaf;
} LeafHdr;

typedef struct AMitab_ele{
    bool_t      valid;
    AMhdr_str   AMhdr;
    short       hdrchanged;
    int         PFfd;
    int         numScan;
    char        fileName[50];

    struct AMitab_ele  *next_ele;
} AMitab_ele;

extern AMitab_ele *AMitab;
extern AMitab_ele *freeAMHead;
extern int numAMitab_ele;

typedef struct AMsearchPtr{
    int pagenum;
    int index;
    RECID lastRetrived;
} AMsearchPtr;

typedef struct AMScantab_ele{
    bool_t  valid;
    int     AMfd;
    int     op;
    char    value[255];
    bool_t  ifInit;

    AMsearchPtr curSearch;

    struct AMScantab_ele   *next_ele;
} AMScantab_ele;

extern AMScantab_ele *AMScantab;
extern AMScantab_ele *freeAMScanHead;
extern int numAMScan_ele;

/*
#define MAXSTR 255
typedef struct AMrec_str{
    char string_val[MAXSTR];
    float flaot_val;
    int int_val;
} AMrec_str;
*/


/*int BtreeInsert(const int fileDesc,const int pagenum,const char attrType,
                const int attrLength,const char *value,const RECID recID, char* buf);*/
int AMcmp(const char *value1,const char *value2,const char attrType,const int attrLength);
bool_t ifRetrived(AMsearchPtr *searchPtr,RECID curId);
bool_t ifSatisOp(const int op, const char *value1, const char *value2, const char attrType, const int attrLength);
char *keyLoc(char *pagebuf, const int fileDesc, bool_t isLeaf);
char *childLoc(char *pagebuf, const int fileDesc, const int attrLength, bool_t isLeaf);
int BtreeIns(const int fileDesc, const char attrType, const int attrLength, const char *value, const RECID recID);
int BtreeDelete(const int fileDesc,const int pagenum,const char attrType,
                const int attrLength,const char *value,const RECID recId);
int BtreeSearchOp(const int op, const int fileDesc, const int pagenum,const char *value, AMsearchPtr *searchRet);
int BtreeLeftMost(const int fileDesc, const int pagenum, AMsearchPtr *searchRet);
int findNext(const int op, const int fileDesc, const int pagenum,const char *value, AMsearchPtr *searchPtr);
bool_t isNodeFull(Nodehdr *curHdr, int fileDesc);
#endif
