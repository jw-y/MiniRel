#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "minirel.h"
#include "hf.h"

#define RECSIZE 80
#define STRSIZE 80
#define NUMBER  10000
#define RECORDVAL 77
#define RECORDVALSTR "entry0"
#define STRCMP "entry100"
#define FILE1 "recfile"
#define FILE2 "compfile"
#define FILE3 "recfile2"

#ifndef offsetof
#define offsetof(type, field)   ((size_t)&(((type *)0) -> field))
#endif

struct rec_struct
{
   char string_val[STRSIZE];
   float float_val;
   int int_val;
};

int read_string_recs(const char *filename);

/****************************************************************/
/* hftest1:                                                     */
/* Insert NUMBER records and delete those that are odd numbered.*/
/* Note that deleted records are still in the file, and simply  */
/* marked as deleted.                                           */
/****************************************************************/

void hftest1()
{

  int i,fd; 
  RECID recid;
  RECID next_recid;
  char recbuf[RECSIZE];
  char record[RECSIZE];

  /* making sure FILE1 doesn't exist */
  unlink(FILE1);

  /* Creating and opening the HF file */

  if (HF_CreateFile(FILE1,RECSIZE) != HFE_OK)
     HF_PrintError("Problem creating HF file.\n");
  if ((fd = HF_OpenFile(FILE1)) < 0)
     HF_PrintError("Problem opening HF file.\n");

  /* Adding the records */
    
  for (i = 0; i < NUMBER; i++)
  {
     memset(recbuf,' ',RECSIZE);
     sprintf(recbuf, "record%d", i);
     printf("adding record: %s\n", recbuf);
     recid = HF_InsertRec(fd, recbuf);
     if (!HF_ValidRecId(fd,recid))
     {
        HF_PrintError("Problem inserting record.\n");
        exit(1);
     }
  }   

  /* Getting the records */

  next_recid = HF_GetFirstRec(fd, record);
  if (!HF_ValidRecId(fd,next_recid))
  {
      HF_PrintError("Problem getting first record.\n");
      exit(1);
  }
   
  i = 0;
  while (HF_ValidRecId(fd,next_recid))
  {
    /* delete record if 'i' is odd numbered */
    if ((i % 2) != 0) 
    {
       printf("deleting record: %s\n", record);
       printf("p: %d, r: %d\n", recid.pagenum, recid.recnum);
       if (HF_DeleteRec(fd, next_recid) != HFE_OK){
          HF_PrintError("Problem deleting record.\n");
          exit(1);
       }
    }
    recid = HF_GetNextRec(fd, next_recid, record);
    next_recid = recid;
    printf("next rec(p,r): %d %d\n", recid.pagenum, recid.recnum);
    i++;
  }
  if (HF_CloseFile(fd) != HFE_OK)
  {
    HF_PrintError("Problem closing file.\n");
    exit(1);
  }

  read_string_recs(FILE1);
}


/***************************************************/
/* hftest2:                                        */
/* Run hftest1 to insert and delete records.       */
/* Then, insert new records and show the space used*/
/* to store the old records previously is now      */
/* occupied by the new records.                    */
/***************************************************/

void hftest2()
{
   int i,fd;
   RECID recid;
   char record[RECSIZE];

   /* inserts and delete records in FILE1 */
   hftest1();

   if ((fd = HF_OpenFile(FILE1)) < 0) 
   {
      HF_PrintError("Problem opening FILE1\n"); 
      exit(1);
   }
    
   /* clearing up the record */
   memset(record, ' ', RECSIZE);

   /* actual insertion */
   for (i = 1; i < NUMBER; i++)
   {
      sprintf(record, "New record %d",i);
      printf("Inserting new record: %s\n", record);
      recid = HF_InsertRec(fd, record);
      if (!HF_ValidRecId(fd,recid))
      {
        HF_PrintError("Problem inserting record.\n");
        exit(1);
      }
   }

  if (HF_CloseFile(fd) != HFE_OK)
  {
    HF_PrintError("Problem closing file.\n");
    exit(1);
  }

  read_string_recs(FILE1);
}

/************************************************/
/* insert_struc_recs:                           */
/* Inserts structured records to a given file.  */
/************************************************/

int insert_struc_recs(const char *filename)
{
  int i,fd;
  RECID recid;
  struct rec_struct record;

  /* making sure file doesn't exists */
  unlink(filename);

  /* Creating file with records of type rec_struct */  
  if (HF_CreateFile(filename, sizeof(struct rec_struct)) != HFE_OK) {
     HF_PrintError("Problem creating file.\n");
     exit(1);
  }
  
  /* Opening the file to get the file descriptor */
  if ((fd = HF_OpenFile(filename)) < 0) {
     HF_PrintError("Problem opening file\n.");
     exit(1);
  }
  
  /* Adding the records. */

  for (i = 0; i < NUMBER; i++) {
     memset((char *)&record, ' ', sizeof(struct rec_struct));
     sprintf(record.string_val, "entry%d", i);
     record.float_val = (float)i;
     record.int_val = i;
    
     printf("inserting structured record: (%s, %f, %d)\n",
		record.string_val, record.float_val, record.int_val);

     /* Actual insertion */
     recid = HF_InsertRec(fd, (char *)&record);
     if (!HF_ValidRecId(fd,recid)) {

        printf("recid: %d %d\n", recid.pagenum, recid.recnum);
        HF_PrintError("Problem inserting record.\n");
        exit(1);
     }
  }
  
  if (HF_CloseFile(fd) != HFE_OK) {
    HF_PrintError("Problem closing file.\n");
    exit(1);
  }
}
int insert_struc_recs2(const char *filename)
{
  int i,fd;
  RECID recid;
  struct rec_struct record;
  RECID next_recid;
  char recordBuf[RECSIZE];

  /* making sure file doesn't exists */
  unlink(filename);

  /* Creating file with records of type rec_struct */  
  if (HF_CreateFile(filename, sizeof(struct rec_struct)) != HFE_OK) {
     HF_PrintError("Problem creating file.\n");
     exit(1);
  }
  
  /* Opening the file to get the file descriptor */
  if ((fd = HF_OpenFile(filename)) < 0) {
     HF_PrintError("Problem opening file\n.");
     exit(1);
  }
  
  /* Adding the records. */

  for (i = 0; i < NUMBER; i++) {
     memset((char *)&record, ' ', sizeof(struct rec_struct));
     sprintf(record.string_val, "entry%d", i);
     record.float_val = (float)i;
     record.int_val = i;
    
     printf("inserting structured record: (%s, %f, %d)\n",
		record.string_val, record.float_val, record.int_val);

     /* Actual insertion */
     recid = HF_InsertRec(fd, (char *)&record);
     if (!HF_ValidRecId(fd,recid)) {
        HF_PrintError("Problem inserting record.\n");
        exit(1);
     }
  }

  next_recid = HF_GetFirstRec(fd, recordBuf);
  if (!HF_ValidRecId(fd,next_recid))
  {
      HF_PrintError("Problem getting first record.\n");
      exit(1);
  }
   
  i = 0;
  while (HF_ValidRecId(fd,next_recid))
  {
    /* delete record if 'i' is odd numbered */
    if ((i % 2) != 0) 
    {
       printf("deleting record: %s\n", recordBuf);
       printf("p: %d, r: %d\n", recid.pagenum, recid.recnum);
       if (HF_DeleteRec(fd, next_recid) != HFE_OK){
          HF_PrintError("Problem deleting record.\n");
          exit(1);
       }
    }
    recid = HF_GetNextRec(fd, next_recid, recordBuf);
    next_recid = recid;
    printf("next rec(p,r): %d %d\n", recid.pagenum, recid.recnum);
    i++;
  }

  next_recid = HF_GetFirstRec(fd, recordBuf);
  if (!HF_ValidRecId(fd,next_recid))
  {
      HF_PrintError("Problem getting first record.\n");
      exit(1);
  }
   
  i = 0;
  while (HF_ValidRecId(fd,next_recid))
  {
    /* delete record if 'i' is odd numbered */
    if ((i % 2) != 0) 
    {
       printf("deleting record: %s\n", recordBuf);
       printf("p: %d, r: %d\n", recid.pagenum, recid.recnum);
       if (HF_DeleteRec(fd, next_recid) != HFE_OK){
          HF_PrintError("Problem deleting record.\n");
          exit(1);
       }
    }
    recid = HF_GetNextRec(fd, next_recid, recordBuf);
    next_recid = recid;
    printf("next rec(p,r): %d %d\n", recid.pagenum, recid.recnum);
    i++;
  }
  
  if (HF_CloseFile(fd) != HFE_OK) {
    HF_PrintError("Problem closing file.\n");
    exit(1);
  }
}
int insert_struc_recs3(const char *filename)
{
  int i,fd;
  RECID recid;
  struct rec_struct record;
  RECID next_recid;
  char recordBuf[RECSIZE];

  /* making sure file doesn't exists */
  unlink(filename);

  /* Creating file with records of type rec_struct */  
  if (HF_CreateFile(filename, sizeof(struct rec_struct)) != HFE_OK) {
     HF_PrintError("Problem creating file.\n");
     exit(1);
  }
  
  /* Opening the file to get the file descriptor */
  if ((fd = HF_OpenFile(filename)) < 0) {
     HF_PrintError("Problem opening file\n.");
     exit(1);
  }
  
  /* Adding the records. */

  for (i = 0; i < NUMBER; i++) {
     memset((char *)&record, ' ', sizeof(struct rec_struct));
     sprintf(record.string_val, "entry%d", i);
     record.float_val = (float)i;
     record.int_val = i;
    
     printf("inserting structured record: (%s, %f, %d)\n",
		record.string_val, record.float_val, record.int_val);

     /* Actual insertion */
     recid = HF_InsertRec(fd, (char *)&record);
     if (!HF_ValidRecId(fd,recid)) {
        HF_PrintError("Problem inserting record.\n");
        exit(1);
     }
  }

   next_recid = HF_GetFirstRec(fd, recordBuf);
   
  if (!HF_ValidRecId(fd,next_recid))
  {
      HF_PrintError("Problem getting first record.\n");
      exit(1);
  }
   next_recid = HF_GetNextRec(fd, next_recid, recordBuf);
   next_recid = HF_GetNextRec(fd, next_recid, recordBuf);
  i = 0;
  while (HF_ValidRecId(fd,next_recid))
  {
    /* delete record if 'i' is odd numbered */
    /*if ((i % 2) != 0) */
    {
       printf("deleting record: %s\n", recordBuf);
       printf("p: %d, r: %d\n", recid.pagenum, recid.recnum);
       if (HF_DeleteRec(fd, next_recid) != HFE_OK){
          HF_PrintError("Problem deleting record.\n");
          exit(1);
       }
    }
    recid = HF_GetNextRec(fd, next_recid, recordBuf);
    next_recid = recid;
    printf("next rec(p,r): %d %d\n", recid.pagenum, recid.recnum);
    i++;
  }
  
  printf("hi\n");

  
  if (HF_CloseFile(fd) != HFE_OK) {
    HF_PrintError("Problem closing file.\n");
    exit(1);
  }
}

/**************************************************/
/* read_string_recs:                              */
/* reads string records from an input file.       */
/**************************************************/

int read_string_recs(const char *filename)
{
  int fd;
  struct rec_struct record;
  RECID recid, next_recid;
  char recbuf[RECSIZE];
  int numRet=0;
 
  /* opening the file */
  if ((fd = HF_OpenFile(filename)) < 0) {
     HF_PrintError("Problem opening file.\n");
     exit(1);
  }

  /* getting first record */
  recid = HF_GetFirstRec(fd, recbuf);
  if (!HF_ValidRecId(fd,recid)) {
     HF_PrintError("Problem getting record.\n");
     exit(1);
  }
  
  /* getting the rest of the records */
  while (HF_ValidRecId(fd,recid)) {
     /* printing the record value */
     numRet++;
     printf("recid: %d %d\n", recid.pagenum, recid.recnum);
     printf("retrieved record: %s\n", recbuf);
     next_recid = HF_GetNextRec(fd, recid, recbuf);
     recid = next_recid;
  }
   printf("num Retrived: %d\n", numRet);

  if (HF_CloseFile(fd) != HFE_OK) {
     HF_PrintError("Problem closing file.\n");
     exit(1);
  }
}

/**************************************************/
/* read_struc_recs:                               */
/* reads structured records from an input file.   */
/**************************************************/

int read_struc_recs(const char *filename)
{
  int fd;
  struct rec_struct record;
  RECID recid, next_recid;
  
 
  /* opening the file */
  if ((fd = HF_OpenFile(filename)) < 0) {
     HF_PrintError("Problem opening file.\n");
     exit(1);
  }

  /* getting first record */
  recid = HF_GetFirstRec(fd, (char *)&record);
  if (!HF_ValidRecId(fd,recid)) {
     HF_PrintError("Problem getting record.\n");
     exit(1);
  }

  printf("hello\n");
  
  /* getting the rest of the records */
  while (HF_ValidRecId(fd,recid)) {
     /* printing the record value */
     printf("retrieved structured record: (%s, %f, %d)\n",
		record.string_val, record.float_val, record.int_val); 
     next_recid = HF_GetNextRec(fd, recid, (char *)&record);
     recid = next_recid;
  }

  if (HF_CloseFile(fd) != HFE_OK) {
     HF_PrintError("Problem closing file.\n");
     exit(1);
  }
}

/*********************************************************/
/* hftest3:                                              */
/* Using insert_struc_recs, we will insert structured    */
/* records to a file, and then scan the file based on    */
/* it float attribute values of the records.             */
/* All the records with a value greater or equal to 50.0 */
/* will be retrived.                                     */
/*********************************************************/

void hftest3() 
{
  int fd, sd;  
  RECID recid, saved_recid;
  struct rec_struct record;
  float value;
  int valueInt;
  int numRet=0;

  /* making sure file doesn't exits */
  unlink(FILE2);
  
  /* inserting records in file */
  insert_struc_recs(FILE2); 
  read_struc_recs(FILE2); 

  /* opening the file because we need 'fd' for opening the scan */
  if ((fd = HF_OpenFile(FILE2)) < 0)
  {
     HF_PrintError("Problem opening file.\n");
     exit(1);
  } 

  /* opening scan in the float field of struc rec_struc */
  /* with comparison operator greater or equal.           */
  /* So only those records with a value greater or equal  */
  /* to 'value' will be returned by the scan.             */

  value = 50.0;
  valueInt = 99;
   /*(char *)&value*/
   /*
  if ((sd = HF_OpenFileScan(fd,REAL_TYPE,sizeof(float),offsetof(struct rec_struct, float_val),LE_OP,(char *)&value)) <0)
  {
     HF_PrintError("Problem opening scan\n.");
     exit(1);
  }*/
   printf("string size: %d\n", sizeof(RECORDVALSTR));

  if ((sd = HF_OpenFileScan(fd,STRING_TYPE,sizeof(RECORDVALSTR)+1,
                     offsetof(struct rec_struct, string_val),GT_OP, &RECORDVALSTR) ) <0)
  {
     HF_PrintError("Problem opening scan\n.");
     exit(1);
  }

   
  /* Getting the values from the scan */

  recid = HF_FindNextRec(sd,(char *)&record);

  printf("<< Scan records whose floating value >= 50 >>\n");
  while (HF_ValidRecId(fd,recid))
  {   
     numRet++;
     /* Save this record id for testing HF_GetThisRec() */
     /*if (record.int_val == RECORDVAL) saved_recid = recid;*/
     if(strcmp(record.string_val, &RECORDVALSTR)==0) saved_recid = recid;

     /* printing the record value */
     printf("scanned structured record: (%s, %f, %d)\n",
		record.string_val, record.float_val, record.int_val);

     recid = HF_FindNextRec(sd,(char *)&record);
  }
  printf("num retrieved: %d\n", numRet);

  printf("saved rec: %d %d\n", saved_recid.pagenum, saved_recid.recnum);

  /* Testing HF_GetThisRec() function */
  if (HF_GetThisRec(fd, saved_recid, (char *)&record) != HFE_OK) {
     HF_PrintError("Problem getting a record\n.");
     exit(1);
  }
  else {
     printf("<< fetch a record whose int value = %d >>\n",RECORDVAL);
     printf("record fetched by id: (%s, %f, %d)\n",
		record.string_val, record.float_val, record.int_val);
  }

  if (HF_CloseFileScan(sd) != HFE_OK) {
     HF_PrintError("Problem closing scan.\n");
     exit(1);
  }

  if (HF_CloseFile(fd) != HFE_OK) {
     HF_PrintError("Problem closing file.\n");
     exit(1);
  }

  if (HF_DestroyFile(FILE2) != HFE_OK) {
     HF_PrintError("Problem destroying the file.\n");
     exit(1);
  }

}

void hftest4() 
{
  int fd, sd;  
  RECID recid, saved_recid;
  struct rec_struct record;
  float value;
  int numRet = 0;

  /* making sure file doesn't exits */
  unlink(FILE2);
  
  /* inserting records in file */
  insert_struc_recs3(FILE2); 
  read_struc_recs(FILE2); 

  printf("hi\n");

  /* opening the file because we need 'fd' for opening the scan */
  if ((fd = HF_OpenFile(FILE2)) < 0)
  {
     HF_PrintError("Problem opening file.\n");
     exit(1);
  } 

  /* opening scan in the float field of struc rec_struc */
  /* with comparison operator greater or equal.           */
  /* So only those records with a value greater or equal  */
  /* to 'value' will be returned by the scan.             */

  value = 50.0;
   /*(char *)&value*/
  if ((sd = HF_OpenFileScan(fd,REAL_TYPE,sizeof(float),offsetof(struct rec_struct, 
                           float_val),GE_OP,NULL)) <0)
  {
     HF_PrintError("Problem opening scan\n.");
     exit(1);
  }
   
  /* Getting the values from the scan */

  recid = HF_FindNextRec(sd,(char *)&record);

  printf("<< Scan records whose floating value >= 50 >>\n");
  while (HF_ValidRecId(fd,recid))
  {   
     numRet++;
     /* Save this record id for testing HF_GetThisRec() */
     if (record.int_val == RECORDVAL) saved_recid = recid;

     /* printing the record value */
     printf("scanned structured record: (%s, %f, %d)\n",
		record.string_val, record.float_val, record.int_val);

     recid = HF_FindNextRec(sd,(char *)&record);
  }
   printf("num return: %d\n", numRet);
  printf("saved rec: %d %d\n", saved_recid.pagenum, saved_recid.recnum);

  /* Testing HF_GetThisRec() function */
  if (HF_GetThisRec(fd, saved_recid, (char *)&record) != HFE_OK) {
     HF_PrintError("Problem getting a record\n.");
     exit(1);
  }
  else {
     printf("<< fetch a record whose int value = %d >>\n",RECORDVAL);
     printf("record fetched by id: (%s, %f, %d)\n",
		record.string_val, record.float_val, record.int_val);
  }

  if (HF_CloseFileScan(sd) != HFE_OK) {
     HF_PrintError("Problem closing scan.\n");
     exit(1);
  }

  if (HF_CloseFile(fd) != HFE_OK) {
     HF_PrintError("Problem closing file.\n");
     exit(1);
  }

  if (HF_DestroyFile(FILE2) != HFE_OK) {
     HF_PrintError("Problem destroying the file.\n");
     exit(1);
  }

}

void hftest5(){
   int i,fd; 
  RECID recid;
  RECID next_recid;
  char recbuf[RECSIZE];
  char record[RECSIZE];

  /* making sure FILE1 doesn't exist */
  unlink(FILE1);

  /* Creating and opening the HF file */

  if (HF_CreateFile(FILE1,RECSIZE) != HFE_OK)
     HF_PrintError("Problem creating HF file.\n");
  if ((fd = HF_OpenFile(FILE1)) < 0)
     HF_PrintError("Problem opening HF file.\n");

  /* Adding the records */
    
  for (i = 0; i < NUMBER; i++)
  {
     memset(recbuf,' ',RECSIZE);
     sprintf(recbuf, "record%d", i);
     printf("adding record: %s\n", recbuf);
     recid = HF_InsertRec(fd, recbuf);
     if (!HF_ValidRecId(fd,recid))
     {
        HF_PrintError("Problem inserting record.\n");
        exit(1);
     }
  }   

  /* Getting the records */

  next_recid = HF_GetFirstRec(fd, record);
  if (!HF_ValidRecId(fd,next_recid))
  {
      HF_PrintError("Problem getting first record.\n");
      exit(1);
  }
   
  i = 0;
  while (HF_ValidRecId(fd,next_recid))
  {
    /* delete record if 'i' is odd numbered */
    if ((i % 2) != 0) 
    {
       printf("deleting record: %s\n", record);
       printf("p: %d, r: %d\n", recid.pagenum, recid.recnum);
       if (HF_DeleteRec(fd, next_recid) != HFE_OK){
          HF_PrintError("Problem deleting record.\n");
          exit(1);
       }
    }
    recid = HF_GetNextRec(fd, next_recid, record);
    next_recid = recid;
    printf("next rec(p,r): %d %d\n", recid.pagenum, recid.recnum);
    i++;
  }

  next_recid = HF_GetFirstRec(fd, record);
  if (!HF_ValidRecId(fd,next_recid))
  {
      HF_PrintError("Problem getting first record.\n");
      exit(1);
  }
   
  i = 0;
  while (HF_ValidRecId(fd,next_recid))
  {
    /* delete record if 'i' is odd numbered */
    if ((i % 2) != 0) 
    {
       printf("deleting record: %s\n", record);
       printf("p: %d, r: %d\n", recid.pagenum, recid.recnum);
       if (HF_DeleteRec(fd, next_recid) != HFE_OK){
          HF_PrintError("Problem deleting record.\n");
          exit(1);
       }
    }
    recid = HF_GetNextRec(fd, next_recid, record);
    next_recid = recid;
    printf("next rec(p,r): %d %d\n", recid.pagenum, recid.recnum);
    i++;
  }

  if (HF_CloseFile(fd) != HFE_OK)
  {
    HF_PrintError("Problem closing file.\n");
    exit(1);
  }

  read_string_recs(FILE1);
}

void hftest6(){
   int i,fd; 
  RECID recid;
  RECID next_recid;
  char recbuf[RECSIZE];
  char record[RECSIZE];
  char fileName[100];

   int fdd[MAXOPENFILES];
  int sdd[MAXSCANS];

  float value = 50.0;
  
   for(i=0; i<MAXOPENFILES; i++){
      sprintf(fileName, "opentabtest%d", i);
      unlink(fileName);
      if (HF_CreateFile(fileName,sizeof(struct rec_struct)) != HFE_OK)
         HF_PrintError("Problem creating HF file.\n");
      printf("opening: %s\n", fileName);
      if ((fdd[i] = HF_OpenFile(fileName)) < 0)
         HF_PrintError("Problem opening HF file.\n");
      if ((sdd[i] = HF_OpenFileScan(fdd[i],REAL_TYPE,sizeof(float),offsetof(struct rec_struct, float_val),LE_OP,(char *)&value)) <0){
         HF_PrintError("Problem opening scan\n.");
         exit(1);
      }
   }

   for(i=0; i<MAXOPENFILES; i++){
      if (HF_CloseFileScan(sdd[i]) != HFE_OK) {
         HF_PrintError("Problem closing scan.\n");
         exit(1);
      }
      sprintf(fileName, "opentabtest%d", i);
      if (HF_CloseFile(fdd[i]) != HFE_OK){
         HF_PrintError("Problem closing file.\n");
         exit(1);
      }
   }
   
   for(i=0; i<MAXOPENFILES; i++){
      sprintf(fileName, "opentabtest%d", i);
      if ((fdd[i] = HF_OpenFile(fileName)) < 0)
         HF_PrintError("Problem opening HF file.\n");
       printf("opening and closing: %s\n", fileName);
      if (HF_CloseFile(fdd[i]) != HFE_OK){
         HF_PrintError("Problem closing file.\n");
         exit(1);
      }
   }
   for(i=0; i<MAXOPENFILES; i++){
      sprintf(fileName, "opentabtest%d", i);
      if ((fdd[i] = HF_OpenFile(fileName)) < 0)
         HF_PrintError("Problem opening HF file.\n");
       printf("opening and closing: %s\n", fileName);
      if (HF_CloseFile(fdd[i]) != HFE_OK){
         HF_PrintError("Problem closing file.\n");
         exit(1);
      }
   }
   for(i=0; i<MAXOPENFILES; i++){
      sprintf(fileName, "opentabtest%d", i);
      if ((fdd[i] = HF_OpenFile(fileName)) < 0)
         HF_PrintError("Problem opening HF file.\n");
       printf("opening: %s\n", fileName);
   }

   for(i=0; i<MAXOPENFILES; i++){
      sprintf(fileName, "opentabtest%d", i);
      unlink(fileName);
      if (HF_CloseFile(fdd[i]) != HFE_OK){
         HF_PrintError("Problem closing file.\n");
         exit(1);
      }
   }

}

int main()
{
  HF_Init();

  /*
  printf("*** begin of hftest1 ***\n");
  hftest1();
  printf("*** end of hftest1 ***\n");
  
  printf("*** begin of hftest2 ***\n");
  hftest2();
  printf("*** end of hftest2 ***\n");
  */
  printf("*** begin of hftest3 *** \n");
  hftest3();
  printf("*** end of hftest3 *** \n");
  
 /*
  printf("*** begin of hftest4 *** \n");
  hftest4();
  printf("*** end of hftest4 *** \n");
  
 printf("*** begin of hftest5 *** \n");
  hftest5();
  printf("*** end of hftest5 *** \n");
  
 printf("*** begin of hftest6 *** \n");
  hftest6();
  printf("*** end of hftest6 *** \n");
  */
}

