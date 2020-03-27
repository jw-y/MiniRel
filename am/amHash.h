#include "../h/am.h"
#include "aminternal.h"

typedef struct AMhash_entry{
    struct AMhash_entry *nextentry;
    struct AMhash_entry *preventry;
    struct AMitab_ele *AM_ele;
} AMhash_entry;

#define AM_HASH_TBL_SIZE (2 * AM_ITAB_SIZE)
extern AMhash_entry *AMhashT[AM_HASH_TBL_SIZE];
extern AMhash_entry AMfreeHash[AM_ITAB_SIZE];
extern AMhash_entry *AMfreeHashHead;

void AMhashInit();
bool_t isInAMHash(const char *filename);
void AMaddToHash(AMitab_ele *AM_ele);
void AMremoveFromHash(AMitab_ele *AM_ele);

