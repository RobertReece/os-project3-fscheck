#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <stdbool.h>

#include "types.h"
#include "fs.h"

#define BLOCK_SIZE (BSIZE)

int test1(){
 return 0; //return 0 if test passes otherwise print error message and exit
}

//function for test case #2
//for each inode its blocks must point to a valid data block address in the image
int test2(){

 fprintf(stderr, "ERROR: bad direct address in inode.\n");   					//exit with error for bad direct inode address
 fprintf(stderr, "ERROR: bad indirect address in inode.\n"); 					//exit with error for bad indirect inode address
 return 0; //return 0 if test passes 
}

int test3(){
 return 0; //return 0 if test passes
}

int test4(){
 return 0; //return 0 if test passes
}


//function for test case #6
//for blocks marked in-use in the bitmap the block should be used by an inode or an indirect inode
int test6(){
 fprintf(stderr, "ERROR: bitmap marks block in use but it is not in use.\n");			//exit with error for data-bitmap inode inconsistency
 return 0; //return 0 if test passes
}

//function for test case #7
//direct addresses in inodes should only be used once
int test7(){
 fprintf(stderr, "ERROR: direct address used more than once.\n");				//exit with error for repeate direct address in inodes
 return 0; //return 0 if test passes
}

//function for test case #8
//indirect addresses in inodes should only be used once
int test8(){
 fprintf(stderr, "ERROR: indirect address used more than once.\n");				//exit with error for repeate indirect address in inodes
 return 0; //return 0 if test passes
}


//function for test case #10
//inodes in directories are marked for use in the inode bitmap
int test10(){
 fprintf(stderr, "ERROR: inode referred to in directory but marked free.\n");			//exit with error for directory inode-bitmap inconsistency
 return 0; //return 0 if test passes
}


//function for test case #12
//no extra links for directories
int test12(){
 fprintf(stderr, "ERROR: directory appears more than once in file system.\n");			//exit with error for invalid directory state
 return 0; //return 0 if test passes
}

int 
main(int argc, char *argv[]){

 if( argc < 2 ){
   fprintf(stderr, "Usage: fcheck <file_system_image>");
   exit(1); //exit 1 if no img file is given
 }


 //run test cases for file system
 //should exit with error (1) if any test fails
 test1();
 test2();
 test3();
 test4();

 test6();
 test7();
 test8();

 test10();

 test12();

 exit(0); //exit 0 if all tests pass
}

