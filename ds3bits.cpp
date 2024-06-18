#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;


int main(int argc, char *argv[]) {
  if (argc != 2) {
    cout << argv[0] << ": diskImageFile" << endl;
    return 1;
  }
  //cout << argv[1] << endl;
  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSys = new LocalFileSystem(disk);
  super_t *superBlock = new super_t;
  fileSys->readSuperBlock(superBlock);
  int inodeRegionAddr = superBlock->inode_region_addr;
  int dataRegionAddr = superBlock->data_region_addr;

  unsigned char *inodeBitmap= new unsigned char[superBlock->inode_bitmap_len * UFS_BLOCK_SIZE];
  fileSys->readInodeBitmap(superBlock, inodeBitmap);
  unsigned char *dataBitmap = new unsigned char[superBlock->num_inodes * UFS_BLOCK_SIZE];
  fileSys->readDataBitmap(superBlock, dataBitmap);
  
  cout << "Super" << endl;
  cout << "inode_region_addr " << inodeRegionAddr << endl;
  cout << "data_region_addr " << dataRegionAddr << endl;
  cout<<endl;
  //cout<<superBlock->num_inodes<<endl;
  cout<< "Inode bitmap" << endl;
  for(int i = 0; i < superBlock->inode_bitmap_len * UFS_BLOCK_SIZE; i++) {
    cout<< (unsigned int)inodeBitmap[i] << " ";
    /*
    int integer = (unsigned int)inodeBitmap[i];
    while(integer > 1) {
      cout<< integer % 2 << " ";
      integer = integer / 2;
    }
    if(integer == 1) {
      cout<< 1 << " ";
    } else {
      cout << 0 << " ";
    }
    */
  }
  cout << endl;
  cout << endl;
  cout<< "Data bitmap" << endl;
  for(int i =0; i < superBlock->data_bitmap_len * UFS_BLOCK_SIZE; i++) {
    cout<< (unsigned int)dataBitmap[i]<< " ";
    /*
    int integer = (unsigned int)dataBitmap[i];
    while(integer > 1) {
      cout<< integer %2 << " ";
      integer = integer /2;
    }
    if(integer == 1) {
      cout << 1 << " ";
    } else {
      cout << 0 << " ";
    }
    */
  }
  cout<<endl;
  
}
