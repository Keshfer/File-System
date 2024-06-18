#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <vector>
#include <map>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"
#include <bitset>


using namespace std;
bool compareDirEntries(const dir_ent_t* a, const dir_ent_t* b) {
  int val = strcmp(a->name, b->name);
  if(val <= 0) {
    return true;
  } else {
    return false;
  }
}
void list(dir_ent_t *dir, LocalFileSystem *fileSys, string path) {
  //cout<< "num to fetch is " << dir->inum << endl; 
  inode_t *inode = new inode_t;
  fileSys->stat(dir->inum, inode);
  /*super_t *super = new super_t;
  inode_t *inodeArray = new inode_t[super->num_inodes];
  
  fileSys->readInodeRegion(super, inodeArray);
  for(int i =0; i < super->num_inodes; i++) {
    cout<< inodeArray[i].size<<" ";
  }
  cout<<endl;
  */
  if(inode->type != UFS_DIRECTORY) {
    return;
  }
  //printf("my name is %s\n", dir->name);
  //printf("my inode is %d\n", dir->inum);
  //printf("my file size is %d\n", inode->size);
  char *buffer = new char[inode->size];
  fileSys->read(dir->inum, buffer, inode->size);
  int numOfEntries = (inode->size)/sizeof(dir_ent_t);
  //delete inode;
  //printf("num of entries is %d", numOfEntries);
  vector<dir_ent_t*> exploreDir;
  cout<< "Directory ";
  cout<<path<<endl;
  vector<dir_ent_t*> dirEntries;
  //map<string, dir_ent_t*> stringEntry;
  //dir_ent_t *temp = NULL;
  for(int i =0; i < numOfEntries; i++) {
    dir_ent_t *entry = new dir_ent_t;
    memcpy(entry, buffer, sizeof(dir_ent_t));
    buffer += sizeof(dir_ent_t);
    dirEntries.push_back(entry);

  }
  sort(dirEntries.begin(), dirEntries.end(), compareDirEntries);
  for(size_t i =0; i < dirEntries.size(); i++) {
    //cout<< stringEntry[dirEntries[i]]->inum << "\t" << dirEntries[i] << endl;
    cout << dirEntries[i]->inum << "\t" << dirEntries[i]->name << endl;
    if(strcmp(dirEntries[i]->name, ".") != 0 && strcmp(dirEntries[i]->name, "..") != 0) { //directories to explore excluding . and ..
      exploreDir.push_back(dirEntries[i]);
    }
  }
  
  //printf("\n");
  //printing out the entry that didn't get print in the for loop
  //cout<< temp->inum << "\t" << temp->name << endl; 
  cout<< endl;
  for(long unsigned int i =0; i < exploreDir.size(); i++) {
    string dirName(exploreDir[i]->name);
    string updatedPath(path + dirName + "/");
    list(exploreDir[i], fileSys, updatedPath);
  }
  
  
  return;
  
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    cout << argv[0] << ": diskImageFile" << endl;
    return 1;
  }
  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSys = new LocalFileSystem(disk);

  dir_ent_t root = {
    "/",
    0,
  };
  string path = root.name;
  list(&root, fileSys, path);
}