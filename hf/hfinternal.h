#ifndef HFINTERNAL_H
#define HFINTERNAL_H

#include "../h/hf.h"

#define BYTE_SIZE 8
#define NULLPAGE 0

typedef struct HFhdr_str {
    int RecSize;
    int numRecPerPage;
    int numDataPages;
    int totalNumRec;
    int freePagePtr;
    int fullPagePtr;
} HFhdr_str;

typedef struct PageHdr {
    int numRec;
    int prevPage;
    int nextPage;
} PageHdr;

typedef struct HFftab_ele {
    bool_t      valid;
    HFhdr_str   HFhdr;
    short       hdrchanged;
    int         PFfd;
    int         numScan;
    char        fileName[50];

    struct HFftab_ele  *next_ele;
    /*HFScantab_ele *scanHead;*/
} HFftab_ele;

extern int HF_PAGE_NUM;

extern HFftab_ele *HFftab;
extern HFftab_ele *freeHFHead;
extern int numHFftab_ele;

typedef struct HFsearchPtr{
    int pagePtr;
    bool_t isFull;
    int index;
} HFsearchPtr;

typedef struct HFScantab_ele{
    bool_t  valid;
    int     HFfd;
    char    attrType;
    int     attrLength;
    int     attrOffset;
    int     op;
    char    value[255];
    RECID   lastRet;

    HFsearchPtr searchPtr;

    struct HFScantab_ele *next_ele;
} HFScantab_ele;

extern HFScantab_ele *HFScantab;
extern HFScantab_ele *freeHFScanHead;
extern int numHFScan_ele;

void setBitmap(char *pagebuf, const int index);
void unsetBitmap(char *pagebuf, const int index);
bool_t ifRecPresent(char *pagebuf, const int index, const int numRecPerPage);
int HFnextFullSlot(char *pagebuf, const int index, const int numRecPerPage);
char *HFrecLoc(char *pagebuf, const int index, const int RecSize);
int HFgetEmptySlot(char *pagebuf, int numRecPerPage);
int HFfindNextRecInner(const int HFfd, RECID *lastRet, HFScantab_ele* curScanEle, char* record);

#endif
