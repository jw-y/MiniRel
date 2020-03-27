#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>

#include "../h/minirel.h"
#include "../h/fe.h"
#include "../h/am.h"
#include "../h/hf.h"
#include "../h/pf.h"
#include "../h/catalog.h"

#include "../hf/hf_ext.h"

#include "feinternal.h"

int FEerrno = 1;
int relcatFd = -1;
int attrcatFd = -1;

bool_t isDBOpen = FALSE;
char dbOpenName[MAXNAME];


char RELDESCNAME[RELDESCSIZE][20] = {"relname", "relwid", "attrcnt", "indexcnt", "primattr"};
int RELDESCOFFSET[RELDESCSIZE] = {relCatOffset(relname), relCatOffset(relwid), relCatOffset(attrcnt),
                            relCatOffset(indexcnt), relCatOffset(primattr)};
int RELDESCATTRLEN[RELDESCSIZE] = {MAXNAME, sizeof(int), sizeof(int), sizeof(int), MAXNAME};
int RELDESCATTRTYPE[RELDESCSIZE] = {STRING_TYPE, INT_TYPE, INT_TYPE, INT_TYPE, STRING_TYPE};

char ATTRDESCNAME[ATTRDESCSIZE][20] = {"relname", "attrname", "offset", "length",
                                     "type", "indexed", "attrno"};
int ATTRDESCOFFSET[ATTRDESCSIZE] = {attrCatOffset(relname), attrCatOffset(attrname), attrCatOffset(offset),
                attrCatOffset(attrlen), attrCatOffset(attrtype), attrCatOffset(indexed), attrCatOffset(attrno)};
int ATTRDESCATTRLEN[ATTRDESCSIZE] = {MAXNAME, MAXNAME, sizeof(int), sizeof(int),sizeof(int),
                                    sizeof(bool_t), sizeof(int)};
int ATTRDESCATTRTYPE[ATTRDESCSIZE] = {STRING_TYPE, STRING_TYPE, INT_TYPE, INT_TYPE, INT_TYPE,
                                    BOOL_TYPE, INT_TYPE};

static int setErr(int err){
    return (FEerrno = err);
}

void DBcreate(const char *dbname) {
    int msg, i, hffd;
    RECID recid;
    RELDESCTYPE relDesc;
    ATTRDESCTYPE attrDesc;

    if(mkdir(dbname, 0777)!=0)
        return; 

    if(chdir(dbname)!=0)
        return;

    FE_Init();

    if((msg=HF_CreateFile(RELCATNAME, RELDESCSIZE))<0)
        return;

    if ((msg = HF_CreateFile(ATTRCATNAME, ATTRDESCSIZE)) < 0)
        return;

    /*insert into relcat*/
    if((hffd = HF_OpenFile(RELCATNAME))<0)
        return;

    strcpy(relDesc.relname, RELCATNAME);
    relDesc.relwid = RELDESCSIZE;
    relDesc.attrcnt = RELCAT_NATTRS;
    relDesc.indexcnt = 0;
    strcpy(relDesc.primattr, "relname");

    recid = HF_InsertRec(hffd, (char*)&relDesc);
    if(!HF_ValidRecId(hffd, recid))
        return;

    strcpy(relDesc.relname, ATTRCATNAME);
    relDesc.relwid = ATTRDESCSIZE;
    relDesc.attrcnt = ATTRCAT_NATTRS;
    /*strcpy(relDesc.primattr, ATTRCATNAME);*/

    recid = HF_InsertRec(hffd, (char*)&relDesc);
    if(!HF_ValidRecId(hffd, recid))
        return;

    if(HF_CloseFile(hffd)<0)
        return;

    /*insert into attrcat*/
    if((hffd = HF_OpenFile(ATTRCATNAME))<0)
        return;

    strcpy(attrDesc.relname, RELCATNAME);
    attrDesc.indexed = FALSE;
    
    for(i=0; i<RELCAT_NATTRS; i++){
        strcpy(attrDesc.attrname, RELDESCNAME[i]);
        attrDesc.offset = RELDESCOFFSET[i];
        attrDesc.attrlen = RELDESCATTRLEN[i];
        attrDesc.attrtype = RELDESCATTRTYPE[i];
        attrDesc.attrno = i;

        recid = HF_InsertRec(hffd, (char*)&attrDesc);
        if(!HF_ValidRecId(hffd, recid))
            return;
    }
    
    strcpy(attrDesc.relname, ATTRCATNAME);
    attrDesc.indexed = FALSE;

    for(i=0; i<ATTRCAT_NATTRS; i++){
        strcpy(attrDesc.attrname, ATTRDESCNAME[i]);
        attrDesc.offset = ATTRDESCOFFSET[i];
        attrDesc.attrlen = ATTRDESCATTRLEN[i];
        attrDesc.attrtype = ATTRDESCATTRTYPE[i];
        attrDesc.attrno = i;

        recid = HF_InsertRec(hffd, (char*)&attrDesc);
        if(!HF_ValidRecId(hffd, recid))
            return;
    }

    if(HF_CloseFile(hffd)<0)
        return;
    
    if(chdir("../.")!=0)
        return;

}
void DBdestroy(const char *dbname) {
    char files_to_delete[50];

    sprintf(files_to_delete, "rm -rf %s/*", dbname);
    system(files_to_delete);

    if(rmdir(dbname)!=0)
        return;
}
void DBconnect(const char *dbname) {
    if(isDBOpen)
        return;

    isDBOpen = TRUE;
    strcpy(dbOpenName, dbname);

    if(chdir(dbname)!=0)
        return;
    
    if((relcatFd = HF_OpenFile(RELCATNAME))<0)
        return;
    
    if((attrcatFd = HF_OpenFile(ATTRCATNAME))<0)
        return;
}
void DBclose(const char *dbname) {
    int msg;
    if(!isDBOpen || strcmp(dbname, dbOpenName)!=0)
        return;

    if((msg=HF_CloseFile(relcatFd))<0)
        return;
    
    if((msg=HF_CloseFile(attrcatFd))<0)
        return;
    
    if(chdir("../.")!=0)
        return;

    isDBOpen = FALSE;
}

int CreateTable(const char *relName,       /* name	of relation to create	*/
                int numAttrs,              /* number of attributes		*/
                ATTR_DESCR attrs[],        /* attribute descriptors	*/
                const char *primAttrName)  /* primary index attribute	*/{
    int msg, i, j;
    RELDESCTYPE newRel;
    ATTRDESCTYPE newAttr;
    RECID recid;
    int offset=0;
    RELDESCTYPE relDesc;
    char primattr[MAXNAME];
    char *dotstr = NULL;

    /*if rel exists*/
    if(access(relName, F_OK) != -1)
        return setErr(FEE_RELEXISTS);

    if(strlen(relName) >= MAXNAME)
        return setErr(FEE_RELNAMETOOLONG);

    for(i=0; i<numAttrs; i++)
        if(strlen(attrs[i].attrName) >= MAXNAME)
            return setErr(FEE_ATTRNAMETOOLONG);
    
    /*check duplicate name*/
    for(i=0; i<numAttrs; i++)
        for(j=i+1; j<numAttrs; j++)
            if(strcmp(attrs[i].attrName, attrs[j].attrName)==0)
                return setErr(FEE_DUPLATTR);
    
    if(primAttrName != NULL && strlen(primAttrName) >= MAXNAME)
        return setErr(FEE_ATTRNAMETOOLONG);

    if(primAttrName == NULL && (dotstr = strchr(relName, '.')) != NULL)
        strcpy(primattr, dotstr+1);

    /*insert into relcat*/
    newRel.relwid = 0;
    strcpy(newRel.relname, relName);
    for(i=0; i<numAttrs; i++)
        newRel.relwid += attrs[i].attrLen;
    newRel.attrcnt = numAttrs;
    newRel.indexcnt = 0;
    if(primAttrName != NULL) 
        strcpy(newRel.primattr, primAttrName);
    else if (dotstr != NULL)
        strcpy(newRel.primattr, primattr);
    else
        memset(newRel.primattr, 0, MAXNAME);
    
    recid = HF_InsertRec(relcatFd, (char*)&newRel);
    if(!HF_ValidRecId(relcatFd, recid))
        return setErr(FEE_HF);

    /*insert into attrcat*/
    strcpy(newAttr.relname, relName);
    for(i=0; i<numAttrs; i++){
        strcpy(newAttr.attrname, attrs[i].attrName);
        newAttr.offset = offset;
        newAttr.attrlen = attrs[i].attrLen;
        newAttr.attrtype = attrs[i].attrType;
        newAttr.indexed = FALSE;
        newAttr.attrno = i;

        offset += attrs[i].attrLen;
        recid = HF_InsertRec(attrcatFd, (char*)&newAttr);
        if(!HF_ValidRecId(attrcatFd, recid))
            return setErr(FEE_HF);
    }

    if((msg=HF_CreateFile(relName, newRel.relwid))<0)
        return setErr(FEE_HF);

    return FEE_OK;
}

int DestroyTable(const char *relName) /* name of relation to destroy	*/{
    RELDESCTYPE relStr;
    ATTRDESCTYPE attrStr;
    RECID relrecid;
    RECID *attrrecid;
    int hfsd, i, msg;

    if((msg=findRelcat(relName, &relStr, &relrecid))<0)
        return setErr(msg);

    attrrecid = malloc(sizeof(RECID)*relStr.attrcnt);
    
    /*find from attrcat*/
    if((hfsd=HF_OpenFileScan(attrcatFd, STRING_TYPE, strlen(relName), attrCatOffset(relname),
                            EQ_OP, (char*)relName))<0)
        return setErr(FEE_HF);
    
    for(i=0; i<relStr.attrcnt; i++){
        attrrecid[i] = HF_FindNextRec(hfsd, (char *)&attrStr);
        if(!HF_ValidRecId(attrcatFd, attrrecid[i]))
            return setErr(FEE_NOSUCHATTR);

        if(attrStr.indexed)
            if(AM_DestroyIndex(relName, attrStr.attrno)<0)
                return setErr(FEE_AM);
    }

    if(HF_CloseFileScan(hfsd)<0)
        return setErr(FEE_HF);

    /* removing records*/
    if(HF_DeleteRec(relcatFd, relrecid)<0)
        return setErr(FEE_HF);
    
    for(i=0; i<relStr.attrcnt; i++){
        if(HF_DeleteRec(attrcatFd, attrrecid[i])<0)
            return setErr(FEE_HF);
    }

    free(attrrecid);

    return FEE_OK;
}

int BuildIndex(const char *relName,   /* relation name		*/
               const char *attrName) /* name of attr to be indexed	*/{
    int msg, hffd, amfd;
    RECID recid, relrecid, attrrecid;
    RELDESCTYPE relStr;
    ATTRDESCTYPE attrStr;
    char *record;

    /*find from relcat*/
    if ((msg = findRelcat(relName, &relStr, &relrecid)) < 0)
        return setErr(msg);

    record = malloc(relStr.relwid);

    /*find from attrcat*/
    if ((msg = findAttrcat(relName, attrName, &attrStr, &attrrecid)) < 0)
        return setErr(msg);

    /*found from attrcat*/
    /*now build index*/
    if ((hffd = HF_OpenFile(attrStr.relname)) < 0)
        return setErr(FEE_HF);

    if (AM_CreateIndex(attrStr.relname, attrStr.attrno, attrStr.attrtype,
                attrStr.attrlen, FALSE) < 0)
        return setErr(FEE_AM);

    if ((amfd = AM_OpenIndex(attrStr.relname, attrStr.attrno)) < 0)
        return setErr(FEE_AM);

    recid = HF_GetFirstRec(hffd, record);
    while (HF_ValidRecId(hffd, recid)) {
        if (AM_InsertEntry(amfd, record + attrStr.offset, recid) < 0)
            return setErr(FEE_AM);
        recid = HF_GetNextRec(hffd, recid, record);
    }
    if (AM_CloseIndex(amfd) < 0)
        return setErr(FEE_AM);

    if (HF_CloseFile(hffd) < 0)
        return setErr(FEE_HF);

    /*update relcat*/
    if (HF_DeleteRec(relcatFd, relrecid) < 0)
        return setErr(FEE_HF);

    relStr.indexcnt++;
    relrecid = HF_InsertRec(relcatFd, (char *)&relStr);
    if (!HF_ValidRecId(relcatFd, relrecid))
        return setErr(FEE_HF);

    /*update attrcat*/
    if (HF_DeleteRec(attrcatFd, attrrecid) < 0)
        return setErr(FEE_HF);

    attrStr.indexed = TRUE;
    attrrecid = HF_InsertRec(attrcatFd, (char *)&attrStr);
    if (!HF_ValidRecId(attrcatFd, attrrecid))
        return setErr(FEE_HF);

    free(record);

    return FEE_OK;
}

int DropIndex(const char *relname,   /* relation name		*/
              const char *attrName) /* name of indexed attribute	*/{
    int msg;
    RELDESCTYPE relStr;
    RECID relrecid;
    ATTRDESCTYPE attrStr;
    RECID attrrecid;
    int hfsd;

    /*find from relcatI*/
    if((msg=findRelcat(relname, &relStr, &relrecid))<0)
        return setErr(msg);

    if(relStr.indexcnt==0)
        return setErr(FEE_NOTINDEXED);

    if((hfsd = HF_OpenFileScan(attrcatFd, STRING_TYPE, strlen(relname), attrCatOffset(relname),
                            EQ_OP, (char*)relname))<0)
        return setErr(msg);
    
    attrrecid = HF_FindNextRec(hfsd, (char *)&attrStr);
    while(HF_ValidRecId(attrcatFd, attrrecid)){
        if( (attrName==NULL && !attrStr.indexed) ||
             (attrName != NULL && strcmp(attrName, attrStr.attrname)!=0) ){
            attrrecid = HF_FindNextRec(hfsd, (char*)&attrStr);
            continue;
        }

        if(attrName != NULL && !attrStr.indexed)
            return setErr(FEE_NOTINDEXED);

        if(AM_DestroyIndex(attrStr.relname, attrStr.attrno)<0)
            return setErr(FEE_AM);

        if(HF_DeleteRec(relcatFd, relrecid)<0)
            return setErr(FEE_HF);
        relStr.indexcnt--;
        relrecid = HF_InsertRec(relcatFd, (char*)&relStr);
        if(!HF_ValidRecId(relcatFd, relrecid))
            return setErr(FEE_HF);

        if(HF_DeleteRec(attrcatFd, attrrecid)<0)
            return setErr(FEE_HF);
        attrStr.indexed = FALSE;
        attrrecid = HF_InsertRec(attrcatFd, (char*)&attrStr);
        if(!HF_ValidRecId(attrcatFd, attrrecid))
            return setErr(FEE_HF);
        
        if(attrName!=NULL)
            break;

        attrrecid = HF_FindNextRec(hfsd, (char*)&attrStr);
    }

    if(attrName!=NULL && !HF_ValidRecId(attrcatFd, attrrecid))
        return setErr(FEE_NOSUCHATTR);

    if(HF_CloseFileScan(hfsd)<0)
        return setErr(FEE_HF);
    
    return FEE_OK;
}

int PrintTable(const char *relName) /* name of relation to print	*/{
    int msg, i, j;
    RELDESCTYPE relStr;
    RECID relrecid;
    ATTRDESCTYPE *attrStrArr;
    int hffd;
    RECID recid;
    char *record;
    int attrtype;

    /*find from relcat*/
    if ((msg = findRelcat(relName, &relStr, &relrecid)) < 0)
        return setErr(msg);
    
    record = malloc(relStr.relwid);

    /*find all from attrcat*/
    if((msg = getAttrcat(relName, &attrStrArr, relStr.attrcnt))<0)
        return setErr(msg);

    printf("Relation %s:\n", relName);

    for(i=0; i<relStr.attrcnt; i++){
        printf("+");
        if(attrStrArr[i].attrtype=='c'){
            for(j=0; j<max(14, attrStrArr[i].attrlen+2); j++)
                printf("-");
        }else
            printf("--------------");
    }
    printf("+\n");

    for(i=0; i<relStr.attrcnt; i++){
        if(attrStrArr[i].attrtype=='c')
            printf("| %-*s ", max(12, attrStrArr[i].attrlen), attrStrArr[i].attrname);
        else
            printf("| %-12s ", attrStrArr[i].attrname);
    }
    printf("|\n");

    for(i=0; i<relStr.attrcnt; i++){
        printf("+");
        if(attrStrArr[i].attrtype=='c'){
            for(j=0; j<max(14, attrStrArr[i].attrlen+2); j++)
                printf("-");
        }else
            printf("--------------");
    }
    printf("+\n");

    if(strcmp(relName, RELCATNAME)==0)
        hffd = relcatFd;
    else if(strcmp(relName, ATTRCATNAME)==0)
        hffd = attrcatFd;
    else if((hffd = HF_OpenFile(relName))<0)
        return setErr(FEE_HF);

    recid = HF_GetFirstRec(hffd, record);
    while(HF_ValidRecId(hffd, recid)){
        for(i=0; i<relStr.attrcnt; i++){
            if(strcmp(attrStrArr[i].attrname, "type")==0){
                attrtype = *(int*)(record + attrStrArr[i].offset);
                printf("| %-12s ", attrtype == 'c' ? "string": (attrtype == 'f' ? "real" :
                                    (attrtype=='i'? "integer" : "boolean")) );
            }else if(attrStrArr[i].attrtype==INT_TYPE){
                printf("| %12d ", *(int*)(record + attrStrArr[i].offset));
            }else if(attrStrArr[i].attrtype==REAL_TYPE){
                printf("| %12f ", *(float*)(record + attrStrArr[i].offset));
            }else if(attrStrArr[i].attrtype==STRING_TYPE){ /*string type*/
                printf("| %-*s ", max(12, attrStrArr[i].attrlen),record + attrStrArr[i].offset);
            }else { /*bool type*/
                printf("| %-12s ", *(bool_t*)(record + attrStrArr[i].offset) ? "yes": "no");
            }
        }
        printf("|\n");
        recid = HF_GetNextRec(hffd, recid, record);
    }
    
    if(strcmp(relName, RELCATNAME) !=0 && strcmp(relName,ATTRCATNAME) != 0)
        if(HF_CloseFile(hffd)<0 && (printf("closefile hffd: %d\n", hffd) || 1))
            return setErr(FEE_HF);

    free(attrStrArr);
    free(record);

    for(i=0; i<relStr.attrcnt; i++){
        printf("+");
        if(attrStrArr[i].attrtype=='c'){
            for(j=0; j<max(14, attrStrArr[i].attrlen+2); j++)
                printf("-");
        }else
            printf("--------------");
    }
    printf("+\n");

    return FEE_OK;
}

int LoadTable(const char *relName,   /* name of target relation	*/
              const char *fileName) /* file containing tuples	*/{
    int i, unixfd, msg;
    int nread;
    RELDESCTYPE relStr;
    RECID relrecid;
    ATTRDESCTYPE *attrStrArr;
    char *value;
    int hffd;
    int *amfd;
    RECID recid;

    /*find from relcat*/
    if ((msg = findRelcat(relName, &relStr, &relrecid)) < 0)
        return setErr(msg);

    value = malloc(relStr.relwid);
    amfd = malloc(sizeof(int)*relStr.attrcnt);

    /*find all from attrcat*/
    if ((msg = getAttrcat(relName, &attrStrArr, relStr.attrcnt)) < 0)
        return setErr(msg);

    if ((unixfd = open(fileName, O_RDONLY))<0)
        return setErr(FEE_UNIX);

    if((msg = lseek(unixfd, 0, SEEK_SET))<0)
        return setErr(FEE_UNIX);

    /*open hf and index*/
    if((hffd=HF_OpenFile(relName))<0)
        return setErr(FEE_HF);

    for(i=0; i<relStr.attrcnt; i++){
        if(attrStrArr[i].indexed){
            if((amfd[i] = AM_OpenIndex(relName, attrStrArr[i].attrno))<0)
                return setErr(FEE_AM);
        }
    }
    
    while((nread = read(unixfd, value, relStr.relwid))==relStr.relwid){
        recid = HF_InsertRec(hffd, value);
        if(!HF_ValidRecId(hffd, recid))
            return setErr(FEE_HF);
        
        for(i=0; i<relStr.attrcnt; i++){
            if(attrStrArr[i].indexed){
                if(AM_InsertEntry(amfd[i], value+attrStrArr[i].offset, recid)<0)
                    return setErr(FEE_AM);
            }
        }
    }
    if(nread != 0)
        return setErr(FEE_UNIX);

    if(close(unixfd)<0)
        return setErr(FEE_UNIX);

    if(HF_CloseFile(hffd)<0)
        return setErr(FEE_HF);
    
    for(i=0; i<relStr.attrcnt; i++){
        if(attrStrArr[i].indexed){
            if(AM_CloseIndex(amfd[i])<0)
                return setErr(FEE_AM);
        }
    }

    free(value);
    free(amfd);
    free(attrStrArr);

    return FEE_OK;
}

int HelpTable(const char *relName) /* name of relation		*/{
    int msg, i;
    RELDESCTYPE relStr;
    RECID relrecid;
    ATTRDESCTYPE *attrStrArr;

    /*print contents of retcat catalog*/
    if(relName==NULL){
        for(i=0; i<RELCAT_NATTRS; i++)
            printf("+--------------");
        printf("+\n");

        for(i=0; i<RELCAT_NATTRS; i++)
            printf("| %-12s ", RELDESCNAME[i]);
        printf("|\n");

        for(i=0; i<RELCAT_NATTRS; i++)
            printf("+--------------");
        printf("+\n");

        relrecid = HF_GetFirstRec(relcatFd, (char*)&relStr);
        while(HF_ValidRecId(relcatFd, relrecid)){
            printf("| %-12s | %12d | %12d | %12d | %-12s |\n", 
                    relStr.relname, relStr.relwid, relStr.attrcnt, 
                    relStr.indexcnt, (strlen(relStr.primattr)!=0 ? relStr.primattr : "NULL"));
            relrecid = HF_GetNextRec(relcatFd, relrecid, (char*)&relStr);
        }

        for(i=0; i<RELCAT_NATTRS; i++)
            printf("+--------------");
        printf("+\n");

        return FEE_OK;
    }

    if((msg = findRelcat(relName, &relStr, &relrecid))<0)
        return setErr(msg);

    printf("Relation  %s:\n", relStr.relname);
    printf("\t");
    printf("Prim Attr:  %-s", relStr.primattr);
    printf("\tNo of Attrs:  %-d", relStr.attrcnt);
    printf("\tTuple width:  %-d", relStr.relwid);
    printf("\tNo of Indices:  %-d\n", relStr.indexcnt);
    printf("Attributes:\n");

    for(i=0; i<ATTRCAT_NATTRS-1; i++)
        printf("+--------------");
    printf("+\n");

    for(i=1; i<ATTRCAT_NATTRS; i++)
        printf("| %-12s ", ATTRDESCNAME[i]);
    printf("|\n");

    for(i=0; i<ATTRCAT_NATTRS-1; i++)
        printf("+--------------");
    printf("+\n");

    /*find fron relcat*/
    if((msg = getAttrcat(relName, &attrStrArr, relStr.attrcnt))<0)
        return setErr(msg);

    for(i=0; i<relStr.attrcnt; i++){
        printf("| %-12s | %12d | %12d | %-12s | %-12s | %12d |\n",
                attrStrArr[i].attrname, attrStrArr[i].offset,
                attrStrArr[i].attrlen, 
                attrStrArr[i].attrtype=='c' ? "string" : (attrStrArr[i].attrtype=='f' ? "real": 
                                        (attrStrArr[i].attrtype=='i' ? "integer" : "boolean")), 
                (attrStrArr[i].indexed ? "yes": "no"), attrStrArr[i].attrno);
    }

    for(i=0; i<ATTRCAT_NATTRS-1; i++)
        printf("+--------------");
    printf("+\n");

    free(attrStrArr);

    return FEE_OK;
}   

/*
 * Prototypes for QU layer functions
 */

int Select(const char *srcRelName, /* source relation name         */
           const char *selAttr,    /* name of selected attribute   */
           int op,                 /* comparison operator          */
           int valType,            /* type of comparison value     */
           int valLength,          /* length if type = STRING_TYPE */
           char *value,            /* comparison value             */
           int numProjAttrs,       /* number of attrs to print     */
           char *projAttrs[],      /* names of attrs of print      */
           char *resRelName)      /* result relation name         */{
    int i, j, msg, ind;
    RELDESCTYPE relStr;
    RECID relrecid;
    ATTRDESCTYPE *attrStrArr;
    int hffd, amfd;
    int hfsd, amsd;
    ATTR_DESCR *attrDesc;
    ATTR_VAL *attrVal;
    int *projInd;
    RECID recid;
    char *record;

    /*check if relRelName exist*/
    if(resRelName != NULL && access(resRelName, F_OK) != -1)
        return setErr(FEE_RELEXISTS);

    /*find from relcat*/
    if ((msg = findRelcat(srcRelName, &relStr, &relrecid)) < 0)
        return setErr(msg);

    if (numProjAttrs <=0 || numProjAttrs > relStr.attrcnt)
        return setErr(FEE_INCORRECTNATTRS);
    
    attrDesc = malloc(sizeof(ATTR_DESCR) * numProjAttrs);
    projInd = malloc(sizeof(int) * numProjAttrs);
    attrVal = malloc(sizeof(ATTR_VAL) * numProjAttrs);
    record = malloc(relStr.relwid);

    /*find all from attrcat*/
    if ((msg = getAttrcat(srcRelName, &attrStrArr, relStr.attrcnt)) < 0)
        return setErr(msg);

    if(selAttr != NULL){
        if((ind = findAttrName(selAttr, attrStrArr, relStr.attrcnt))<0)
            return setErr(FEE_NOSUCHATTR);

        if(attrStrArr[ind].attrtype !=valType)
            return setErr(FEE_INVATTRTYPE);

        if(attrStrArr[ind].attrlen != valLength)
            return setErr(FEE_INCORRECTNATTRS);

        if(op <1 || op> 6)
            return setErr(FEE_INVALIDOP);
    }

    /*find projAttrs*/
    for(i=0; i<numProjAttrs; i++){
        if((projInd[i] = findAttrName(projAttrs[i], attrStrArr, relStr.attrcnt))<0)
            return setErr(FEE_NOSUCHATTR);
        
        attrDesc[i].attrLen = attrStrArr[projInd[i]].attrlen;
        attrDesc[i].attrType = attrStrArr[projInd[i]].attrtype;
        attrDesc[i].attrName =  attrStrArr[projInd[i]].attrname;
    }
    /*open hf am*/
    if((hffd = HF_OpenFile(srcRelName))<0)
        return setErr(FEE_HF);
    if(selAttr != NULL && attrStrArr[ind].indexed && 
        (amfd = AM_OpenIndex(srcRelName, attrStrArr[ind].attrno))<0)
        return setErr(FEE_AM);

    if(selAttr != NULL){
        if(!attrStrArr[ind].indexed){
            if((hfsd = HF_OpenFileScan(hffd, valType, valLength, attrStrArr[ind].offset, op, value))<0)
                return setErr(FEE_HF);
        } else {
            if((amsd = AM_OpenIndexScan(amfd, op, value))<0)
                return setErr(FEE_HF);
        }
    }

    if(resRelName != NULL){
        if((msg = CreateTable(resRelName, numProjAttrs, attrDesc, relStr.primattr))<0)
            return setErr(msg);
    } else { /*print to stdout*/
        for(i=0; i<numProjAttrs; i++){
            printf("+");
            if(attrStrArr[projInd[i]].attrtype=='c'){
                for(j=0; j<max(14, attrStrArr[projInd[i]].attrlen+2); j++)
                    printf("-");
            }else
                for(j=0; j<14; j++)
                    printf("-");
        }
        printf("+\n");

        for(i=0; i<numProjAttrs; i++){
            if(attrStrArr[projInd[i]].attrtype == 'c')
                printf("| %-*s ", max(12, attrStrArr[projInd[i]].attrlen), projAttrs[i]);
            else
                printf("| %-12s ", attrStrArr[projInd[i]].attrname);
        }
        printf("|\n");

        for(i=0; i<numProjAttrs; i++){
            printf("+");
            if(attrStrArr[projInd[i]].attrtype=='c'){
                for(j=0; j<max(14, attrStrArr[projInd[i]].attrlen+2); j++)
                    printf("-");
            }else
                for(j=0; j<14; j++)
                    printf("-");
        }
        printf("+\n");
    }

    if(selAttr != NULL){
        if(!attrStrArr[ind].indexed){ /*hf file scan*/
            while(1){
                recid = HF_FindNextRec(hfsd, record);
                if(!HF_ValidRecId(hffd, recid))
                    break;

                if(resRelName != NULL){
                    updateAttrVal(attrVal, numProjAttrs, projAttrs, projInd, attrStrArr, record);
                    if ((msg = Insert(resRelName, numProjAttrs, attrVal)) < 0)
                        return setErr(msg);
                } else
                    printRecord(numProjAttrs, projAttrs, projInd, attrStrArr, record);

            }
        }else { /*am file scan*/
            while(1){
                recid = AM_FindNextEntry(amsd);
                if(!HF_ValidRecId(hffd, recid))
                    break;
                if(HF_GetThisRec(hffd, recid, record)<0)
                    return setErr(FEE_HF);

                if (resRelName != NULL){
                    updateAttrVal(attrVal, numProjAttrs, projAttrs, projInd, attrStrArr, record);
                    if ((msg = Insert(resRelName, numProjAttrs, attrVal)) < 0)
                        return setErr(msg);
                }else
                    printRecord(numProjAttrs, projAttrs, projInd, attrStrArr, record);
            }
        }
    }else {
        recid = HF_GetFirstRec(hffd, record);
        while (HF_ValidRecId(hffd, recid)){
            if(resRelName != NULL){
                updateAttrVal(attrVal, numProjAttrs, projAttrs, projInd, attrStrArr, record);
                if ((msg = Insert(resRelName, numProjAttrs, attrVal)) < 0)
                    return setErr(msg);
            } else
                printRecord(numProjAttrs, projAttrs, projInd, attrStrArr, record);
            
            recid = HF_GetNextRec(hffd, recid, record);
        }
    }

    if(resRelName == NULL){
        for(i=0; i<numProjAttrs; i++){
            printf("+");
            if(attrStrArr[projInd[i]].attrtype=='c'){
                for(j=0; j<max(14, attrStrArr[projInd[i]].attrlen+2); j++)
                    printf("-");
            }else
                for(j=0; j<14; j++)
                    printf("-");
        }
        printf("+\n");
    }

    /*close hf am*/
    if(selAttr != NULL){
        if(!attrStrArr[ind].indexed){
            if(HF_CloseFileScan(hfsd)<0)
                return setErr(FEE_HF);
        } else {
            if(AM_CloseIndexScan(amsd)<0)
                return setErr(FEE_AM);
        }
    }
    if(HF_CloseFile(hffd)<0)
        return setErr(FEE_HF);
    if(selAttr != NULL && attrStrArr[ind].indexed && 
        AM_CloseIndex(amfd) <0)
        return setErr(FEE_AM);

    free(attrStrArr);
    free(attrDesc);
    free(projInd);
    free(attrVal);
    free(record);

    return FEE_OK;
}

int Join(REL_ATTR *joinAttr1,  /* join attribute #1            */
         int op,               /* comparison operator          */
         REL_ATTR *joinAttr2,  /* join attribute #2            */
         int numProjAttrs,     /* number of attrs to print     */
         REL_ATTR projAttrs[], /* names of attrs to print      */
         char *resRelName)    /* result relation name         */{
    int i, j, msg;
    RELDESCTYPE relStr1, relStr2;
    RECID relrecid1, relrecid2;
    RECID attrrecid1, attrrecid2;
    ATTRDESCTYPE *attrStrArr1, *attrStrArr2;
    int ind1, ind2;
    int hffd1, amfd1;
    int hffd2, amfd2;
    int hfsd1, amsd1;
    int hfsd2, amsd2;
    /*int hfsd1[MAXSCANS], amsd1[MAXSCANS];*/
    /*int hfsd2[MAXSCANS], amsd2[MAXISCANS];*/
    pii *projInd;
    RECID recid1, recid2;
    char *record1, *record2;
    ATTR_DESCR *attrDesc;
    ATTR_VAL *attrVal;

    /*check if relRelName exist*/
    if(resRelName != NULL && access(resRelName, F_OK) != -1)
        return setErr(FEE_RELEXISTS);

    /*check if they have the same name*/
    if(strcmp(joinAttr1->relName, joinAttr2->relName)==0)
        return setErr(FEE_SAMEJOINEDREL);
    
    /*check if relRelName exist*/
    if(resRelName != NULL && access(resRelName, F_OK) != -1)
        return setErr(FEE_RELEXISTS);

    /*find from relcat*/
    if ((msg = findRelcat(joinAttr1->relName, &relStr1, &relrecid1)) < 0)
        return setErr(msg);
    if ((msg = findRelcat(joinAttr2->relName, &relStr2, &relrecid2)) < 0)
        return setErr(msg);

    if(numProjAttrs <=0 || numProjAttrs > relStr1.attrcnt + relStr2.attrcnt)
        return setErr(FEE_INCORRECTNATTRS);
    
    projInd = (pii *)malloc(sizeof(pii) * numProjAttrs);
    attrDesc = malloc(sizeof(ATTR_DESCR) * numProjAttrs);
    attrVal = malloc(sizeof(ATTR_VAL) * numProjAttrs);
    record1 = malloc(relStr1.relwid);
    record2 = malloc(relStr2.relwid);

    /*find all from attrcat*/
    if ((msg = getAttrcat(joinAttr1->relName, &attrStrArr1, relStr1.attrcnt)) < 0)
        return setErr(msg);
    if ((msg = getAttrcat(joinAttr2->relName, &attrStrArr2, relStr2.attrcnt)) < 0)
        return setErr(msg);

    /*find index*/
    if((ind1 = findAttrName(joinAttr1->attrName, attrStrArr1, relStr1.attrcnt))<0)
        return setErr(FEE_NOSUCHATTR);
    if((ind2 = findAttrName(joinAttr2->attrName, attrStrArr2, relStr2.attrcnt))<0)
        return setErr(FEE_NOSUCHATTR);
    
    /*check if same len and type*/
    if(attrStrArr1[ind1].attrlen != attrStrArr2[ind2].attrlen )
        return setErr(FEE_INCORRECTNATTRS);
    
    if(attrStrArr1[ind1].attrtype != attrStrArr2[ind2].attrtype)
        return setErr(FEE_INVATTRTYPE);

    if(op <1 || op>6)
        return setErr(FEE_INVALIDOP);
    
    /*find projAttrs*/
    for(i=0; i<numProjAttrs; i++){
        if(strcmp(joinAttr1->relName, projAttrs[i].relName)==0){
            projInd[i].front = 0;
            if((j = findAttrName(projAttrs[i].attrName, attrStrArr1, relStr1.attrcnt))<0)
                return setErr(FEE_NOSUCHATTR);

            projInd[i].second = j;
            attrDesc[i].attrLen = attrStrArr1[j].attrlen;
            attrDesc[i].attrType = attrStrArr1[j].attrtype;
            attrDesc[i].attrName = attrStrArr1[j].attrname;

        } else if(strcmp(joinAttr2->relName, projAttrs[i].relName)==0){
            projInd[i].front = 1;
            if((j = findAttrName(projAttrs[i].attrName, attrStrArr2, relStr2.attrcnt))<0)
                return setErr(FEE_NOSUCHATTR);

            projInd[i].second = j;
            attrDesc[i].attrLen = attrStrArr2[j].attrlen;
            attrDesc[i].attrType = attrStrArr2[j].attrtype;
            attrDesc[i].attrName = attrStrArr2[j].attrname;

        }else/*relname not valid*/
            return setErr(FEE_RELNOTSAME);
    }

    /*open hf am*/
    if((hffd1 = HF_OpenFile(joinAttr1->relName))<0)
        return setErr(FEE_HF);
    if((hffd2 = HF_OpenFile(joinAttr2->relName))<0)
        return setErr(FEE_HF);
    
    if(attrStrArr1[ind1].indexed && (amfd1 = AM_OpenIndex(joinAttr1->relName, attrStrArr1[ind1].attrno))<0)
        return setErr(FEE_AM);
    if(attrStrArr2[ind2].indexed && (amfd2 = AM_OpenIndex(joinAttr2->relName, attrStrArr2[ind2].attrno))<0)
        return setErr(FEE_AM);

    /*create table or print table columns*/
    if(resRelName != NULL){
        if((msg = CreateTable(resRelName, numProjAttrs, attrDesc, NULL))<0)
            return setErr(msg);
    } else {  /*print columns*/
        for(i=0; i<numProjAttrs; i++){
            printf("+");
            if(attrDesc[i].attrType=='c'){
                for(j=0; j<max(14, attrDesc[i].attrLen+2); j++)
                    printf("-");
            }else
                for(j=0; j<14; j++)
                    printf("-");
        }
        printf("+\n");

        for(i=0; i<numProjAttrs; i++){
            if(attrDesc[i].attrType == 'c')
                printf("| %-*s ", max(12, attrDesc[i].attrLen), attrDesc[i].attrName);
            else
                printf("| %-12s ", attrDesc[i].attrName);
        }
        printf("|\n");

        for(i=0; i<numProjAttrs; i++){
            printf("+");
            if(attrDesc[i].attrType=='c'){
                for(j=0; j<max(14, attrDesc[i].attrLen+2); j++)
                    printf("-");
            }else
                for(j=0; j<14; j++)
                    printf("-");
        }
        printf("+\n");
    }

    if(!attrStrArr1[ind1].indexed && !attrStrArr2[ind2].indexed){ /*none indexed*/

        if(HF_totalNumRec(hffd1) > HF_totalNumRec(hffd2)){/*1 as inner*/

            recid2 = HF_GetFirstRec(hffd2, record2);

            while(HF_ValidRecId(hffd2, recid2)){
                if((hfsd1 = HF_OpenFileScan(hffd1, attrStrArr2[ind2].attrtype, attrStrArr2[ind2].attrlen, 
                                    attrStrArr1[ind1].offset, op, record2 + attrStrArr2[ind2].offset))<0)
                    return setErr(FEE_HF);

                recid1 = HF_FindNextRec(hfsd1, record1);
                while(HF_ValidRecId(hffd1, recid1)){

                    updateAttrValJoin(attrVal, numProjAttrs, projAttrs, projInd, 
                                            attrStrArr1, attrStrArr2, record1, record2);

                    if(resRelName != NULL){
                        if((msg = Insert(resRelName, numProjAttrs, attrVal))<0)
                            return setErr(msg);
                    }else 
                        printRecordAttr(attrVal, numProjAttrs);
                    
                    recid1 = HF_FindNextRec(hfsd1, record1);
                }
                if(HF_CloseFileScan(hfsd1)<0)
                    return setErr(FEE_HF);

                recid2 = HF_GetNextRec(hffd2, recid2, record2);
            }

        } else { /* 2 as inner*/
            if(op == LT_OP) op = GT_OP;
            if(op == GT_OP) op = LT_OP;
            if(op == LE_OP) op = GE_OP;
            if(op == GE_OP) op = LE_OP;

            recid1 = HF_GetFirstRec(hffd1, record1);

            while(HF_ValidRecId(hffd1, recid1)){
                if((hfsd2 = HF_OpenFileScan(hffd2, attrStrArr1[ind1].attrtype, attrStrArr1[ind1].attrlen, 
                                    attrStrArr2[ind2].offset, op, record1 + attrStrArr1[ind1].offset))<0)
                    return setErr(FEE_HF);

                recid2 = HF_FindNextRec(hfsd2, record2);
                while(HF_ValidRecId(hffd2, recid2)){

                    updateAttrValJoin(attrVal, numProjAttrs, projAttrs, projInd, 
                                            attrStrArr1, attrStrArr2, record1, record2);

                    if(resRelName != NULL){
                        if((msg = Insert(resRelName, numProjAttrs, attrVal))<0)
                            return setErr(msg);
                    }else 
                        printRecordAttr(attrVal, numProjAttrs);
                    
                    recid2 = HF_FindNextRec(hfsd2, record2);
                }
                if(HF_CloseFileScan(hfsd2)<0)
                    return setErr(FEE_HF);

                recid1 = HF_GetNextRec(hffd1, recid1, record1);
            }
        }

    } else if (attrStrArr2[ind2].indexed){ /*2 indexed*/
        if(op == LT_OP) op = GT_OP;
        if(op == GT_OP) op = LT_OP;
        if(op == LE_OP) op = GE_OP;
        if(op == GE_OP) op = LE_OP;

        recid1 = HF_GetFirstRec(hffd1, record1);

        while(HF_ValidRecId(hffd1, recid1)){
            if((amsd2 = AM_OpenIndexScan(amfd2, op, record1 + attrStrArr1[ind1].offset))<0)
                return setErr(FEE_AM);
            
            recid2 = AM_FindNextEntry(amsd2);
            while(HF_ValidRecId(hffd2, recid2)){
                if(HF_GetThisRec(hffd2, recid2, record2)<0)
                    return setErr(FEE_HF);

                updateAttrValJoin(attrVal, numProjAttrs, projAttrs, projInd, 
                                        attrStrArr1, attrStrArr2, record1, record2);

                if(resRelName != NULL){
                    if((msg = Insert(resRelName, numProjAttrs, attrVal))<0)
                        return setErr(msg);
                }else 
                    printRecordAttr(attrVal, numProjAttrs);

                recid2 = AM_FindNextEntry(amsd2);
            }
            
            if(AM_CloseIndexScan(amsd2)<0)
                return setErr(FEE_AM);

            recid1 = HF_GetNextRec(hffd1, recid1, record1);
        }

    } else { /* 1 indexed and 2 is not indexed*/
        recid2 = HF_GetFirstRec(hffd2, record2);

        while(HF_ValidRecId(hffd2, recid2)){
            if((amsd1 = AM_OpenIndexScan(amfd1, op, record2 + attrStrArr2[ind2].offset))<0)
                return setErr(FEE_AM);
            
            recid1 = AM_FindNextEntry(amsd1);
            while(HF_ValidRecId(hffd1, recid1)){
                if(HF_GetThisRec(hffd1, recid1, record1)<0)
                    return setErr(FEE_HF);

                updateAttrValJoin(attrVal, numProjAttrs, projAttrs, projInd, 
                                        attrStrArr1, attrStrArr2, record1, record2);

                if(resRelName != NULL){
                    if((msg = Insert(resRelName, numProjAttrs, attrVal))<0)
                        return setErr(msg);
                }else 
                    printRecordAttr(attrVal, numProjAttrs);

                recid1 = AM_FindNextEntry(amsd1);
            }
            if(AM_CloseIndexScan(amsd1)<0)
                return setErr(FEE_AM);

            recid2 = HF_GetNextRec(hffd2, recid2, record2);
        }

    }

    if(resRelName == NULL){
        for(i=0; i<numProjAttrs; i++){
            printf("+");
            if(attrDesc[i].attrType=='c'){
                for(j=0; j<max(14, attrDesc[i].attrLen+2); j++)
                    printf("-");
            }else
                for(j=0; j<14; j++)
                    printf("-");
        }
        printf("+\n");
    }

    /*close hf am*/
    if(HF_CloseFile(hffd1) <0)
        setErr(FEE_HF);
    if(HF_CloseFile(hffd2)<0)
        setErr(FEE_HF);

    if (attrStrArr1[ind1].indexed && AM_CloseIndex(amfd1)<0)
        return setErr(FEE_AM);
    if (attrStrArr2[ind2].indexed && AM_CloseIndex(amfd2)<0)
        return setErr(FEE_AM);

    free(attrStrArr1);
    free(attrStrArr2);
    free(projInd);
    free(attrDesc);
    free(attrVal);
    free(record1);
    free(record2);

    return FEE_OK;
}

int Insert(const char *relName, /* target relation name         */
           int numAttrs,        /* number of attribute values   */
           ATTR_VAL values[])  /* attribute values             */{
    int i, msg;
    RELDESCTYPE relStr;
    RECID relrecid;
    ATTRDESCTYPE *attrStrArr;
    int ind;
    char *value;
    int *amfd;
    int hffd;
    RECID recid;

    /*find from relcat*/
    if ((msg = findRelcat(relName, &relStr, &relrecid)) < 0)
        return setErr(msg);
    
    if(relStr.attrcnt != numAttrs)
        return setErr(FEE_INCORRECTNATTRS);

    value = malloc(relStr.relwid);

    /*find all from attrcat*/
    if ((msg = getAttrcat(relName, &attrStrArr, relStr.attrcnt)) < 0)
        return setErr(msg);

    for(i=0; i<relStr.attrcnt; i++){
        if((ind=findAttrVal(attrStrArr[i].attrname, values, numAttrs))<0)
            return setErr(FEE_NOSUCHATTR);
        
        if(values[ind].valType!=attrStrArr[i].attrtype)
            return setErr(FEE_INVATTRTYPE);
        
        if(values[ind].valLength!=attrStrArr[i].attrlen)
            return setErr(FEE_INCORRECTNATTRS);
        
        if(values[ind].value==NULL)
            return setErr(FEE_INCORRECTNATTRS);
        
        memcpy(value+attrStrArr[i].offset, values[ind].value, values[ind].valLength);
    }

    /* open hf am*/
    if((msg = openHFAM(relName, &hffd, &amfd, relStr.attrcnt, attrStrArr)<0))
        return setErr(msg);
    
    /*insert into hf*/
    recid = HF_InsertRec(hffd, value);
    if(!HF_ValidRecId(hffd, recid))
        return setErr(FEE_HF);
    
    /*insert into am*/
    for(i=0; i<relStr.attrcnt; i++)
        if(attrStrArr[i].indexed)
            if(AM_InsertEntry(amfd[i], value+attrStrArr[i].offset, recid)<0)
                return setErr(FEE_AM);
    
    if((msg = closeHFAM(hffd, amfd, relStr.attrcnt, attrStrArr))<0)
        return setErr(msg);
    
    free(attrStrArr);
    free(value);
    
    return FEE_OK;
}

int Delete(const char *relName, /* target relation name         */
           const char *selAttr, /* name of selection attribute  */
           int op,              /* comparison operator          */
           int valType,         /* type of comparison value     */
           int valLength,       /* length if type = STRING_TYPE */
           char *value)        /* comparison value             */{
    int i, msg, ind=0;
    RELDESCTYPE relStr;
    RECID relrecid;
    ATTRDESCTYPE *attrStrArr;
    int hffd, *amfd;
    int hfsd, *amsd;
    RECID recid;
    char *record;

    /*find from relcat*/
    if ((msg = findRelcat(relName, &relStr, &relrecid)) < 0)
        return setErr(msg);

    record = malloc(relStr.relwid);

    /*find all from attrcat*/
    if ((msg = getAttrcat(relName, &attrStrArr, relStr.attrcnt)) < 0)
        return setErr(msg);

    if(selAttr != NULL){
        if((ind = findAttrName(selAttr, attrStrArr, relStr.attrcnt))<0)
            return setErr(FEE_INCORRECTNATTRS);

        if(attrStrArr[ind].attrtype != valType)
            return setErr(FEE_INVATTRTYPE);
        
        if(attrStrArr[ind].attrlen != valLength)
            return setErr(FEE_INCORRECTNATTRS);

        if(op < 1 || op > 6)
            return setErr(FEE_INVALIDOP);
    }
    
    if((msg = openHFAM(relName, &hffd, &amfd, relStr.attrcnt, attrStrArr))<0)
        return setErr(msg);

    if(selAttr!=NULL){
        if((msg = openHFAMScan(&hfsd, &amsd, hffd, amfd, relStr.attrcnt, 
                            attrStrArr, valType, valLength, attrStrArr[ind].offset, op, value))<0)
            return setErr(msg);
    }else{
        if ((msg = openHFAMScan(&hfsd, &amsd, hffd, amfd, relStr.attrcnt,
                                attrStrArr, valType, valLength, 0, op, NULL)) < 0)
            return setErr(msg);
    }

    while(1){
        recid = HF_FindNextRec(hfsd, record);
        if(!HF_ValidRecId(hffd, recid))
            break;
        HF_DeleteRec(hffd, recid);
        for(i=0; i<relStr.attrcnt; i++){
            if(attrStrArr[i].indexed){
                recid = AM_FindNextEntry(amsd[i]);
                if(AM_DeleteEntry(amfd[i], record + attrStrArr[i].offset, recid)<0)
                    return setErr(FEE_AM);
            }
        }
    }

    if((msg = closeHFAMScan(hfsd, amsd, relStr.attrcnt, attrStrArr))<0)
        return setErr(msg);
    
    if((msg = closeHFAM(hffd, amfd, relStr.attrcnt, attrStrArr))<0)
        return setErr(msg);
    
    free(attrStrArr);
    free(record);

    return FEE_OK;
}

void FE_PrintError(const char *errmsg) /* error message		*/{
    fputs(errmsg, stderr);
    fputs(": ", stderr);
    printf("err: %d\n", FEerrno);

    switch(FEerrno){
        case 1: /*not initialized*/
            break;
        case 0: /*FEE_OK*/
            break;
        case -1:
            fputs("FEE_ALREADYINDEXED", stderr);
            break;
        case -2:
            fputs("FEE_ATTRNAMETOOLONG", stderr);
            break;
        case -3:
            fputs("FEE_DUPLATTR", stderr);
            break;
        case -4:
            fputs("FEE_INCOMPATJOINTYPES", stderr);
            break;
        case -5:
            fputs("FEE_INCORRECTNATTRS", stderr);
            break;
        case -6:
            fputs("FEE_INTERNAL", stderr);
            break;
        case -7:
            fputs("FEE_INVATTRTYPE", stderr);
            break;
        case -8:
            fputs("FEE_INVNBUCKETS", stderr);
            break;
        case -9:
            fputs("FEE_NOTFROMJOINREL", stderr);
            break;
        case -10:
            fputs("FEE_NOSUCHATTR", stderr);
            break;
        case -11:
            fputs("FEE_NOSUCHDB", stderr);
            break;
        case -12:
            fputs("FEE_NOSUCHREL", stderr);
            break;
        case -13:
            fputs("FEE_NOTINDEXED", stderr);
            break;
        case -14:
            fputs("FEE_PARTIAL", stderr);
            break;
        case -15:
            fputs("FEE_PRIMARYINDEX", stderr);
            break;
        case -16:
            fputs("FEE_RELCAT", stderr);
            break;
        case -17:
            fputs("FEE_RELEXISTS", stderr);
            break;
        case -18:
            fputs("FEE_RELNAMETOOLONG", stderr);
            break;
        case -19:
            fputs("FEE_SAMEJOINEDREL", stderr);
            break;
        case -20:
            fputs("FEE_SELINTOSRC", stderr);
            break;
        case -21:
            fputs("FEE_THISATTRTWICE", stderr);
            break;
        case -22:
            fputs("FEE_UNIONCOMPAT", stderr);
            break;
        case -23:
            fputs("FEE_WRONGVALTYPE", stderr);
            break;
        case -24:
            fputs("FEE_RELNOTSAME", stderr);
            break;
        case -25:
            fputs("FEE_NOMEM", stderr);
            break;
        case -26:
            fputs("FEE_EOF", stderr);
            break;
        case -27:
            fputs("FEE_CATALOGCHANGE", stderr);
            break;
        case -28:
            fputs("FEE_STRTOOLONG", stderr);
            break;
        case -29:
            fputs("FEE_SCANTABLEFULL", stderr);
            break;
        case -30:
            fputs("FEE_INVALIDSCAN", stderr);
            break;
        case -31:
            fputs("FEE_INVALIDSCANDESC", stderr);
            break;
        case -32:
            fputs("FEE_INVALIDOP", stderr);
            break;
        case -100:
            fputs("FEE_UNIX", stderr);
            break;
        case -101:
            fputs("FEE_HF\n", stderr);
            HF_PrintError("HF_ERR");
            return;
            break;
        case -102:
            fputs("FEE_AM\n", stderr);
            AM_PrintError("AM_ERR");
            return;
            break;
        case -103:
            fputs("FEE_PF\n", stderr);
            PF_PrintError("PF_ERR");
            break;
    }
    fputs("\n", stderr);
}
void FE_Init(void)                    /* FE initialization		*/{
    AM_Init();
}
