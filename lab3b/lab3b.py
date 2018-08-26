#NAME: Robathan Harries
#ID: 904836501
#EMAIL: r0b@ucla.edu
#SLIPDAYS: 0

import sys
import math


# Define exit codes
EXIT_NO_CORRUPT = 0
EXIT_CORRUPT = 2
EXIT_ERR = 1

# Various EXT2 constants
INODE_DIRECT_BLOCKS = 12
C_UNSIGNED_INT_SIZE = 4 # in bytes


# classes to organize csv entries for each type of csv entry

class Superblock:
    def __init__(self, fileEntry):
        # convert entries from strings to numbers
        self.totalBlocks = int(fileEntry[0])
        self.totalInodes = int(fileEntry[1])
        self.blockSize = int(fileEntry[2])
        self.inodeSize = int(fileEntry[3])
        self.blocksInGroup = int(fileEntry[4])
        self.inodesInGroup = int(fileEntry[5])
        self.firstInode = int(fileEntry[6])
        
        self.blockEntriesPerBlock = self.blockSize / C_UNSIGNED_INT_SIZE        
        
class Group:
    def __init__(self, fileEntry):
        self.groupNum = int(fileEntry[0])        
        self.totalBlocks = int(fileEntry[1])
        self.totalInodes = int(fileEntry[2])
        self.freeBlocks = int(fileEntry[3])
        self.freeInodes = int(fileEntry[4])
        self.blockBMAddr = int(fileEntry[5])
        self.inodeBMAddr = int(fileEntry[6])
        self.firstInodeBlockNum = int(fileEntry[7])
        
class Inode:
    def __init__(self, fileEntry):
        self.inodeNum = int(fileEntry[0])
        self.fileType = fileEntry[1]
        self.mode = fileEntry[2]
        self.owner = int(fileEntry[3])
        self.group = int(fileEntry[4])
        self.linkCount = int(fileEntry[5])
        self.inodeChangeTime = fileEntry[6]
        self.modTime = fileEntry[7]
        self.accessTime = fileEntry[8]
        self.fileSize = int(fileEntry[9])
        self.halfBlocksTaken = int(fileEntry[10])

        # if file is symbolic link with less than 60 bytes, it won't have block pointers        
        if len(fileEntry) > 11:
            # identifies block as having block pointers
            self.hasBlocks = True            
            
            self.blocks = [int(i) for i in fileEntry[11:23]]
            self.sindBlock = int(fileEntry[23]) #singly indirect
            self.dindBlock = int(fileEntry[24]) #double indirect
            self.tindBlock = int(fileEntry[25]) #triple indirect
        
        else:
            self.hasBlocks = False         
        
class IndirectBlock:
    def __init__(self, fileEntry):
        self.inodeNum = int(fileEntry[0])
        self.level = int(fileEntry[1])
        self.offset = int(fileEntry[2])
        self.parentBlockNum = int(fileEntry[3])
        self.blockNum = int(fileEntry[4])
        
class Dirent:
    def __init__(self, fileEntry):
        self.parentInodeNum = int(fileEntry[0])
        self.entryOffset = int(fileEntry[1])
        self.fileInodeNum = int(fileEntry[2])
        self.entryLen = int(fileEntry[3])
        self.nameLen = int(fileEntry[4])
        self.name = fileEntry[5][:-1]
        
        
# class used to audit blocks        
class BlockTracker:
    def __init__(self, superblock, groups, inodes, indirect, bfree):
        self.SB = superblock
        self.groups = groups
        self.inodes = inodes
        self.indirectBlocks = indirect
        self.freeBlocks = bfree
        
        # records whether corruption has been found
        self.errorFound = False
    
    # helper function for checking validity    
    # doesn't trigger for block entries with address 0, which are valid    
    def blockInvalid(self, blockNum):
        if blockNum < 0 or blockNum >= self.SB.totalBlocks:
            return True
        return False
    
    # helper function for checking whether blocks are reserved
    # doesn't trigger for block entries with address 0, which are not reserved
    def blockReserved(self, blockNum):
        if blockNum > 0 and blockNum <= 2:
            return True
        for g in self.groups:
            if blockNum == g.blockBMAddr or blockNum == g.inodeBMAddr:
                return True
            if blockNum >= g.firstInodeBlockNum and blockNum < g.firstInodeBlockNum + math.ceil(g.totalInodes * self.SB.inodeSize / self.SB.blockSize):
                return True
        return False                
    
    # prints corruption lines if it finds them
    def checkInodeBlockPointers(self):
        
        for i in self.inodes:
            # check inode blocks only if inode isn't symbolic link with size less than or equal to 60 bytes
            if i.hasBlocks:
                # check direct blocks
                for blockIndex in range(len(i.blocks)):
                    if self.blockInvalid(i.blocks[blockIndex]):
                        self.errorFound = True
                        print("INVALID BLOCK " + str(i.blocks[blockIndex]) + " IN INODE " + str(i.inodeNum) + " AT OFFSET " + str(blockIndex))                    
                    elif self.blockReserved(i.blocks[blockIndex]):
                        self.errorFound = True
                        print("RESERVED BLOCK " + str(i.blocks[blockIndex]) + " IN INODE " + str(i.inodeNum) + " AT OFFSET " + str(blockIndex))
                
                # check single-indirect block reference
                if self.blockInvalid(i.sindBlock):
                    self.errorFound = True
                    print("INVALID INDIRECT BLOCK " + str(i.sindBlock) + " IN INODE " + str(i.inodeNum) + " AT OFFSET " + str(INODE_DIRECT_BLOCKS))
                elif self.blockReserved(i.sindBlock):
                    self.errorFound = True
                    print("RESERVED INDIRECT BLOCK " + str(i.sindBlock) + " IN INODE " + str(i.inodeNum) + " AT OFFSET " + str(INODE_DIRECT_BLOCKS))
    
                # check double-indirect block reference
                if self.blockInvalid(i.dindBlock):
                    self.errorFound = True
                    print("INVALID DOUBLE INDIRECT BLOCK " + str(i.dindBlock) + " IN INODE " + str(i.inodeNum) + " AT OFFSET " + str(int(INODE_DIRECT_BLOCKS + self.SB.blockEntriesPerBlock)))
                elif self.blockReserved(i.dindBlock):
                    self.errorFound = True
                    print("RESERVED DOUBLE INDIRECT BLOCK " + str(i.dindBlock) + " IN INODE " + str(i.inodeNum) + " AT OFFSET " + str(int(INODE_DIRECT_BLOCKS + self.SB.blockEntriesPerBlock)))
    
                # check triple-indirect block reference
                if self.blockInvalid(i.tindBlock):
                    self.errorFound = True
                    print("INVALID TRIPLE INDIRECT BLOCK " + str(i.tindBlock) + " IN INODE " + str(i.inodeNum) + " AT OFFSET " + str(int(INODE_DIRECT_BLOCKS + self.SB.blockEntriesPerBlock + self.SB.blockEntriesPerBlock ** 2)))
                elif self.blockReserved(i.tindBlock):
                    self.errorFound = True
                    print("RESERVED TRIPLE INDIRECT BLOCK " + str(i.tindBlock) + " IN INODE " + str(i.inodeNum) + " AT OFFSET " + str(int(INODE_DIRECT_BLOCKS + self.SB.blockEntriesPerBlock + self.SB.blockEntriesPerBlock ** 2)))
                
    # prints corruption lines if it finds them
    def checkIndirectBlockPointers(self):
        
        for i in self.indirectBlocks:
            errorStr = ""
            indirectionLevel = ""
            
            if self.blockInvalid(i.blockNum):
                errorStr = "INVALID "
            elif self.blockReserved(i.blockNum):
                errorStr = "RESERVED "
            
            if errorStr != "":
                # record that an error was found
                self.errorFound = True                
                
                # account for indirect blocks in error output
                if i.level == 3:
                    indirectionLevel = "DOUBLE INDIRECT "
                elif i.level == 2:
                    indirectionLevel = "INDIRECT "
                # if this entry is singly indirect, then the error message is for a direct data block, so no indirectionLevel is necessary
            
                # print message
                print(errorStr + indirectionLevel + "BLOCK " + str(i.blockNum) + " IN INODE " + str(i.inodeNum) + " AT OFFSET " + str(i.offset))
    
    # helper function for checkForDuplicates(): prints error messsages for all referenced blocks with address blockNum
    def printDuplicateMessage(self, blockNum):    
        # check inodes        
        for i in self.inodes:            
            if i.hasBlocks:
                # direct blocks            
                for blockIndex in range(len(i.blocks)):
                    if i.blocks[blockIndex] == blockNum:
                        print("DUPLICATE BLOCK " + str(blockNum) + " IN INODE " + str(i.inodeNum) + " AT OFFSET " + str(blockIndex))
                
                # single-indirect blocks
                if i.sindBlock == blockNum:
                    print("DUPLICATE INDIRECT BLOCK " + str(blockNum) + " IN INODE " + str(i.inodeNum) + " AT OFFSET " + str(INODE_DIRECT_BLOCKS))
                
                # double-indirect blocks
                if i.dindBlock == blockNum:
                    print("DUPLICATE DOUBLE INDIRECT BLOCK " + str(blockNum) + " IN INODE " + str(i.inodeNum) + " AT OFFSET " + str(int(INODE_DIRECT_BLOCKS + self.SB.blockEntriesPerBlock)))
                
                # triple-indirect blocks
                if i.tindBlock == blockNum:
                    print("DUPLICATE TRIPLE INDIRECT BLOCK " + str(blockNum) + " IN INODE " + str(i.inodeNum) + " AT OFFSET " + str(int(INODE_DIRECT_BLOCKS + self.SB.blockEntriesPerBlock + self.SB.blockEntriesPerBlock ** 2)))
        
        # check indirects
        for i in self.indirectBlocks:
            if i.blockNum == blockNum:
                indirectLevel = ""
                if i.level == 3:
                    indirectLevel = "DOUBLE INDIRECT "
                elif i.level == 2:
                    indirectLevel = "INDIRECT "
                
                print("DUPLICATE " + indirectLevel + "BLOCK " + str(blockNum) + " IN INODE " + str(i.inodeNum) + " AT OFFSET " + str(i.offset))
    
    # checks for duplicate blocks, prints error messages. 
    # also initializes the allocatedList, which is full of the allocated block numbers
    def checkForDuplicates(self):       
        self.allocatedList = []
        duplicateList = []
        
        # create full list (with duplicates) of block numbers from inodes and indirects
        blocks = [ind.blockNum for ind in self.indirectBlocks]
        for i in self.inodes:
            if i.hasBlocks:
                blocks = blocks + i.blocks + [i.sindBlock, i.dindBlock, i.tindBlock]

        for blockNum in blocks:   
            if blockNum != 0 and blockNum not in duplicateList:
                if blockNum in self.allocatedList:
                    self.printDuplicateMessage(blockNum)
                    duplicateList.append(blockNum)
                    self.errorFound = True
                else:
                    self.allocatedList.append(blockNum)
        
    # prints error messages for blocks both allocated and on free list at once
    # MUST have run checkForDuplicates first to initialize allocatedList
    def checkIncorrectlyFreedBlocks(self):
        for freeBlockNum in self.freeBlocks:
            if freeBlockNum in self.allocatedList:
                self.errorFound = True
                print("ALLOCATED BLOCK " + str(freeBlockNum) + " ON FREELIST")
        
    # checks for unreferenced blocks, printing error messages. 
    # MUST have run checkForDuplicates first to initialize allocatedList
    def checkUnreferencedBlocks(self):        
        for g in self.groups:
            # subtract 2 from totalBlocks for block and inode bitmap, and an extra 3 for blocks 0,1,2, which are reserved on the system - only works for single-group file systems
            blocksInGroup = g.totalBlocks - 2 - math.ceil(g.totalInodes * self.SB.inodeSize / self.SB.blockSize) - 3
            firstBlock = g.firstInodeBlockNum + math.ceil(g.totalInodes * self.SB.inodeSize / self.SB.blockSize)
            for blockNum in [firstBlock + i for i in range(blocksInGroup)]:
                if blockNum not in (self.allocatedList + self.freeBlocks):
                    self.errorFound = True
                    print("UNREFERENCED BLOCK " + str(blockNum))
        
    # main blockTracker function: runs all audits and returns True if corruption found, False otherwise
    def mainCheck(self):
        self.checkInodeBlockPointers()
        self.checkIndirectBlockPointers()
        self.checkForDuplicates()
        self.checkIncorrectlyFreedBlocks()
        self.checkUnreferencedBlocks()
        
        return self.errorFound

# class to perform inode audit
class InodeTracker:
    def __init__(self, superblock, groups, inodes, ifree):
        self.SB = superblock
        self.groups = groups
        self.inodes = inodes
        self.freeInodeNums = ifree
        
        self.errorFound = False
        
    # checks for allocated inodes on freelist, and prints error messages
    def checkIncorrectlyFreedInodes(self):
        for i in self.inodes:
            if i.inodeNum in self.freeInodeNums:
                self.errorFound = True
                print("ALLOCATED INODE " + str(i.inodeNum) + " ON FREELIST")
    
    # checks for unreferenced inodes, and prints error messages
    def checkUnreferencedInodes(self):
        referencedInodes = self.freeInodeNums + [i.inodeNum for i in self.inodes]
        possibleInodes = [2] + list(range(self.SB.firstInode, self.SB.totalInodes + 1))
            
        for inodeNum in possibleInodes:
            if inodeNum not in referencedInodes:
                self.errorFound = True
                print("UNALLOCATED INODE " + str(inodeNum) + " NOT ON FREELIST")
    
    # overall check routine, returns True if corruption found, False otherwise
    def mainCheck(self):
        self.checkIncorrectlyFreedInodes()
        self.checkUnreferencedInodes()
        
        return self.errorFound
    
# class to perform directory audit
class DirectoryTracker:
    def __init__(self, superblock, groups, inodes, dirent, ifree):
        self.SB = superblock
        self.groups = groups
        self.inodes = inodes
        self.dirent = dirent
        self.freeInodes = ifree
        
        self.errorFound = False
     
    # helper function: check validity of inodeNum
    def inodeInvalid(self, inodeNum):
        if inodeNum <= 0 or inodeNum > self.SB.totalInodes:
            return True        
        return False
     
    # check link count, print error messages
    def checkLinkCount(self):
        # create list of 0's to tally up inode references
        linkCount = [0] * (self.SB.totalInodes + 1)
        for d in self.dirent:
            if not self.inodeInvalid(d.fileInodeNum):
                linkCount[d.fileInodeNum] += 1
        
        for i in self.inodes:
            if linkCount[i.inodeNum] != i.linkCount:
                self.errorFound = True
                print("INODE " + str(i.inodeNum) + " HAS " + str(linkCount[i.inodeNum]) + " LINKS BUT LINKCOUNT IS " + str(i.linkCount))
                
    # checks directory inode references, printing error messages if they're invalid or unallocated
    def checkInodeReferences(self):
        for d in self.dirent:
            errorStr = ""
            if self.inodeInvalid(d.fileInodeNum):
                errorStr = "INVALID "
            elif d.fileInodeNum not in [i.inodeNum for i in self.inodes]:
                errorStr = "UNALLOCATED "
                
            if errorStr != "":
                self.errorFound = True
                print("DIRECTORY INODE " + str(d.parentInodeNum) + " NAME " + d.name + " " + errorStr + "INODE " + str(d.fileInodeNum))
    
    # checks default directory entries '.' and '..' for correctness
    def checkDefaultDirents(self):
        for d in self.dirent:
            if d.name == "'.'" and d.parentInodeNum != d.fileInodeNum:
                self.errorFound = True
                print("DIRECTORY INODE " + str(d.parentInodeNum) + " NAME " + d.name + " LINK TO INODE " + str(d.fileInodeNum) + " SHOULD BE " + str(d.parentInodeNum))
            
            if d.name == "'..'":
                # find inode which .. should point to
                if d.parentInodeNum == 2: # if root node, .. points back to root
                    parentDirectoryInode = 2
                else:
                    for d2 in self.dirent:
                        if d2.fileInodeNum == d.parentInodeNum:
                            parentDirectoryInode = d2.parentInodeNum
                            break
                
                if d.fileInodeNum != parentDirectoryInode:
                    self.errorFound = True
                    print("DIRECTORY INODE " + str(d.parentInodeNum) + " NAME " + d.name + " LINK TO INODE " + str(d.fileInodeNum) + " SHOULD BE " + str(parentDirectoryInode))                
        
    # performs all checks, returns True if corruption detected, False otherwise
    def mainCheck(self):
        self.checkLinkCount()
        self.checkInodeReferences()
        self.checkDefaultDirents()
        
        return self.errorFound
    

#Exit with specified exit code, printing given error message
def exitProg(msg, exitCode):
    print(msg, file=sys.stderr)
    sys.exit(exitCode)       
        
        
def main():
    #Parse through command line for file name, then attempt to open file
    if (len(sys.argv) != 2):
        exitProg("Incorrect Format: ./lab3b FileName", EXIT_ERR)

    try:
        with open(sys.argv[1], 'rb') as f:
            #separate csv file into separate separate lists for each type of entry
            SB = []
            group = []
            bfree = []
            ifree = []
            inode = []
            dirent = []
            indirect = []            
            for rawLine in f:
                rawLine = rawLine.decode()
                line = rawLine.split(',')
                if (line[0] == "SUPERBLOCK"):
                    if (SB != []): exitProg("Multiple Superblock entries in file summary!", EXIT_ERR)
                    SB = Superblock(line[1:])
                elif (line[0] == "GROUP"):
                    group.append(Group(line[1:]))
                elif (line[0] == "BFREE"):
                    bfree.append(int(line[1]))
                elif (line[0] == "IFREE"):
                    ifree.append(int(line[1]))
                elif (line[0] == "INODE"):
                    inode.append(Inode(line[1:]))
                elif (line[0] == "DIRENT"):
                    dirent.append(Dirent(line[1:]))
                elif (line[0] == "INDIRECT"):
                    indirect.append(IndirectBlock(line[1:]))
                else:
                    exitProg("Incorrectly formatted file summary line: " + rawLine, EXIT_ERR)
                    
                    
            
            blockCheck = BlockTracker(SB, group, inode, indirect, bfree)
            inodeCheck = InodeTracker(SB, group, inode, ifree)
            dirCheck = DirectoryTracker(SB, group, inode, dirent, ifree)
            
            errorFound = False
            if blockCheck.mainCheck():
                errorFound = True
            if inodeCheck.mainCheck():
                errorFound = True
            if dirCheck.mainCheck():
                errorFound = True
            
    except IOError:
        exitProg("Could not open file: " + sys.argv[1], EXIT_ERR)

    # exit with correct exit code
    if errorFound: 
        sys.exit(EXIT_CORRUPT)
    else:
        sys.exit(EXIT_NO_CORRUPT)
    
if __name__ == '__main__':   
    main()