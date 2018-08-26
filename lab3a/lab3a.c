//NAME: Robathan Harries
//ID: 904836501
//EMAIL: r0b@ucla.edu
//SLIPDAYS: 0


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include "ext2_fs.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

// Define exit codes
#define SUCCESS_EXIT 0
#define BADARG_EXIT 1
#define ERROR_EXIT 2

// File organization constants
#define SUPERBLOCK_OFFSET 1024
#define BLOCK_OFFSET(blockNum) ((blockNum - 1) * blockSize + SUPERBLOCK_OFFSET) // Do not use until superblock is scanned

// access rights masks
#define MASK_IRWX_U 0x01c0 // user
#define MASK_IRWX_G 0x0038 // group
#define MASK_IRWX_O 0x0007 // other

// time buffer
#define TIME_BUFFER_LEN 100

// Global variables
static unsigned int blockSize; // Set when superblock is scanned

// List of allocated pointers
#define POINTER_NUM 20
static void* allocated[POINTER_NUM] = { NULL }; // whenever something is allocated it will be copied into this array
static int allocatedCounter = 0; // used to index into the next unused element of the allocated array

// Frees every pointer in allocated
void freeMem() {
  int i = 0;
  while (i < allocatedCounter)
    free(allocated[i++]);
}

// Generic exit routine - prints error message, frees memory, and exits with given code
void exitProgram(char *message, int exitCode) {
  if (exitCode != SUCCESS_EXIT) {
    if (errno) {
      fprintf(stderr, "%s: %s\n", message, strerror(errno));
    }
    else {
      fprintf(stderr, "%s\n", message);
    }
  }

  freeMem();

  exit(exitCode);
}

// Returns the image filename
char *parseArgs(int argc, char **argv) {
  if (argc != 2) exitProgram("Incorrect Format: ./lab3a image_file", BADARG_EXIT);
  
  return argv[1];
}

// Attempts to open the file, assigning the resulting file descriptor to fd
int openFile(char *fileName) {
  int fd = open(fileName, O_RDONLY);

  if (fd < 0) {
    exitProgram("Cannot open file", BADARG_EXIT);
  }

  return fd;
}

// Scan superblock, placing result into struct pointed to by superblock pointer - must be preallocated
void superblock_scan(int fd, struct ext2_super_block *sbPtr) {
  if (sbPtr == NULL) {
    fprintf(stderr, "INCORRECT USE OF FUNCTION: superblock_scan\n");
    free(sbPtr);
    exit(BADARG_EXIT);
  }

  if (pread(fd, sbPtr, sizeof(struct ext2_super_block), SUPERBLOCK_OFFSET) < 0)
    exitProgram("Cannot read superblock", ERROR_EXIT);

  blockSize = EXT2_MIN_BLOCK_SIZE << sbPtr->s_log_block_size;

  // check magic signature
  if (sbPtr->s_magic != EXT2_SUPER_MAGIC)
    exitProgram("File system isn't ext2", ERROR_EXIT);
  
  printf("SUPERBLOCK,%u,%u,%u,%u,%u,%u,%u\n",
	 sbPtr->s_blocks_count,
	 sbPtr->s_inodes_count,
	 blockSize,
	 sbPtr->s_inode_size,
	 sbPtr->s_blocks_per_group,
	 sbPtr->s_inodes_per_group,
	 sbPtr->s_first_ino);
}

// read group descriptor table - fills gdArr, which is pointer to start of array of group descriptors
void group_descriptor_table_scan(int fd, struct ext2_super_block *sbPtr, struct ext2_group_desc *gdArr, int groupNum) {
  int listSize = sizeof(struct ext2_group_desc) * groupNum;

  if (pread(fd, gdArr, listSize, SUPERBLOCK_OFFSET + blockSize) < 0)
    exitProgram("Cannot read group descriptor table", ERROR_EXIT);

  // record the total amount of blocks/inodes, then decrement to find the block/inode size of the last, incomplete group
  __u32 blocksRemaining = sbPtr->s_blocks_count;
  __u32 blocksInGroup = sbPtr->s_blocks_per_group;
  __u32 inodesRemaining = sbPtr->s_inodes_count;
  __u32 inodesInGroup = sbPtr->s_inodes_per_group;
  
  int i;
  for (i = 0; i < groupNum; i++) {
    if (blocksRemaining < blocksInGroup)
      blocksInGroup = blocksRemaining;
    if (inodesRemaining < inodesInGroup)
      inodesInGroup = inodesRemaining;

    printf("GROUP,%u,%u,%u,%u,%u,%u,%u,%u\n",
	   i,
	   blocksInGroup,
	   inodesInGroup,
	   gdArr[i].bg_free_blocks_count,
	   gdArr[i].bg_free_inodes_count,
	   gdArr[i].bg_block_bitmap,
	   gdArr[i].bg_inode_bitmap,
	   gdArr[i].bg_inode_table);

    blocksRemaining -= blocksInGroup;
    inodesRemaining -= inodesInGroup;
  }
}

// read free block entries from group block bitmaps
void free_block_scan(int fd, struct ext2_super_block *sbPtr, struct ext2_group_desc *gdArr, unsigned int groupNum) {
  unsigned int i;
  for (i = 0; i < groupNum; i++) {
    unsigned int j;
    for (j = 0; j < blockSize; j++) {
      // one byte at a time from block bitmap
      __u8 byte;
      if (pread(fd, &byte, 1, (blockSize * gdArr[i].bg_block_bitmap) + j) < 0)
	exitProgram("Can't read block bitmap", ERROR_EXIT);

      char maskBase = 1;
      int k;
      for (k = 0; k < 8; k++)
	if (!(byte & (maskBase << k)))
	  printf("BFREE,%u\n", i * sbPtr->s_blocks_per_group + j * 8 + k + 1);
    }
  }
}

// scans inode bitmap, printing entries for free inodes and filling the inode bitmap
void free_inode_scan(int fd, struct ext2_super_block *sbPtr, struct ext2_group_desc *gdArr, unsigned int groupNum, __u8 *imapArr) {
  unsigned int i;
  for (i = 0; i < groupNum; i++) {
    unsigned int j;
    for (j = 0; j < 1 + (sbPtr->s_inodes_per_group - 1) / 8; j++) {
      __u8 byte;
      if (pread(fd, &byte, 1, (blockSize * gdArr[i].bg_inode_bitmap) + j) < 0)
	exitProgram("Can't read inode bitmap", ERROR_EXIT);

      imapArr[blockSize * i + j] = byte;

      char maskBase = 1;
      int k;
      for (k = 0; k < 8 && j * 8 + k < sbPtr->s_inodes_per_group; k++)
	if (!(byte & (maskBase << k)))
	  printf("IFREE,%u\n", i * sbPtr->s_inodes_per_group + j * 8 + k + 1);
    }
  }
}

// format time into string
void formatTime(__u32 time, char *buf) {
  time_t epoch = time;
  struct tm ts = *gmtime(&epoch);
  strftime(buf, TIME_BUFFER_LEN, "%m/%d/%y %H:%M:%S", &ts);
}

// recursive scan for indirect directory references
void indirectRecursiveDirScan(int fd, int level, struct ext2_inode *inodePtr, int inodeNum, __u32 blockNum, unsigned int size) {
  __u32 totalEntries = blockSize / sizeof(__u32);
  __u32 *entries = calloc(totalEntries, sizeof(__u32));
  allocated[allocatedCounter++] = entries;

  if (pread(fd, entries, blockSize, BLOCK_OFFSET(blockNum)) < 0)
    exitProgram("Can't read indirect directory", ERROR_EXIT);

  char *block = calloc(blockSize, sizeof(char));
  struct ext2_dir_entry *entry;
   
  unsigned int i;
  for (i = 0; i < totalEntries; i++) {
    if (entries[i] != 0) {
      if (level > 1)
	indirectRecursiveDirScan(fd, level - 1, inodePtr, inodeNum, entries[i], size);

      else {
	if (pread(fd, block, blockSize, BLOCK_OFFSET(entries[i])) < 0)
	  exitProgram("Can't read indirect directory entries", ERROR_EXIT);
	
	entry = (struct ext2_dir_entry *) block;
      
	while (entry->file_type && size < inodePtr->i_size) {
	  if (entry->inode != 0) {
	    char fileName[EXT2_NAME_LEN + 1];
	    memcpy(fileName, entry->name, entry->name_len);
	    fileName[entry->name_len] = 0;
	    
	    printf("DIRENT,%d,%d,%d,%d,%d,'%s'\n",
		   inodeNum,
		   size,
		   entry->inode,
		   entry->rec_len,
		   entry->name_len,
		   fileName);
	  }

	  size += entry->rec_len;
	  entry = (void*) entry + entry->rec_len;
	}
      }
    }
  }

  // free allocated mem
  free(entries);
  allocatedCounter--;
}

// scan directory and print entries
void dir_scan(int fd, struct ext2_inode *inodePtr, unsigned int inodeNum) {
  // allocate a block's worth of memory
  unsigned char *block = calloc(blockSize, sizeof(unsigned char));
  allocated[allocatedCounter++] = block;
  
  struct ext2_dir_entry *dirEntryPtr;
  unsigned int size = 0;

  unsigned int i;
  for (i = 0; i < EXT2_NDIR_BLOCKS; i++) {
    if (pread(fd, block, blockSize, BLOCK_OFFSET(inodePtr->i_block[i])) < 0)
      exitProgram("Can't read directory entry", ERROR_EXIT);

    dirEntryPtr = (struct ext2_dir_entry *) block;

    while (dirEntryPtr->file_type && size < inodePtr->i_size) {
      char fileName[EXT2_NAME_LEN + 1];
      memcpy(fileName, dirEntryPtr->name, dirEntryPtr->name_len);
      fileName[dirEntryPtr->name_len] = 0;

      if (dirEntryPtr->inode != 0) {
	printf("DIRENT,%d,%d,%d,%d,%d,'%s'\n",
	       inodeNum,
	       size,
	       dirEntryPtr->inode,
	       dirEntryPtr->rec_len,
	       dirEntryPtr->name_len,
	       fileName);
      }

      size += dirEntryPtr->rec_len;
      dirEntryPtr = (void*) dirEntryPtr + dirEntryPtr->rec_len;
    }

    // check for indirect blocks
    if (inodePtr->i_block[EXT2_IND_BLOCK] != 0)
      indirectRecursiveDirScan(fd, 1, inodePtr, inodeNum, inodePtr->i_block[EXT2_IND_BLOCK], size);
    if (inodePtr->i_block[EXT2_DIND_BLOCK] != 0)
      indirectRecursiveDirScan(fd, 2, inodePtr, inodeNum, inodePtr->i_block[EXT2_DIND_BLOCK], size);
    if (inodePtr->i_block[EXT2_TIND_BLOCK] != 0)
      indirectRecursiveDirScan(fd, 3, inodePtr, inodeNum, inodePtr->i_block[EXT2_TIND_BLOCK], size);
  }

  //free block
  free(block);
  allocatedCounter--;
}

// read indirect block references
void indirect_block_scan(int fd, int level, unsigned int inodeNum, unsigned int blockNum, unsigned int fileOffset) {
  unsigned int totalEntries = blockSize / sizeof(unsigned int);
  unsigned int *entries = calloc(totalEntries, sizeof(unsigned int));
  allocated[allocatedCounter++] = entries;
  
  if (pread(fd, entries, blockSize, BLOCK_OFFSET(blockNum)) < 0)
    exitProgram("Can't read indirect block", ERROR_EXIT);

  unsigned int i;
  for (i = 0; i < totalEntries; i++) {
    if (entries[i] != 0) {
      printf("INDIRECT,%u,%u,%u,%u,%u\n",
	     inodeNum,
	     level,
	     fileOffset,
	     blockNum,
	     entries[i]);
      if (level == 2 || level == 3)
	indirect_block_scan(fd, level - 1, inodeNum, entries[i], fileOffset);
    }
    switch(level) {
    case 1:
      fileOffset++;
      break;
    case 2:
      fileOffset += totalEntries;
      break;
    case 3:
      fileOffset += totalEntries * totalEntries;
      break;
    }
  }

  // free entries
  free(entries);
  allocatedCounter--;
}

// inode summary - must occur after free_inode_scan
void used_inode_scan(int fd, struct ext2_super_block *sbPtr, struct ext2_group_desc *gdArr, __u8 *imapArr) {
  unsigned int inodeNum;
  // starts with root node (2), then goes to first unreserved inode
  for (inodeNum = 2; inodeNum < sbPtr->s_inodes_count; inodeNum++) {
    // find which block inode is stored in, and what its index is in that block
    unsigned int blockGroup = (inodeNum - 1) / sbPtr->s_inodes_per_group;
    unsigned int localInodeIndex = (inodeNum - 1) % sbPtr->s_inodes_per_group;

    char byte = imapArr[blockSize * blockGroup + localInodeIndex / 8];
    char used = (byte & (1 << (localInodeIndex % 8))) >> (localInodeIndex % 8); // boolean value

    if (!used) {
      if (inodeNum == 2) inodeNum = sbPtr->s_first_ino - 1;
      continue;
    }

    // Read inode from inode table
    unsigned int offset = BLOCK_OFFSET(gdArr[blockGroup].bg_inode_table) + (inodeNum - 1) * sizeof(struct ext2_inode);
    struct ext2_inode inode;
    if (pread(fd, &inode, sizeof(struct ext2_inode), offset) < 0)
      exitProgram("Cannot read inode", ERROR_EXIT);

    // get inode format of file
    char fileType = '?';
    if (S_ISDIR(inode.i_mode))
      fileType = 'd';
    else if (S_ISREG(inode.i_mode))
      fileType = 'f';
    else if (S_ISLNK(inode.i_mode))
      fileType = 's';

    // get permissions
    __u16 mode = inode.i_mode & (MASK_IRWX_U | MASK_IRWX_G | MASK_IRWX_O);

    // find times
    char ctime[TIME_BUFFER_LEN];
    formatTime(inode.i_ctime, ctime);
    char mtime[TIME_BUFFER_LEN];
    formatTime(inode.i_mtime, mtime);
    char atime[TIME_BUFFER_LEN];
    formatTime(inode.i_atime, atime);

    // print inode entry
    printf("INODE,%d,%c,%o,%u,%u,%u,%s,%s,%s,%u,%u",
	   inodeNum,
	   fileType,
	   mode,
	   inode.i_uid,
	   inode.i_gid,
	   inode.i_links_count,
	   ctime,
	   mtime,
	   atime,
	   inode.i_size,
	   inode.i_blocks);

    // add on block addresses
    int k;
    for (k = 0; k < EXT2_N_BLOCKS; k++)
      printf(",%u", inode.i_block[k]);
    printf("\n");

    // scan directories
    if (fileType == 'd')
      dir_scan(fd, &inode, inodeNum);
    
    // if not directory, check for indirect blocks
    else {
      if (inode.i_block[EXT2_IND_BLOCK])
	indirect_block_scan(fd, 1, inodeNum, inode.i_block[EXT2_IND_BLOCK], EXT2_IND_BLOCK);
      if (inode.i_block[EXT2_DIND_BLOCK])
	indirect_block_scan(fd, 2, inodeNum, inode.i_block[EXT2_DIND_BLOCK], EXT2_IND_BLOCK + blockSize / sizeof(unsigned int));
      if (inode.i_block[EXT2_TIND_BLOCK])
	indirect_block_scan(fd, 3, inodeNum, inode.i_block[EXT2_TIND_BLOCK], EXT2_IND_BLOCK + blockSize / sizeof(unsigned int) + blockSize * blockSize / (sizeof(unsigned int) * sizeof(unsigned int)));
    }

    
    if (inodeNum == 2) inodeNum = sbPtr->s_first_ino - 1; // will be incremented before next iteration
  }
}

// Main routine
int main(int argc, char **argv) {
  int fd = openFile(parseArgs(argc, argv));

  // superblock
  struct ext2_super_block superblock;
  superblock_scan(fd, &superblock);

  // Allocate memory to group descriptor array
  unsigned int groupNum = 1 + (superblock.s_blocks_count - 1) / superblock.s_blocks_per_group;
  struct ext2_group_desc *groupDescriptorArr = malloc(groupNum * sizeof(struct ext2_group_desc));
  allocated[allocatedCounter++] = (void*) groupDescriptorArr;

  // group descriptors
  group_descriptor_table_scan(fd, &superblock, groupDescriptorArr, groupNum);

  // find free blocks
  free_block_scan(fd, &superblock, groupDescriptorArr, groupNum);

  // Allocate mem to inode bitmap array
  __u8 *inodeBitmapArr = malloc(groupNum * superblock.s_inodes_per_group * sizeof(__u8));
  allocated[allocatedCounter++] = (void *) inodeBitmapArr;

  // fill inode bitmap array
  free_inode_scan(fd, &superblock, groupDescriptorArr, groupNum, inodeBitmapArr);

  // scan all allocated inodes
  used_inode_scan(fd, &superblock, groupDescriptorArr, inodeBitmapArr);

  close(fd);
  exitProgram(NULL, SUCCESS_EXIT);
}
