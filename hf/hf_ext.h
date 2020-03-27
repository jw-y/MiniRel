#include "hfinternal.h"
int HF_totalNumRec(int fileDesc){
    if(!HFftab[fileDesc].valid)
        return (HFerrno = HFE_FD);
    return HFftab[fileDesc].HFhdr.totalNumRec;
}