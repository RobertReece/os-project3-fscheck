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

//stat struct in stat.h was causing compile errors due to sharing a name with the stat structure used for fstat
//definitions from stat.h are copied here to fix this
#define T_DIR 1		//dir
#define T_FILE 2	//file
#define T_DEV 3		//device

#define BLOCK_SIZE (BSIZE)
#define INODE_ADDR(i) ((struct dinode *)(addr + IBLOCK(i) * BLOCK_SIZE) + ((i) % IPB))

char* addr;
struct dinode *dip;
struct superblock *sb;
struct dirent *de;
int fsfd;


int get_bit(int block_number){
 int bitmap_block = BBLOCK(block_number, sb->ninodes);
 unsigned char *bitmap = (unsigned char *)(addr + bitmap_block * BLOCK_SIZE);
 return (bitmap[block_number / 8] >> (block_number % 8)) & 1;

}

void get_indirect_blocks(int indirect_block, int *block_list){
 uint *indirect = (uint *) (addr + indirect_block * BLOCK_SIZE);
 
 int i;
 for(i = 0; i < NINDIRECT; i++)
 {
  block_list[i] = indirect[i];
 }

}

int test1(){
 return 0; //return 0 if test passes otherwise print error message and exit
}

//function for test case #2
//for each inode its blocks must point to a valid data block address in the image
int test2(){
 int i,j;
 int start = sb->size - sb->nblocks; 

 for(i = 1; i < sb->ninodes + 1; i++){								//run test for every inode
  struct dinode *inode = INODE_ADDR(i);
  for(j = 0; j < NDIRECT; j++){									//test all direct blocks
    if(inode->addrs[j] == 0){continue;}								//skip if block is unassigned
    if(inode->addrs[j] < start || inode->addrs[j] >= sb->size){
    fprintf(stderr, "ERROR: bad direct address in inode.\n");                                   //exit with error for bad direct inode address
    exit(1);
   }
  }
  
  if(inode->addrs[NDIRECT] == 0){continue;} 							//skip if indirect block is unassigned
  int direct_blocks[NINDIRECT];									//array to hold indirect addresses
  int indirect_block = inode->addrs[NDIRECT];
  get_indirect_blocks(indirect_block, direct_blocks);						//call helper to find indirect blocks
  for(j = 0; j < NINDIRECT; j++){								//test all indirect blocks
    if(direct_blocks[j] == 0){ continue; }							//skip if block is unassigned
    if(direct_blocks[j] < start || direct_blocks[i] >= sb->size){
     fprintf(stderr, "ERROR: bad indirect address in inode.\n");                                //exit with error for bad indirect inode address
     exit(1);
    }
  }
 }

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
 int i, j;					
 int bits[sb->size + 1];									//array to store blocks in inodes
 for(i = 0; i < sb->size + 1; i++){								//initialize array to 0
  bits[i] = 0;
 }

 for(i = 0; i < sb->ninodes; i++){								//loop through all inodes
  struct dinode *inode = INODE_ADDR(i);
  for(j = 0; j < NDIRECT; j++){
   if(inode->addrs[j] == 0){continue;}								//skip if block is unassigned
   bits[inode->addrs[j]] = 1;									//record bit for this block
  }

  if(inode->addrs[NDIRECT] == 0){continue;}							//skip if indirect block is unassigned
  int direct_blocks[NINDIRECT];
  int indirect_block = inode->addrs[NDIRECT];
  get_indirect_blocks(indirect_block, direct_blocks);

  for(j = 0; j < NINDIRECT; j++){								//find blocks used by indirect block
   if(direct_blocks[j] == 0){continue;}								
   bits[direct_blocks[j]] = 1;									//record bit for this block
  }
 }

 //compare the bitmap created using inodes to the bitmap in the image file
 
 for(i = sb->size; i >= sb->size - sb->nblocks; i--)
 {
   //printf("%d\n",i);
   int bit = get_bit(i);
   if(bit == 0) {continue;}
   if(bits[i] == 0){
   //  printf("%dAAAAAAAAA%dBBBBBBBB%d\n",bit,bits[i],i); //test output
   }
 }
 
 //fprintf(stderr, "ERROR: bitmap marks block in use but it is not in use.\n");			//exit with error for data-bitmap inode inconsistency
 return 0; //return 0 if test passes
}

//function for test case #7 and test case #8
//direct and indirect addresses in inodes should only be used once
int test78(){
 
 int i,j;
 int address_marks[sb->size + 1];								//array to mark previously visited addresses
 for(int i = 0; i < sb->size+ 1; i++){
  address_marks[i] = 0;										//initialize address array to 0
 }

 for(i = 1; i < sb->ninodes + 1; i++){								//run test for every inode
  struct dinode *inode = INODE_ADDR(i);
  for(j = 0; j < NDIRECT; j++){									//test all direct blocks
   if(inode->addrs[j] == 0){continue;}								//skip if direct block is unassigned
   if(address_marks[inode->addrs[j]] == 1){ 
    fprintf(stderr, "ERROR: direct address used more than once.\n");                            //exit with error for repeate direct address in inodes
    exit(1);
   }
   address_marks[inode->addrs[j]] = 1;								//mark address as visited
  }

  if(inode->addrs[NDIRECT] == 0){continue;}                                                     //skip if indirect block is unassigned
  int direct_blocks[NINDIRECT];                                                                 //array to hold indirect addresses
  int indirect_block = inode->addrs[NDIRECT];
  get_indirect_blocks(indirect_block, direct_blocks);                                           //call helper to find indirect blocks
  for(j = 0; j < NINDIRECT; j++){                                                               //test all indirect blocks
   if(direct_blocks[j] == 0){continue;}                                                     	//skip if block is unassigned
   if(address_marks[direct_blocks[j]] == 1){
    fprintf(stderr, "ERROR: indirect address used more than once.\n");                          //exit with error for repeate indirect address in inodes
    exit(1);
   }
   address_marks[direct_blocks[j]] = 1;								//mark address as visited
  }
 }
 return 0; //return 0 if test passes
}

//function for test case #11
//number of links in a file does not mach its appearances in directories
int test11(){
 int i;
 int count[sb->ninodes];
 for(int i = 0; i < sb->ninodes; i++)
 {
  count[i] = 0;
 }

 for(i = 0; i < sb->ninodes; i++){								//run test for every inode
  struct dinode *inode = INODE_ADDR(i);
  if(inode->type == T_DIR){
        
	//need to navigate the dirents for this inode
	//  if(de->inum != 0){
	//  count[de->inum]++;

   }
  }

 for(i = 0; i < sb->ninodes; i++){
  struct dinode *inode = INODE_ADDR(i);
  if(inode->type == T_FILE){
   if(inode[i].nlink != count[i]){
    fprintf(stderr, "ERROR: inode referred to in directory but marked free.\n");                 //exit with error for file reference count inconsistency
    exit(1);
   }
  }
 }
 
 return 0; //return 0 if test passed
}


//function for test case #12
//no extra links for directories
int test12(){
 int i;
 for(i = 0; i < sb->ninodes; i++){								//run test for every inode
  struct dinode *inode = INODE_ADDR(i);
  if(inode->type == T_DIR && inode->nlink > 1){							//check number of links if inode is a directory
   fprintf(stderr, "ERROR: directory appears more than once in file system.\n");		//exit with error for invalid directory links
   exit(1);
  }
 }
 return 0; //return 0 if test passes
}

int 
main(int argc, char *argv[]){
 
 //int fsfd;

 if( argc < 2 ){
   fprintf(stderr, "Usage: fcheck <file_system_image>");
   exit(1); //exit 1 if no img file is given
 }

 fsfd = open(argv[1], O_RDONLY);
 if( fsfd < 0 ){
   fprintf(stderr, "image not found.\n");
   exit(1);
 }

 struct stat st;
 fstat(fsfd, &st);

 addr = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fsfd, 0);
 
 if(addr == MAP_FAILED){
  exit(1);
 }

 sb = (struct superblock *) (addr + 1 * BLOCK_SIZE);
 printf("fs size %d, no. of blocks %d, no. of inodes %d \n", sb->size, sb->nblocks, sb->ninodes);


 
 

 //run test cases for file system
 //should exit with error (1) if any test fails
 test2();
 
 //test6();
 test78();
 
 //test11();
 test12();

 exit(0); //exit 0 if all tests pass
}

