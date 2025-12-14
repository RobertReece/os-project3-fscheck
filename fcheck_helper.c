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

#define INODE_ADDR(i) ((struct dinode *)(addr + IBLOCK(i) * BLOCK_SIZE) + ((i) % IPB))

char *addr;
struct dinode *dip;
struct superblock *sb;
struct dirent *de;
int* active_inode_list;//used to track allocated inodes to check if they're present in directories 
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

//Helper function to recursively traverse directories from the given inode
//this function assumes that we've already validated every inode
void print_directory_contents(int dir_inum) {
	bool found_parent = false;
	bool found_self = false;
	// Access the inode for the directory
	struct dinode *dip = INODE_ADDR(dir_inum);

	// Only process if it's a directory (type == 1)
	if (dip->type != 1) {
		return;
	}
    
	// Read the directory blocks
	struct dirent *de = (struct dirent *)(addr + (dip->addrs[0]) * BLOCK_SIZE);
	int num_entries = dip->size / sizeof(struct dirent);
    
	// Loop through the directory entries and print them
	for (int i = 0; i < num_entries; i++, de++) {
	
		// If the entry is a directory, print its contents recursively
		if (de->inum != 0) {
			printf("inum %d, name %s\n", de->inum, de->name);
			
			//check if inode was allocated when we looped through the inodes
			if (active_inode_list[de->inum] == 0){
				fprintf(stderr, "ERROR: inode referred to in directory but marked free.\n");
				exit(1);
			}

			active_inode_list[de->inum] = -1;
			

			//Skip "." and ".." directory entries and note that we found them
			if (strcmp(de->name, ".") == 0){
				if (de->inum != dir_inum){
					fprintf(stderr, "ERROR: directory not properly formatted.\n");
				}
				found_self = true;
				continue;
			} else if (strcmp(de->name, "..") == 0) {
				found_parent = true;
				//If we're curretnly in the root dir, check that .. is the root dir still
				if (dir_inum == ROOTINO && de->inum != dir_inum){
					fprintf(stderr, "ERROR: root directory does not exist.\n");
					exit(1);
				}
				continue;
			}
			
			struct dinode *entry_inode = INODE_ADDR(de->inum);

			// If it's a directory, recurse
			if (entry_inode->type == 1){
				printf("  Recursively listing directory: %s\n", de->name);
				print_directory_contents(de->inum);
			}
		}
	}
	
	// If the inode has an indirect block, read the indirect block
	if (dip->addrs[NDIRECT] != 0) {
		
		printf("Done looking at direct blocks\n");
		int indirect_block = dip->addrs[NDIRECT];
		
		int block_list[NINDIRECT];

		// Get the list of blocks from the indirect block
		get_indirect_blocks(indirect_block, block_list);
		
		for (int i = 0; i < NINDIRECT; i++) {

			int block = block_list[i];
			if (block != 0) {
				struct dirent *de = (struct dirent *)(addr + block_list[i] * BLOCK_SIZE);
				int num_entries = BLOCK_SIZE / sizeof(struct dirent); 
				
				// Loop through the directory entries and print them
				for (int i = 0; i < num_entries; i++, de++) {
	
					// If the entry is a directory, print its contents recursively
					if (de->inum != 0) {
						printf("inum %d, name %s\n", de->inum, de->name);
			
						//check if inode was allocated when we looped through the inodes
						if (active_inode_list[de->inum] == 0){
							fprintf(stderr, "ERROR: inode referred to in directory but marked free.\n");
							exit(1);
						}

						active_inode_list[de->inum] = -1;
			

						//Skip "." and ".." directory entries and note that we found them
						if (strcmp(de->name, ".") == 0){
							if (de->inum != dir_inum){
								fprintf(stderr, "ERROR: directory not properly formatted.\n");
							}
							found_self = true;
							continue;
						} else if (strcmp(de->name, "..") == 0) {
							found_parent = true;
							//If we're curretnly in the root dir, check that .. is the root dir still
							if (dir_inum == ROOTINO && de->inum != dir_inum){
								fprintf(stderr, "ERROR: root directory does not exist.\n");
								exit(1);
							}
							continue;
						}
			
						struct dinode *entry_inode = INODE_ADDR(de->inum);

						// If it's a directory, recurse
						if (entry_inode->type == 1){
							printf("  Recursively listing directory: %s\n", de->name);
							print_directory_contents(de->inum);
						}
					}
				}
			}
		}
	}
	if (found_self && found_parent) { return;}
	fprintf(stderr, "ERROR: directory not properly formatted.\n");
	exit(1);
}

int main(int argc, char *argv[]) {
  int r,i,n,fsfd;

  if(argc < 2){
    fprintf(stderr, "Usage: sample fs.img ...\n");
    exit(1);
  }


  fsfd = open(argv[1], O_RDONLY);
  if(fsfd < 0){
    perror(argv[1]);
    exit(1);
  }

  struct stat st;
  fstat(fsfd, &st);
  addr = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fsfd, 0);
  if (addr == MAP_FAILED){
	perror("mmap failed");
	exit(1);
  }
  /* read the super block */
  sb = (struct superblock *) (addr + 1 * BLOCK_SIZE);
  printf("fs size %d, no. of blocks %d, no. of inodes %d \n", sb->size, sb->nblocks, sb->ninodes);

  /* read the inodes */
  dip = (struct dinode *) (addr + IBLOCK((uint)0)*BLOCK_SIZE); 
  printf("begin addr %p, begin inode %p , offset %d \n", addr, dip, (char *)dip -addr);

  // read root inode
  printf("Root inode  size %d links %d type %d \n", dip[ROOTINO].size, dip[ROOTINO].nlink, dip[ROOTINO].type);
  /*
  // get the address of root dir 
  de = (struct dirent *) (addr + (dip[ROOTINO].addrs[0])*BLOCK_SIZE);

  // print the entries in the first block of root dir 

  n = dip[ROOTINO].size/sizeof(struct dirent);
  for (i = 0; i < n; i++,de++){
 	printf(" inum %d, name %s address %p", de->inum, de->name, de);
    	printf("inode  size %d links %d type %d \n", dip[de->inum].size, dip[de->inum].nlink, dip[de->inum].type);
  }
  */
 
 //Array for tracking
  active_inode_list = (int *)calloc(sb->ninodes + 1, sizeof(int));


  
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
			printf("Block %d used by inode %d (direct) Allocated: %d\n", block, inum, alloc);
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
				printf("Block %d used by inode %d (indirect) Allocated: %d\n", block, inum, alloc);
				if (alloc != 1) { 
					fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
					exit(1);
				}
			}
		}
	}


	printf("inode %d: type %d size %d nlink %d\n", inum, ip->type, ip->size, ip->nlink);

  }



  print_directory_contents(ROOTINO);
  
  for(inum = 1; inum < sb->ninodes; inum++){
	if (active_inode_list[inum] == 1){
		
		printf("ERROR with inode %d\n", inum);
		fprintf(stderr, "ERROR: inode marked use but not found in a directory.\n");
		exit(1);
	}
  }


  exit(0);

}

