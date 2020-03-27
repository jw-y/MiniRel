#include "../h/hf.h"
#include "hfinternal.h"

typedef struct HFhash_entry{
    struct HFhash_entry *nextentry;
    struct HFhash_entry *preventry;
    struct HFftab_ele *HF_ele;
} HFhash_entry;

#define HF_HASH_TBL_SIZE (2 * HF_FTAB_SIZE)
extern HFhash_entry *HFhashT[HF_HASH_TBL_SIZE];
extern HFhash_entry HFfreeHash[HF_FTAB_SIZE];
extern HFhash_entry *HFfreeHashHead;

void HFhashInit();
bool_t isInHFHash(const char *filename);
void HFaddToHash(HFftab_ele *HF_ele);
void HFremoveFromHash(HFftab_ele *HF_ele);

