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
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define BLOCK_SIZE (BSIZE)									//constant for block size
#define INODE_ADDR(i) ((struct dinode *)(addr + IBLOCK(i) * BLOCK_SIZE) + ((i) % IPB))		//translate logical block to physical

char* addr;			//used to access image file using mmap

struct dinode *dip;		//global struct for inodes from fcheck_helper
struct superblock *sb;		//global struct for super block from fcheck_helper
struct dirent *de;		//global struct for directories entry from fcheck_helper

int fsfd;			//used to open image file
int* active_inode_list;		//used to track allocated inodes to check if they're present in directories 
int* dir_visited;    // used to track inodes that we visit

//Function for test case 1
//this function checks what type is on the inode 
//if it's not a valid type check if its unallocated
//if its both invalid type and allocated, throw error
void check_valid_inode(struct dinode *ip){
if (ip->type < 1 || ip->type > 3){
 if (ip-> type == 0 && ip->size == 0) { return; }
  fprintf(stderr, "ERROR: bad inode.\n");
  exit(1);
 }
}

// Function to read the value for block in the bitmap
// returns  and integer 1/0 (allocated/not allocated)
int get_bit(int block_number) {
	int bitmap_block = BBLOCK(block_number, sb->ninodes); 
	unsigned char *bitmap = (unsigned char *)(addr + bitmap_block * BLOCK_SIZE);
	// Get the byte and check the bit corresponding to the block
	return (bitmap[block_number / 8] >> (block_number % 8)) & 1;
}

// Helper function to get the indirect blocks into a list for processing
// takes the provided block number and gets the pointer for that block
// loops through the indirect block and copies it to the passed pointer to be used later
void get_indirect_blocks(int indirect_block, int *block_list) {
 uint *indirect = (uint *)(addr + indirect_block * BLOCK_SIZE);
 // Read the indirect block and copy it to block_list
 for (int i = 0; i < NINDIRECT; i++) {
  block_list[i] = indirect[i];  // Store each block pointer
 }
}

//Helper function for processing directore entries (dirents)
//Checks if the inode is marked free
//Checks if the dirent is a properly formatted directory
//
void process_dirent(struct dirent *de, int dir_inum, bool* found_parent, bool* found_self){
	
 // If the entry is a directory, print its contents recursively
 if (de->inum != 0) {
 //printf("inum %d, name %s\n", de->inum, de->name);

 //if (strcmp(de->name, "") == 0) { continue;} //If the inode doesn't have a name dont count it as an entry
 //check if inode was allocated when we looped through the inodes
 if (active_inode_list[de->inum] == 0){
  fprintf(stderr, "ERROR: inode referred to in directory but marked free.\n");
  exit(1);
 }

 active_inode_list[de->inum] += 1;
			

 //Skip "." and ".." directory entries and note that we found them
 if (strcmp(de->name, ".") == 0){
 if (de->inum != dir_inum){
  fprintf(stderr, "ERROR: directory not properly formatted.\n");
  exit(1);
 }
  *found_self = true;
  return;
 } else if (strcmp(de->name, "..") == 0) {
 *found_parent = true;
 //If we're curretnly in the root dir, check that .. is the root dir still
 if (dir_inum == ROOTINO && de->inum != dir_inum){
  fprintf(stderr, "ERROR: root directory does not exist.\n");
  exit(1);
 }
  return;
}
			
  struct dinode *entry_inode = INODE_ADDR(de->inum);

  // If it's a directory, recurse only once per directory inode
  if (entry_inode->type == 1) {
  	if (!dir_visited[de->inum]) {
  		dir_visited[de->inum] = 1;
  		print_directory_contents(de->inum);
  	}
  }
 }
}

//Helper function to recursively traverse directories from the given inode
//this function assumes that we've already validated every inode
void print_directory_contents(int dir_inum) {
	
	struct dinode *dip = INODE_ADDR(dir_inum);
	
	if (dip->type != 1) {return;}
	
	bool found_parent = false;
	bool found_self = false;
	int remaining = dip->size;

	/* ---------- Direct blocks ---------- */
	for (int b = 0; b < NDIRECT && remaining > 0; b++) {
		if (dip->addrs[b] == 0) {continue;}

		struct dirent *de =(struct dirent *)(addr + dip->addrs[b] * BLOCK_SIZE);

		int entries = BLOCK_SIZE / sizeof(struct dirent);
		if (entries * sizeof(struct dirent) > remaining) { entries = remaining / sizeof(struct dirent);}

		for (int i = 0; i < entries; i++, de++) {
			process_dirent(de, dir_inum, &found_parent, &found_self);
		}

		remaining -= entries * sizeof(struct dirent);
	}

	/* ---------- Indirect blocks ---------- */
	if (remaining > 0 && dip->addrs[NDIRECT] != 0) {
		
		uint *indirect =(uint *)(addr + dip->addrs[NDIRECT] * BLOCK_SIZE);

		for (int b = 0; b < NINDIRECT && remaining > 0; b++) {
			if (indirect[b] == 0){continue;}
	
			struct dirent *de =(struct dirent *)(addr + indirect[b] * BLOCK_SIZE);
	
			int entries = BLOCK_SIZE / sizeof(struct dirent);
			if (entries * sizeof(struct dirent) > remaining) {entries = remaining / sizeof(struct dirent);}

			for (int i = 0; i < entries; i++, de++) {
				process_dirent(de, dir_inum, &found_parent, &found_self);
			}

		remaining -= entries * sizeof(struct dirent);
		}
	}

 
	if (found_self && found_parent) { return;}
	
	fprintf(stderr, "ERROR: directory not properly formatted.\n");
	exit(1);
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
    if(direct_blocks[j] < start || direct_blocks[j] >= sb->size){
     fprintf(stderr, "ERROR: bad indirect address in inode.\n");                                //exit with error for bad indirect inode address
     exit(1);
    }
  }
 }

 return 0; //return 0 if test passes 
}

//function for test case #6
//for blocks marked in-use in the bitmap the block should be used by an inode or an indirect inode
int test6(){
 int i, j;					
 int bits[sb->size];										//array to store blocks in inodes
 for(i = 0; i < sb->size; i++){									//initialize array to 0
  bits[i] = 0;
 }
 
 int niblock = (sb->ninodes / IPB);								//calculate the number of inode blocks needed
 if((sb->ninodes % IPB) != 0){
  niblock ++;
 }

 int bmblock = (sb->size / (BSIZE * 8));							//calculate the number of bitmap blocks needed

 if((sb->size % (BSIZE * 8)) != 0){
  bmblock ++;
 }

 int metablocks = 2 + niblock + bmblock;							//total overhead blocks =  2 + inodes + bitmap

 for(i = 0; i < metablocks + 1; i++){								//initialize overhead blocks to 1
  bits[i] = 1;
 }
 
 for(i = 0; i < sb->ninodes; i++){								//loop through all inodes
  struct dinode *inode = INODE_ADDR(i);
  for(j = 0; j < NDIRECT; j++){
   if(inode->addrs[j] == 0){continue;}								//skip if block is unassigned
   bits[inode->addrs[j]] = 1;									//record bit for this block
  }

  if(inode->addrs[NDIRECT] == 0){continue;}							//skip if indirect block is unassigned
  bits[inode->addrs[NDIRECT]] = 1;								//record bit for indirect block

  int direct_blocks[NINDIRECT];									//get blocks from indirect block
  int indirect_block = inode->addrs[NDIRECT];
  get_indirect_blocks(indirect_block, direct_blocks);

  for(j = 0; j < NINDIRECT; j++){								//find blocks used by indirect block
   if(direct_blocks[j] == 0){continue;}								
   bits[direct_blocks[j]] = 1;									//record bit for this block
  }
 }

 //compare the bitmap created using inodes to the bitmap in the image file
 
 for(i = 0; i < sb->size; i++)									//for every block
 {
   //printf("%d\n",i);
   int bit = get_bit(i);									//git bit for block i using helper function
   if(bit == 0) {continue;}
   if(bits[i] == 0){
    fprintf(stderr, "ERROR: bitmap marks block in use but it is not in use.\n");                 //exit with error for data-bitmap inode inconsistency
    exit(1);
    //printf("%dAAAAAAAAA%dBBBBBBBB%d\n",bit,bits[i],i); //test output
   }
 }
 
 return 0; //return 0 if test passes
}

//function for test case #7 and test case #8
//direct and indirect addresses in inodes should only be used once
int test78(){
 
 int i,j;
 int address_marks[sb->size + 1];					//array to mark previously visited addresses
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

 for(i = 0; i < sb->ninodes; i++){						//run test for every inode
  struct dinode *inode = INODE_ADDR(i);
  if(inode->type == T_FILE){							//only need to test regular files
   
   //active_inode_list is calulated in the directory helper function
   int refcount = active_inode_list[i] - 1;				       //get reference count in directories for inode
   
   //printf("%d AAAAAAAAAAA %d BBBBBBBBB %d \n",inode->nlink,refcount, i);
   if(inode->nlink != refcount){					       // compare directory references to inode links
    fprintf(stderr, "ERROR: bad reference count for file.\n");                 //exit with error for file reference count inconsistency
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
 for(i = 1; i < sb->ninodes; i++){								//run test for every inode
  struct dinode *inode = INODE_ADDR(i);
  if(inode->type == T_DIR && inode->nlink  > 1){						//check number of links if inode is a directory
   fprintf(stderr, "ERROR: directory appears more than once in file system.\n");		//exit with error for invalid directory links
   exit(1);
  }
 }
 return 0; //return 0 if test passes
}

int 
main(int argc, char *argv[]){
 
 //int fsfd;
 int i;

 if( argc < 2 ){										//check if arg number is valid
   fprintf(stderr, "Usage: fcheck <file_system_image>");
   exit(1); //exit 1 if no img file is given
 }

 fsfd = open(argv[1], O_RDONLY);								//attempt to open file
 if( fsfd < 0 ){										//exit with error if file no found
   fprintf(stderr, "image not found.\n");
   exit(1);
 }

 struct stat st;										//stats about image file
 fstat(fsfd, &st);										//used for determining mmap size

 addr = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fsfd, 0);				//mmap image file
 
 if(addr == MAP_FAILED){									//exit with error is map fails
  exit(1);
 }

sb = (struct superblock *) (addr + 1 * BLOCK_SIZE);						//find the superblock in the image
 //printf("fs size %d, no. of blocks %d, no. of inodes %d \n", sb->size, sb->nblocks, sb->ninodes);


active_inode_list = (int *)calloc(sb->ninodes + 1, sizeof(int));				//allocate memory for array used in directory helper
dir_visited = (int *)calloc(sb->ninodes + 1, sizeof(int));				//allocate memory for array used in directory helper

  
  //Loop over every inode
  int inum;
  //TODO: Potential indexing error?
  for(inum = 1; inum < sb->ninodes + 1; inum++) {
   struct dinode *ip = INODE_ADDR(inum);
   check_valid_inode(ip);
   if (inum == 1 && ip->size == 0){
     fprintf(stderr, "ERROR: root directory does not exist.\n");
     exit(1);
	}
   if(ip->type  == 0) {continue;}//skip over unallocated inodes
    active_inode_list[inum] = 1;
    //check that all of the inodes  blocks are present in the bitmap
    for (i = 0; i < NDIRECT; i++) {
     int block = ip->addrs[i];
     if (block != 0) {
      int alloc = get_bit(block);			
      //printf("Block %d used by inode %d (direct) Allocated: %d\n", block, inum, alloc);
      if (alloc != 1) { 
	fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
	exit(1);
      }
     }   
    }

    // If the inode has an indirect block, read the indirect block
    if (ip->addrs[NDIRECT] != 0) {
     int indirect_block = ip->addrs[NDIRECT];
     int block_list[NINDIRECT];
     // Get the list of blocks from the indirect block
     get_indirect_blocks(indirect_block, block_list);
     for (int i = 0; i < NINDIRECT; i++) {
      int block = block_list[i];
      if (block != 0) {
	int alloc = get_bit(block);			
	//printf("Block %d used by inode %d (indirect) Allocated: %d\n", block, inum, alloc);
      if (alloc != 1) { 
	fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
	exit(1);
      }
     }
    }
   }


   //printf("inode %d: type %d size %d nlink %d\n", inum, ip->type, ip->size, ip->nlink);

}


print_directory_contents(ROOTINO);
  
for(inum = 1; inum < sb->ninodes; inum++){
 //printf("inode %d seen %d times\n",inum, active_inode_list[inum] - 1);
 if (active_inode_list[inum] == 1){
  //printf("ERROR with inode %d\n", inum);
  fprintf(stderr, "ERROR: inode marked use but not found in a directory.\n");
  exit(1);
 }
}


 //run test cases for file system
 //should exit with error (1) if any test fails
 test2();	//call function for test #2
 test6();     	//call function for test #6
 test78(); 	//call function for tests #7 and #8
 test11();  	//call function for test #11
 test12();	//call function for test #12

 exit(0); //exit 0 if all tests pass
}
