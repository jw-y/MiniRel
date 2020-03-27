#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "minirel.h"
#include "bf.h"
#include "pf.h"

#include "pfinternal.h"

/*
 * default files
 */
#define FILE1 "file1"

/*
 * Open the file, allocate as many pages in the file as the buffer manager
 * would allow, and write the page number into the data, then close file.
 */
void writefile(const char *fname)
{
    int i;
    int fd, pagenum;
    char *buf;
    int error;

    /* open file1, and allocate a few pages in there */
    if ((fd = PF_OpenFile(fname)) < 0)
    {
        PF_PrintError((char *)"open file1");
        exit(1);
    }
    printf("\n******** %s opened for write ***********\n", fname);

    for (i = 0; i < 2 * BF_MAX_BUFS; i++)
    {
        if ((error = PF_AllocPage(fd, &pagenum, &buf)) != PFE_OK)
        {
            printf("PF_AllocPage fails (i=%d)\n", i);
            PF_PrintError("first buffer\n");
            exit(1);
        }
        memcpy(buf, (char *)&i, sizeof(int));
        /*((int*)buf)[0] = i;*/
        printf("allocated page %d, value_written %d\n", pagenum, i);

        /* mark all these pages dirty */
        if (PF_DirtyPage(fd, pagenum) != PFE_OK)
        {
            PF_PrintError("PF_DirtyPage");
            exit(1);
        }

        /* unfix these pages */
        if ((error = PF_UnpinPage(fd, pagenum, FALSE)) != PFE_OK)
        {
            PF_PrintError("unfix buffer");
            exit(1);
        }
    }

    /* close the file */
    if ((error = PF_CloseFile(fd)) != PFE_OK)
    {
        PF_PrintError("close file1");
        exit(1);
    }
}

/*
 * print the contents off the specified file
 */
void printfile(int fd)
{
    int i, error;
    char *buf;
    int pagenum;

    printf("\n ********* reading file **********\n");
    printf("filename: %s\n", PFftab[fd].fname);
    /* pagenum = -1 means from the beginning */
    /*
    pagenum = -1;
*/

    if ((error = PF_GetFirstPage(fd, &pagenum, &buf)) == PFE_OK)
    {
        memcpy((char *)&i, buf, sizeof(int));
        printf("got page %d, value_read %d\n", pagenum, i);
        fflush(stdout);
        if ((error = PF_UnpinPage(fd, pagenum, FALSE)) != PFE_OK)
        {
            PF_PrintError("unfix");
            exit(1);
        }
    }
    else
    {
        PF_PrintError("First Page");
        printf("error = %d\n", error);
        exit(1);
    }

    while ((error = PF_GetNextPage(fd, &pagenum, &buf)) == PFE_OK)
    {
        memcpy((char *)&i, buf, sizeof(int));
        printf("got page %d, value_read %d\n", pagenum, i);
        fflush(stdout);
        if ((error = PF_UnpinPage(fd, pagenum, FALSE)) != PFE_OK)
        {
            PF_PrintError("unfix");
            exit(1);
        }
    }
    if (error != PFE_EOF)
    {
        PF_PrintError("not eof");
        exit(1);
    }
    printf("\n ********** eof reached **********\n");
}

/*
 * read the specified file and then print its contents
 */
void readfile(const char *fname)
{
    int error;
    int fd;

    printf("\n ********** %s opened for read ********\n", fname);
    if ((fd = PF_OpenFile(fname)) < 0)
    {
        PF_PrintError("open file");
        exit(1);
    }
    printfile(fd);
    if ((error = PF_CloseFile(fd)) != PFE_OK)
    {
        PF_PrintError("close file");
        exit(1);
    }
}

/*
 * general tests of PF layer
 */
void testpf1(void)
{
    int i, error, pagenum;
    char *buf;
    int fd1, fd2;
    char command[30];
    int temp;

    /* Making sure file don't exist */
    unlink(FILE1);

    /* create a few files */
    if ((error = PF_CreateFile(FILE1)) != PFE_OK)
    {
        PF_PrintError(FILE1);
        exit(1);
    }
    printf("\n*********** %s created *********\n", FILE1);

    /* write to file1 */
    writefile(FILE1);

    /* print it out */
    readfile(FILE1);

    printf("\n ****** Showing the file has been written *****\n");
    sprintf(command, "ls -al %s", FILE1);
    system(command);

    /* write to and read from file1 again */
    writefile(FILE1);
    readfile(FILE1);

    printf("\n ****** Showing the file has been written *****\n");
    sprintf(command, "ls -al %s", FILE1);
    system(command);

    /*
    if (PF_DestroyFile(FILE1)!= PFE_OK){
        PF_PrintError(FILE1);
        exit(1);
    }
*/
}

void testpf2(){
    char filename[10];
    int i, error;
    int fd;
    
    printf("Opening %d files\n", MAXOPENFILES);

    for(i=0; i<MAXOPENFILES*2; i++){
        sprintf(filename, "%s%d", "test", i);
        unlink(filename);
        
        /* create a few files */
        if ((error = PF_CreateFile(filename)) != PFE_OK){
            PF_PrintError(filename);
            exit(1);
        }
        printf("\n*********** %s created *********\n", filename);
    }

    for(i=0; i<MAXOPENFILES; i++){
        sprintf(filename, "%s%d", "test", i);
        if ((fd = PF_OpenFile(filename)) < 0){
            PF_PrintError((char *)"open file1");
            printf("%s not there\n", filename);
            exit(1);
        }
        printf("\n******** %s opened for write ***********\n", filename);
    }


    for(i=0; i<MAXOPENFILES; i++){
        if ((error = PF_CloseFile(i)) != PFE_OK){
            PF_PrintError("close file");
            exit(1);
        }

        printf("\n******** %d closed ***********\n", i);
    }

    for(i=0; i<MAXOPENFILES; i++){
        sprintf(filename, "%s%d", "test", i+20);
        if ((fd = PF_OpenFile(filename)) < 0){
            PF_PrintError((char *)"open file1");
            printf("%s not there\n", filename);
            exit(1);
        }
        printf("\n******** %s opened for write ***********\n", filename);
    }

    for(i=0; i<MAXOPENFILES; i++){
        if(PFftab[i].valid){
            printf("filename: %s\n", PFftab[i].fname);
        }else {
            printf("fd %d not valid\n", i);
        }
    }
    

    for(i=0; i<MAXOPENFILES; i++){
        if ((error = PF_CloseFile(i)) != PFE_OK){
            PF_PrintError("close file");
            exit(1);
        }

        printf("\n******** %d closed ***********\n", i);
    }

    

    for(i=0; i<MAXOPENFILES; i++){
        sprintf(filename, "%s%d", "test", i);
        if ((fd = PF_OpenFile(filename)) < 0){
            PF_PrintError((char *)"open file1");
            printf("%s not there\n", filename);
            exit(1);
        }
        printf("\n******** %s opened for write ***********\n", filename);
    }

    for(i=0; i<MAXOPENFILES; i++){
        if ((error = PF_CloseFile(i)) != PFE_OK){
            PF_PrintError("close file");
            exit(1);
        }

        printf("\n******** %d closed ***********\n", i);
    }


    for(i=0; i<MAXOPENFILES *2; i++){
        sprintf(filename, "%s%d", "test", i);
        if (PF_DestroyFile(filename)!= PFE_OK){
            PF_PrintError(filename);
            exit(1);
        }
        printf("\n*********** %s deleted *********\n", filename);
    }
}

int main()
{
    /* initialize PF layer */
    PF_Init();

    printf("\n************* Starting testpf1 *************\n");
    /*testpf1();*/
    testpf2();
    printf("\n************* End testpf1 ******************\n");
}
