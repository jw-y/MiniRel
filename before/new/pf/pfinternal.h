#ifndef PFINTERNAL_H
#define PFINTERNAL_H

#include <unistd.h>
#include <sys/types.h>

#include "minirel.h"
#include "pf.h"

typedef struct PFhdr_str {
    int    numpages;      /* number of pages in the file */
} PFhdr_str;

typedef struct PFftab_ele {
    bool_t    valid;       /* set to TRUE when a file is open. */
    ino_t     inode;       /* inode number of the file         */
    char      fname[50];      /* file name                        */
    int       unixfd;      /* Unix file descriptor             */
    PFhdr_str hdr;         /* file header                      */
    short     hdrchanged;  /* TRUE if file header has changed  */

    struct PFftab_ele *next_ele;
} PFftab_ele;

extern PFftab_ele *PFftab;
extern PFftab_ele *freePFHead;
extern int numPFftab_ele;

typedef struct PFhash_entry{
    struct PFhash_entry *nextentry;
    struct PFhash_entry *preventry;
    struct PFftab_ele *PF_ele;
} PFhash_entry;

#define PF_HASH_TBL_SIZE (2 * MAXOPENFILES)
extern PFhash_entry *PFhashT[PF_HASH_TBL_SIZE];
extern PFhash_entry PFfreeHash[MAXOPENFILES];
extern PFhash_entry *PFfreeHashHead;

#endif
