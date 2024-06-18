#include <iostream>
#include <string>
#include <vector>
#include <assert.h>
#include <cstring>
#include <unistd.h>
#include <bitset>

#include "LocalFileSystem.h"
#include "ufs.h"

using namespace std;


LocalFileSystem::LocalFileSystem(Disk *disk) {
  this->disk = disk;
}
int AllocateDiskBlock(LocalFileSystem *fileSys, super_t *super, unsigned int *diskBlockNum) {
    unsigned char* dataBitmap = new unsigned char[super->data_bitmap_len * UFS_BLOCK_SIZE];
    fileSys->readDataBitmap(super, dataBitmap);
    cout<<"before changes to data bit map in allocate disk block"<<endl;
    for(int i =0; i < super->data_bitmap_len * UFS_BLOCK_SIZE; i++) {
      cout<< (unsigned int)dataBitmap[i]<<" ";
    }
    cout<<endl;
    //vector<int> dataBits;
    unsigned int dataInteger;
    int dataByteNum;
    unsigned int diskNum;
    for(int i =0; i < super->data_bitmap_len * UFS_BLOCK_SIZE; i++) {
      if(dataBitmap[i] != 255) { //inodeBitmap[i] does not have binary of 11111111 so that means there is a 0 somewhere in the binary
        dataInteger = (unsigned int)dataBitmap[i];
        dataByteNum = i;
        break;
      }
      if(i > super->num_data) {
        return -ENOTENOUGHSPACE;
      }
    }
    bitset<8> dataBits(dataInteger);
    int bitPos;
    for(unsigned long int i =0; i < dataBits.size(); i++) {
      if(dataBits[i] == false) {
        dataBits[i] = true; //change from 0 to 1
        bitPos = i;
        break;
      }
    }
    diskNum = (dataByteNum * 8) + (bitPos);
    //diskNum = 7 - diskNum; //IMPORTANT readDiskBlock reads the diskblock positions from left to right. bitset reads the positions from right to left. 
    //so diskblock 5 in bitset is diskblock 2 on readdiskblock
    diskNum += super->data_region_addr; // we offset by the data region address to get the actual disk block numbers. This is to avoid returning important disk blocks such as inode region, bit maps, superblock, etc
    dataBitmap[dataByteNum] = (int)(dataBits.to_ulong());
    cout<<"after changes to data bit map in allocate disk block"<<endl;
    for(int i =0; i < super->data_bitmap_len * UFS_BLOCK_SIZE; i++) {
      cout<< (unsigned int)dataBitmap[i]<<" ";
    }
    cout<<endl;
    fileSys->writeDataBitmap(super, dataBitmap);
    /*
    //find the 0 bit in the byte
    while(dataInteger > 0) {
      if(dataInteger == 1) {
        //cerr<<"somehow we went through the whole binary format without finding a 0(create dataBitmap)" << endl;
        dataBits.push_back(dataInteger);
        break;
      }
      if(dataInteger == 0) {
        dataBits.push_back(dataInteger);
        break;
      }
      int remainder = dataInteger % 2;
      dataBits.push_back(remainder);
      if(remainder == 0) { //found an available disk block so no need to keep searching
        break;
      }
      dataInteger = dataInteger / 2;
    }
    if(dataBits[dataBits.size() - 1] == 1 && dataBits.size() < 8) {
      dataBits.push_back(0);
    }
    diskNum = (dataByteNum * 8) + (dataBits.size() - 1);
    dataBits.at(dataBits.size() - 1) = 1; //change from 0 to 1
      
    string dataBinaryString = "";
    for(auto i = dataBits.rbegin(); i != dataBits.rend(); ++i) {
      dataBinaryString.append(to_string(*i));
    }
    //converting binaryString to decimal
    unsigned int dataDecVal = 0;
    int dataModifier = 1;
    for(long unsigned int i = 0; i < dataBinaryString.length(); i++) {
      if(dataBinaryString[i] != '0' && dataBinaryString[i] != '1') {
        cerr << "binaryString has a non 1 or 0 character"<<endl;
      }
      if(dataBinaryString[i] == '1') {
        dataDecVal += dataModifier;
      }
      dataModifier = dataModifier * 2;
    }
    dataBitmap[dataByteNum] = dataDecVal;
    fileSys->writeDataBitmap(super, dataBitmap);
    */
    *diskBlockNum = diskNum; 
    cout<<"this disk block was allocated "<< *diskBlockNum<<endl;

    //memcpy(diskBlockNum, &diskNum, sizeof(unsigned int));
    delete[] dataBitmap;
    unsigned char* test = new unsigned char[super->data_bitmap_len * UFS_BLOCK_SIZE];
    fileSys->readDataBitmap(super, test);
    cout<<"after storing changes in allocate disk block"<<endl;
    for(int i =0; i < super->data_bitmap_len * UFS_BLOCK_SIZE; i++) {
      cout<< (unsigned int)test[i]<<" ";
    }
    cout<<endl;
    return 0;
}
void LocalFileSystem::readSuperBlock(super_t *super) {
  char *buffer = new char [UFS_BLOCK_SIZE]; //maybe use char type
  disk->readBlock(0, buffer);
  memcpy(super, buffer, sizeof(super_t));
}

int LocalFileSystem::lookup(int parentInodeNumber, string name) {
  inode_t *inode = new inode_t;
  int status = stat(parentInodeNumber, inode);
  if(status != 0 || inode->type != UFS_DIRECTORY) {
    return -EINVALIDINODE;
  }
  char *buffer = new char[inode->size];
  read(parentInodeNumber, buffer, inode->size);
  int numOfEntries = inode->size/sizeof(dir_ent_t);
  for(int i =0; i < numOfEntries; i++) {
    dir_ent_t *entry = new dir_ent_t;
    memcpy(entry, buffer, sizeof(dir_ent_t));
    buffer += sizeof(dir_ent_t);
    int result = strcmp(entry->name, name.c_str());
    if( result == 0) { //check if directory entry is the entry we are looking for
    
      return entry->inum;
    }
  }
  return -ENOTFOUND;
}

int LocalFileSystem::stat(int inodeNumber, inode_t *inode) {
  super_t *superBlock = new super_t;
  readSuperBlock(superBlock);
  inode_t *inodesArray = new inode_t[superBlock->num_inodes];

  readInodeRegion(superBlock, inodesArray);
  inode_t *selectInode = &inodesArray[inodeNumber];
  if(inodeNumber < 0) { //negative inode number
    return -EINVALIDINODE;
  }
  if(inodeNumber >= superBlock->num_inodes) { //requesting an inode that is beyond the number of inodes
    return -EINVALIDINODE;
  }
  unsigned char *inodeBitmap = new unsigned char[superBlock->inode_bitmap_len * UFS_BLOCK_SIZE];
  readInodeBitmap(superBlock, inodeBitmap);
  int bitmapIndex = inodeNumber / 8; //each element in the bit map  holds 8 bits/inode statuses. Example: 0-7 is under index 0
  size_t bitmapOffset = inodeNumber % 8;
  bitset<8> inodeBits(inodeBitmap[bitmapIndex]);

  /*int integer = inodeBitmap[bitmapIndex];
  vector<int>bitFormat;
  while(integer >= 0) {
    if(integer == 1) {
      bitFormat.push_back(integer);
      break;
    } 
    if(integer == 0) {
      bitFormat.push_back(integer);
      break;
    }
    int remainder = integer % 2;
    bitFormat.push_back(remainder);
    integer = integer / 2;

  }
  */
  /*
  if(bitFormat.size() <= bitmapOffset) {
    return -EINVALIDINODE;
  }
  */
  if(inodeBits[bitmapOffset] == 0) {
    return -EINVALIDINODE;
  }
  memcpy(inode, selectInode, sizeof(inode_t));
  return 0;
}

int LocalFileSystem::read(int inodeNumber, void *buffer, int size) {

  inode_t *selectInode = new inode_t;
  
  int status = stat(inodeNumber, selectInode);
  
  if(status != 0) {
    return status; // -EINVALIDINODE
  }
  if(inodeNumber < 0) {
    return -EINVALIDINODE;
  }
  if(size < 0 || size > MAX_FILE_SIZE) { // attempting to read more bytes than the bytes that make up selectInode's file
    return -EINVALIDSIZE;
  }
  int bytesToRead;
  if(selectInode->size < size) {
    bytesToRead = selectInode->size;
  } else { //selectInode->size > size
    bytesToRead = size;
  }
  int directIndex =0;
  char *charBuffer;
  /*
  if(selectInode->type == UFS_REGULAR_FILE) {
    charBuffer = new char[size + 1];
    bytesToRead += 1; // accounts for \0
  } else { //UFS_DIRECTORY 
    charBuffer = new char[size];
  }
  */
  charBuffer = new char[size];
  char *charBufferPtr = charBuffer;
  //int index =0;
  while( bytesToRead > 0) {
    char tempBuffer[UFS_BLOCK_SIZE];
    disk->readBlock(selectInode->direct[directIndex], tempBuffer);
    if(bytesToRead <= UFS_BLOCK_SIZE) {
      memcpy(charBufferPtr, tempBuffer, bytesToRead);
      charBufferPtr += bytesToRead;
      bytesToRead -= bytesToRead;
      directIndex++;
      //memcpy(buffer, tempBuffer, size);
    } else { // bytesToRead > UFS_BLOCK_SIZE
      memcpy(charBufferPtr, tempBuffer, UFS_BLOCK_SIZE);
      //index += UFS_BLOCK_SIZE;
      charBufferPtr+=UFS_BLOCK_SIZE;

      bytesToRead -= UFS_BLOCK_SIZE;
      directIndex++;
    }
  }
  /*
  if(selectInode->type == UFS_REGULAR_FILE) {
    memcpy(buffer, charBuffer, size + 1);
  } else {// UFS_DIRECTORY
    memcpy(buffer, charBuffer, size);
  }
  */
  memcpy(buffer, charBuffer, size);
  return size;
}

int LocalFileSystem::create(int parentInodeNumber, int type, string name) {
  disk->beginTransaction();
  if(name.length() + 1 > 28) { // +1 is for the \0 which is not included in the length call
    disk->rollback();
    return -EINVALIDNAME;
  }
  inode_t *parentInode = new inode_t;
  int status = stat(parentInodeNumber, parentInode);
  if(status == -EINVALIDINODE) {
    disk->rollback();
    return status; //-EINVALIDINODE
  }
  int nameInodeNum = lookup(parentInodeNumber, name);
  if(nameInodeNum != -ENOTFOUND) { //file with name already exists within the parentInode
    inode_t *nameInode = new inode_t;
    stat(nameInodeNum, nameInode);
    if(nameInode->type != type ) {
      //return error
      disk->rollback();
      return -EINVALIDTYPE;
    } else {
      //return success
      disk->rollback();
      return nameInodeNum;
      
    }
  
  } else { //file doesn't exist in the directory so create
    //need to find an available inode num
    //need to make dir_ent_t to put into parent directory
    //need to make an inode with the appropriate name and type
    inode_t *newInode = new inode_t; //inode will have type, size of 0, and no directs. This is due to having no data associated with file/directory
    newInode->type = type;
    newInode->size = 0;
    for(int i =0; i < DIRECT_PTRS; i++) {
      newInode->direct[i] = -1; //invalidating all unused directs
    }
    //update inode bitmap
    super_t *super = new super_t;
    readSuperBlock(super);
    unsigned char* inodeBitmap = new unsigned char[super->inode_bitmap_len * UFS_BLOCK_SIZE];
    readInodeBitmap(super, inodeBitmap);
    //vector<int> inodeBits;
    unsigned int integer;
    int byteNum;
    int inodeNum = -1;
    for(int i = 0; i < super->inode_bitmap_len * UFS_BLOCK_SIZE; i++) {
      if(inodeBitmap[i] != 255) { //inodeBitmap[i] does not have binary of 11111111 so that means there is a 0 somewhere in the binary

        integer = (unsigned int)inodeBitmap[i];
        byteNum = i;
        break;
      }
      if(i > super->num_inodes) {
        disk->rollback();
        return -ENOTENOUGHSPACE;
      }
    }
    int bitPos;
    bitset<8> inodeBits(integer);
    for(unsigned long int i =0; i < inodeBits.size(); i++) {
      if(inodeBits[i] == false) {
        inodeBits[i] = true;
        bitPos = i;
        break;
      }
    }
    inodeNum = (byteNum * 8) + (bitPos); 
    inodeBitmap[byteNum] = (int)(inodeBits.to_ulong());
    writeInodeBitmap(super, inodeBitmap);

    if(type == UFS_DIRECTORY) { //need to allocate a disk block to hold . and ..
      unsigned int diskNum;
      int status = AllocateDiskBlock(this, super, &diskNum);
      
      if(status != 0) {
        disk->rollback();
        return status; //-EINOTENOUGHSPACE
      }
      newInode->direct[0] = diskNum;
      dir_ent_t *curDir = new dir_ent_t;
      char dot[DIR_ENT_NAME_SIZE] = ".";
      strcpy(curDir->name, dot);
      curDir->inum = inodeNum;

      dir_ent_t *prevDir = new dir_ent_t;
      char dot2[DIR_ENT_NAME_SIZE] = "..";
      strcpy(prevDir->name, dot2);
      prevDir->inum = parentInodeNumber;

      newInode->size = 2 * sizeof(dir_ent_t); //. and .. are dir_ent_t so they take up 2 dir_ent_t worth of space
      //placing the data of curDir and prevDir into a buffer so I can use disk->writeblock to put the data into the allocated disk block
      char *insertBuffer = new char[UFS_BLOCK_SIZE];
      char *insertBufferPtr = insertBuffer;
      memcpy(insertBufferPtr, curDir, sizeof(dir_ent_t));//placing curDir data into bufferPtr
      insertBufferPtr += sizeof(dir_ent_t); //shifting pointer to the tail end of the added data
      memcpy(insertBufferPtr, prevDir, sizeof(dir_ent_t));
      int diskNumInt = (int)diskNum;
      disk->writeBlock(diskNumInt, insertBuffer);
    }



    //making dir entry for parent Inode to hold onto in its own direct
    dir_ent_t *newEntry = new dir_ent_t;
    strcpy(newEntry->name, name.c_str());
    newEntry->inum = inodeNum;

    //grabbing parentInode's latest direct's disk block data, shifting to the end of the data, and appending newEntry
    //first we check to see if we need to allocate a new block
    int directIndex = (parentInode->size + sizeof(dir_ent_t)) / UFS_BLOCK_SIZE;
    unsigned int diskBlock = parentInode->direct[directIndex];
    if(diskBlock == (unsigned int)-1) { //if diskBlock is -1, allocate a disk block for it
      int status = AllocateDiskBlock(this, super, &diskBlock);
      if(status != 0) {
        disk->rollback();
        return status; //-ENOTENOUGHSPACE
      }
    }
    //read from appropriate block, appending newEntry's data to it and then writing it back to the same block 
    char *directBuffer = new char[UFS_BLOCK_SIZE];
    char *directBufferPtr = directBuffer;
    disk->readBlock(diskBlock, directBuffer);
    directBufferPtr += parentInode->size % UFS_BLOCK_SIZE;
    memcpy(directBufferPtr, newEntry, sizeof(dir_ent_t));
    disk->writeBlock(diskBlock, directBuffer);

    //read in inode region, insert newInode into index of inodeNum
    inode_t *inodeArray = new inode_t[super->num_inodes];
    readInodeRegion(super, inodeArray);
    memcpy(&(inodeArray[inodeNum]), newInode, sizeof(inode_t));
    //also increase parentInode->size by sizeof(dir_ent_t)
    parentInode->size += sizeof(dir_ent_t);
    memcpy(&(inodeArray[parentInodeNumber]), parentInode, sizeof(inode_t));
    writeInodeRegion(super, inodeArray);
    disk->commit();
    return inodeNum;
  }
  
}

int LocalFileSystem::write(int inodeNumber, const void *buffer, int size) {
  disk->beginTransaction();
  super_t *super = new super_t;
  readSuperBlock(super);
  inode_t *inode = new inode_t;
  int status = stat(inodeNumber, inode);
  /*
  for(int i =0; i < 30; i++) {
    cout<< inode->direct[i]<< " ";
  }
  cout<< endl;
  */
  if(status < 0) {
    disk->rollback();
    return status; //-EINVALIDINODE
  }
  if(size < 0 || size > MAX_FILE_SIZE) {
    disk->rollback();
    return -EINVALIDSIZE;
  }
  if(inode->type != UFS_REGULAR_FILE) {
    disk->rollback();
    return -EINVALIDTYPE;
  }
  int diskBlocksToUse = size / UFS_BLOCK_SIZE + 1;
  if(diskBlocksToUse > DIRECT_PTRS){
    disk->rollback();
    return -ENOTENOUGHSPACE;
  }
  /*
  if(diskBlocksToUse > oldDiskBlockCount) {
    int allocateNum = diskBlocksToUse - oldDiskBlockCount;
    int directIndex = oldDiskBlockCount;
    for(int i =0; i < allocateNum; i++) {
      unsigned int newDataBlockNum;
      int status = AllocateDiskBlock(this, super, &newDataBlockNum);
      if(status != 0) {
        disk->rollback();
        return status; // -ENOTENOUGHSPACE
      }
      inode->direct[directIndex]  = newDataBlockNum;
      directIndex++;
    }
  } else if(diskBlocksToUse < oldDiskBlockCount) {
    int invalidateNum = oldDiskBlockCount - diskBlocksToUse;
    int directIndex = oldDiskBlockCount - 1;
    unsigned char *dataBitmap = new unsigned char[super->data_bitmap_len * UFS_BLOCK_SIZE];
    readDataBitmap(super, dataBitmap);
    int index = inode->direct[directIndex] / 8;
    int bitPos = inode->direct[directIndex] % 8;
    inode->direct[directIndex] = -1;
    for(int i =0; i < invalidateNum; i++) {
      int integer = dataBitmap[index];
      bitset<8> dataBits(integer);
      dataBits[bitPos] = false;
      int val = (int)(dataBits.to_ulong());
      dataBitmap[index] = val;
      writeDataBitmap(super, dataBitmap);

      
    }
    directIndex--;
  }
  */
  inode->size = size;

  int bytesToWrite = size;
      
  char *charBuffer = new char[size]; 
  memcpy(charBuffer, buffer, size); //copy buffer data to charBuffer so I can do pointer arithmetic
  char *charBufferPtr = charBuffer; // points to the same location as buffer
  for(int i =0; i < diskBlocksToUse; i++) {
    
    if(inode->direct[i] == (unsigned int)-1) {
      //find an available disk block on the data bitmap and set direct[i] to that disk block number
      unsigned int newDataBlockNum;
      int status = AllocateDiskBlock(this, super, &newDataBlockNum);
      if(status != 0) {
        disk->rollback();
        return status; //-ENOTENOUGHSPACE
      }
      inode->direct[i] = newDataBlockNum;

    }
    
    //writes size number of buffer bytes to inode->direct[i]
    if(bytesToWrite > UFS_BLOCK_SIZE) {
      char *bufferToWrite = new char[UFS_BLOCK_SIZE];
      memcpy(bufferToWrite, charBufferPtr, UFS_BLOCK_SIZE);
      disk->writeBlock(inode->direct[i], bufferToWrite);
      charBufferPtr += UFS_BLOCK_SIZE;
      bytesToWrite -= UFS_BLOCK_SIZE;
      
      if(i + 1 <= DIRECT_PTRS) {
        inode->direct[i + 1] = -1; // invalidating the next direct element in the event that there are bytesToWrite leftover
      }
      
      
    } else { //bytesToWrite is less than UFS_BLOCK_SIZE
      char *bufferToWrite = new char[bytesToWrite];
      memcpy(bufferToWrite, charBufferPtr, bytesToWrite);
      disk->writeBlock(inode->direct[i], bufferToWrite);
      charBufferPtr += bytesToWrite;
      bytesToWrite -= bytesToWrite;
      //no need for a break statement here since we should be on the last block to write to
    }
  }
  inode_t *inodeArray = new inode_t[super->num_inodes];
  readInodeRegion(super, inodeArray);
  inode_t *updatedInodes = new inode_t[super->num_inodes];
  for(int i = 0; i < super->num_inodes; i++) {
    if(i == inodeNumber) {
      updatedInodes[i] = *inode;
    } else {
      updatedInodes[i] = inodeArray[i];
    }
  }
  delete[] inodeArray;

  writeInodeRegion(super, updatedInodes);

  disk->commit();
  return size;
}

int LocalFileSystem::unlink(int parentInodeNumber, string name) {
  disk->beginTransaction();
  inode_t *parentInode = new inode_t;
  int status = stat(parentInodeNumber, parentInode);
  if(status < 0) {
    disk->rollback();
    return -EINVALIDINODE;
  }
  if(name.length() + 1 > 28) { // +1 is for the \0 which is not included in the length call
    disk->rollback();
    return -EINVALIDNAME;
  }
  if(strcmp(name.c_str(), ".") == 0 || strcmp(name.c_str(), "..") == 0) {
    disk->rollback();
    return -EUNLINKNOTALLOWED;
  }
  int numOfEntries = parentInode->size / sizeof(dir_ent_t);
  char *parentBuffer = new char[parentInode->size];
  read(parentInodeNumber, parentBuffer, parentInode->size); 
  char *updateBuffer = new char[parentInode->size];
  char *updateBufferPtr = updateBuffer;
  int freeInum = -1;
  for(int i = 0; i < numOfEntries; i++) {
    dir_ent_t *entry = new dir_ent_t;
    memcpy(entry,parentBuffer, sizeof(dir_ent_t));
    if(strcmp(entry->name, name.c_str()) != 0){
      memcpy(updateBufferPtr, entry, sizeof(dir_ent_t));
      updateBufferPtr += sizeof(dir_ent_t);
    } else {
      freeInum = entry->inum;
      inode_t *entryInode = new inode_t;
      stat(entry->inum, entryInode);
      if(entryInode->type == UFS_DIRECTORY && (long unsigned int)entryInode->size > 2*sizeof(dir_ent_t)) {
        disk->rollback();
        return -EDIRNOTEMPTY;
      }
    }
    parentBuffer += sizeof(dir_ent_t);
  }
  //updating parentInode's size
  if(freeInum != 1) {
    parentInode->size -= sizeof(dir_ent_t);
    super_t *super = new super_t;
    readSuperBlock(super);
    inode_t *inodeArray = new inode_t[super->num_inodes];
    readInodeRegion(super,inodeArray);

    memcpy(&(inodeArray[parentInodeNumber]), parentInode, sizeof(inode_t));
    writeInodeRegion(super, inodeArray);

    readInodeRegion(super,inodeArray);

  }

  dir_ent_t *dirList = new dir_ent_t[numOfEntries - 1];
  memcpy(dirList, updateBuffer, parentInode->size );
  
  
  if(freeInum != 1) {
    int diskBlocksToUse = (parentInode->size)/ UFS_BLOCK_SIZE + 1;
    
    int bytesToWrite = parentInode->size;
    char *insertBuffer = new char[bytesToWrite];
    memcpy(insertBuffer, updateBuffer, bytesToWrite);
    char *insertBufferPtr = insertBuffer;
    //writing update dir entries to parent inode's directs
    
    int count =0;
    
    while(bytesToWrite > 0) {
      if(bytesToWrite >= UFS_BLOCK_SIZE) {
        char *bufferToWrite = new char[UFS_BLOCK_SIZE];
        memcpy(bufferToWrite, insertBufferPtr, UFS_BLOCK_SIZE);
        disk->writeBlock(parentInode->direct[count], bufferToWrite);
        count++;
        updateBufferPtr += UFS_BLOCK_SIZE;
        bytesToWrite -= UFS_BLOCK_SIZE;
      } else { //bytesToWrite is less then UFS_BLOCK_SIZE
        char *bufferToWrite = new char[bytesToWrite];
        memcpy(bufferToWrite, insertBufferPtr, bytesToWrite);
        disk->writeBlock(parentInode->direct[count], bufferToWrite);
        count++;
        updateBufferPtr += bytesToWrite;
        bytesToWrite -= bytesToWrite;
      }
    }
    
    /*
    for(int i =0; i < diskBlocksToUse; i++) {
      if(bytesToWrite > UFS_BLOCK_SIZE) {
        char *bufferToWrite = new char[UFS_BLOCK_SIZE];
        memcpy(bufferToWrite, insertBufferPtr, UFS_BLOCK_SIZE);
        disk->writeBlock(parentInode->direct[i], bufferToWrite);
        insertBufferPtr += UFS_BLOCK_SIZE;
        bytesToWrite -= UFS_BLOCK_SIZE;
      } else { //bytesToWrite is less than UFS_BLOCK_SIZE
        char *bufferToWrite = new char[bytesToWrite];
        memcpy(bufferToWrite, insertBufferPtr, bytesToWrite);
        //cout<<"index " << i << "is disk block "<< inode->direct[i]<<endl;
        disk->writeBlock(parentInode->direct[i], bufferToWrite);
        insertBufferPtr += bytesToWrite;
        bytesToWrite -= bytesToWrite;
        //no need for a break statement here since we should be on the last block to write to
      }
    }
    */   
    //update data bit map
    if(diskBlocksToUse < (parentInode->size / UFS_BLOCK_SIZE + 1)) {//checks if there is a disk block we can free
      int directIndex = parentInode->size/UFS_BLOCK_SIZE;
      int freeBlock = parentInode->direct[directIndex];
      super_t *super = new super_t;
      readSuperBlock(super);
      unsigned char *dataBitmap = new unsigned char[super->data_bitmap_len * UFS_BLOCK_SIZE];
      readDataBitmap(super, dataBitmap);
      int index = freeBlock / 8;
      int bitPos = freeBlock % 8;
      bitset<8> dataBits(dataBitmap[index]);
      dataBits[bitPos] = false;
      int val = (int)(dataBits.to_ulong());
      dataBitmap[index] = val;
      writeDataBitmap(super, dataBitmap);
      /*
      int integer = dataBitmap[index];
      vector<int> dataBits;
      while(integer >= 0) {
        if(integer == 1) {
          dataBits.push_back(integer);
          break;
        } 
        if(integer == 0) {
          dataBits.push_back(integer);
          break;
        }
        int remainder = integer % 2;
        dataBits.push_back(remainder);
        if(remainder == 0) {
          break; //found an available inode number and it is inodeBits.size()  
        }
        integer = integer / 2;
      }
      while(dataBits.size() < 8) {
        dataBits.push_back(0);
      }
      dataBits.at(bitPos) = 0;
      string dataBinaryString ="";
      for(auto i = dataBits.rbegin(); i != dataBits.rend(); ++i) {
        dataBinaryString.append(to_string(*i));
      }
      unsigned int decVal = 0;
      int modifier = 1;
      for(long unsigned int i = 0; i < dataBinaryString.length(); i++) {
          if(dataBinaryString[i] != '0' && dataBinaryString[i] != '1') {
            cerr << "binaryString has a non 1 or 0 character"<<endl;
          }
          if(dataBinaryString[i] == '1') {
            decVal += modifier;
          
          }
          modifier = modifier * 2;
      }
      dataBitmap[index] = decVal;
      */
      
    }
  }
  
  //update inode bitmap
  if(freeInum != -1) {
    super_t *super = new super_t;
    readSuperBlock(super);
    unsigned char *inodeBitmap = new unsigned char[super->inode_bitmap_len * UFS_BLOCK_SIZE];
    readInodeBitmap(super, inodeBitmap);
    int index = freeInum / 8;// gives us index for the inode Bit map array
    int bitPos = freeInum % 8; //gives us the bit position of the integer we search
    bitset<8> inodeBits(inodeBitmap[index]);
    inodeBits[bitPos] = false;
    int val = (int)(inodeBits.to_ulong());
    inodeBitmap[index] = val;
    writeInodeBitmap(super, inodeBitmap);
    /*
    int integer = inodeBitmap[index];
    vector<int> inodeBits;
    while(integer >= 0) {
        if(integer == 1) {
          inodeBits.push_back(integer);
          break;
        } 
        if(integer == 0) {
          inodeBits.push_back(integer);
          break;
        }
        int remainder = integer % 2;
        inodeBits.push_back(remainder);
        if(remainder == 0) {
          break; //found an available inode number and it is inodeBits.size()  
        }
        integer = integer / 2;
    }
    while(inodeBits.size() < 8) {
      inodeBits.push_back(0);
    }

    inodeBits.at(bitPos) = 0;
    string inodeBinaryString = "";
    for(auto i = inodeBits.rbegin(); i != inodeBits.rend(); ++i) {
      inodeBinaryString.append(to_string(*i));
    }
    cout<<"binary string "<<inodeBinaryString<<endl;
    unsigned int decVal = 0;
    int modifier = 1;
    for(long unsigned int i = 0; i < inodeBinaryString.length(); i++) {
        if(inodeBinaryString[i] != '0' && inodeBinaryString[i] != '1') {
          cerr << "binaryString has a non 1 or 0 character"<<endl;
        }
        if(inodeBinaryString[i] == '1') {
          decVal += modifier;
          
        }
        modifier = modifier * 2;
    }
    inodeBitmap[index] = decVal;
    */

  }

  disk->commit();
  return 0;
}
void LocalFileSystem::readInodeRegion(super_t *super, inode_t *inodes) {
  int inodeRegionAddr = super->inode_region_addr;
  int inodeRegionLength = super->inode_region_len;
  unsigned char *readBuffer = new unsigned char[UFS_BLOCK_SIZE * inodeRegionLength]; //readBuffer points to a new char array (stores the address)
  unsigned char *bufferPtr = readBuffer; //bufferPtr also points to the char array (stores the same address)
  //inode_t *inodesPtr = inodes;
  //int index =0;
  for(int i = 0; i < inodeRegionLength; i++) {
    disk->readBlock(inodeRegionAddr, bufferPtr);
    //index += UFS_BLOCK_SIZE;
    //bufferPtr = &(bufferPtr[index]); //if this doesn't work, try bufferPtr += UFS_BLOCK_SIZE
    //memcpy(inodesPtr, bufferPtr, UFS_BLOCK_SIZE);
    bufferPtr += UFS_BLOCK_SIZE;
    inodeRegionAddr += 1;
    
    
    //inodesPtr += UFS_BLOCK_SIZE;
  }
  //memcpy(inodes, readBuffer, UFS_BLOCK_SIZE * inodeRegionLength);
  memcpy(inodes, readBuffer, super->num_inodes * sizeof(inode_t));
  /*
  for(int i = 0; i < UFS_BLOCK_SIZE * inodeRegionLength; i++) {
    cout<< appendBuffer[i];
  }
  cout<< endl;
  */
}
void LocalFileSystem::writeInodeRegion(super_t *super, inode_t *inodes) {
  //expecting inodes to be an array of inodes, already edited to be the updated inode region.
  //write the data into a buffer using memcpy then disk->writeBlock them into super->inode_region_addr
  
  char *buffer = new char[UFS_BLOCK_SIZE * super->inode_region_len];
  inode_t *temp = inodes;
  //char *bufferPtr = buffer;
  //inode_t *inodesPtr = inodes;
  //putting all the inodes' data into the buffer
  int size = UFS_BLOCK_SIZE * super->inode_region_len;
  memcpy(buffer, temp, size);
  /*
  long unsigned int count = 0;
  while(count < sizeof(inodes)) {
    memcpy(bufferPtr, inodesPtr, sizeof(inode_t));
    bufferPtr += sizeof(inode_t);
    inodesPtr += sizeof(inode_t);
    count += sizeof(inode_t);
  }
  */
  int inodeRegionAddr = super->inode_region_addr;
  //writes the buffer into the inode Region. The for loop handles the case where the inode region is multiple blocks
  for(int i =0; i < super->inode_region_len; i++) {
    disk->writeBlock(inodeRegionAddr, buffer);
    inodeRegionAddr += 1;
    buffer += UFS_BLOCK_SIZE;
  }
}
void LocalFileSystem::readInodeBitmap(super_t *super, unsigned char *inodeBitmap) {
  int inodeBitMapAddr = super->inode_bitmap_addr;
  int inodeBitMapAddrLength = super->inode_bitmap_len;
  //char **bufferArray[inodeBitMapAddrLength];
  char readBuffer[UFS_BLOCK_SIZE];
  char appendBuffer[UFS_BLOCK_SIZE * inodeBitMapAddrLength] = "";
  //char *appendBuffer = new char[UFS_BLOCK_SIZE * inodeBitMapAddrLength];
  //char *appendBufferPtr = appendBuffer;
  for(int i =0; i < inodeBitMapAddrLength; i++) {
    
    disk->readBlock(inodeBitMapAddr, readBuffer);
    sprintf(&appendBuffer[strlen(appendBuffer)], "%s", readBuffer);
    //memcpy(appendBufferPtr, readBuffer, UFS_BLOCK_SIZE);
    //appendBufferPtr += UFS_BLOCK_SIZE;
    inodeBitMapAddr += 1;
  }
  memcpy(inodeBitmap, appendBuffer, (UFS_BLOCK_SIZE * inodeBitMapAddrLength));
}
void LocalFileSystem::writeInodeBitmap(super_t *super, unsigned char *inodeBitmap) {
  int inodeBitmapAddr = super->inode_bitmap_addr;
  int inodeBitmapAddrLength = super->inode_bitmap_len;
  unsigned char *inodeBitmapPtr = inodeBitmap;
  for(int i =0; i< inodeBitmapAddrLength; i++) {
    disk->writeBlock(inodeBitmapAddr, inodeBitmapPtr);
    inodeBitmapAddr += 1;
    inodeBitmapPtr += UFS_BLOCK_SIZE;
  }
}
void LocalFileSystem::readDataBitmap(super_t *super, unsigned char *dataBitmap) {
  int dataBitmapAddr = super->data_bitmap_addr;
  int dataBitmapAddrLength = super->data_bitmap_len;

  char readBuffer[UFS_BLOCK_SIZE];
  char appendBuffer[UFS_BLOCK_SIZE * dataBitmapAddrLength] = "";
  for(int i =0; i < dataBitmapAddrLength; i++) {
    disk->readBlock(dataBitmapAddr, readBuffer);
    sprintf(&appendBuffer[strlen(appendBuffer)], "%s", readBuffer);
    dataBitmapAddr +=1;
  }
  memcpy(dataBitmap, appendBuffer, (UFS_BLOCK_SIZE * dataBitmapAddrLength));
}
void LocalFileSystem::writeDataBitmap(super_t *super, unsigned char *dataBitmap) {
  int dataBitmapAddr = super->data_bitmap_addr;
  int dataBitmapAddrLength = super->data_bitmap_len;
  unsigned char *dataBitmapPtr = dataBitmap;
  for(int i =0; i < dataBitmapAddrLength; i++) {
    disk->writeBlock(dataBitmapAddr, dataBitmapPtr);
    //in the event the dataBitmap takes 1+ disk block, incremement dataBitmapAddr to move to the next block and add UFS_BLOCK_SIZE to dataBitmap to write remaining bytes

    dataBitmapAddr += 1;
    dataBitmapPtr += UFS_BLOCK_SIZE;
  }
  
}