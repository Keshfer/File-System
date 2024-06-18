#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sstream>
#include <iostream>
#include <map>
#include <string>
#include <algorithm>
#include <string.h>

#include "DistributedFileSystemService.h"
#include "ClientError.h"
#include "ufs.h"
#include "WwwFormEncodedDict.h"

using namespace std;
bool compareDirEntries(const dir_ent_t* a, const dir_ent_t* b) {
  int val = strcmp(a->name, b->name);
  if(val <= 0) {
    return true;
  } else {
    return false;
  }
}
DistributedFileSystemService::DistributedFileSystemService(string diskFile) : HttpService("/ds3/") {
  this->fileSystem = new LocalFileSystem(new Disk(diskFile, UFS_BLOCK_SIZE));
}  

void DistributedFileSystemService::get(HTTPRequest *request, HTTPResponse *response) {
  vector<string>list = request->getPathComponents();
  int inodeNum = 0;
  for(long unsigned int i =0; i < list.size(); i++) {
    //cout<<list[i]<< endl;
    int result;
    if(strcmp(list[i].c_str(), "ds3") == 0) {
      result = 0;
    } else {
      result = fileSystem->lookup(inodeNum, list[i]);
    }

    if(result >= 0) {
      inodeNum = result;
    } else {
      throw ClientError::notFound();
    }
  }
  inode_t *inode = new inode_t;
  int status = fileSystem->stat(inodeNum,inode);
  if(status < 0) {
    throw ClientError::badRequest();
  }
  char *buffer = new char[inode->size];
  int readStatus = fileSystem->read(inodeNum, buffer, inode->size);
  if(readStatus < 0) {
    throw ClientError::badRequest();
  }
  vector<dir_ent_t*> entries;
  if(inode->type == UFS_DIRECTORY) {
    int bytes = inode->size;
    char *bufferPtr = buffer;
    while(bytes > 0) {
      dir_ent_t *entry = new dir_ent_t;
      memcpy(entry, bufferPtr, sizeof(dir_ent_t));
      bytes -= sizeof(dir_ent_t);
      bufferPtr += sizeof(dir_ent_t);
      if(strcmp(entry-> name, ".") != 0 && strcmp(entry->name, "..") != 0) {
        entries.push_back(entry);    
      }
    }
    sort(entries.begin(), entries.end(), compareDirEntries);
    string bodyString ="";
    for(long unsigned int i =0; i < entries.size(); i++) {
      //cout<<entries[i]->name<<endl;
      inode_t *tempInode = new inode_t;
      fileSystem->stat(entries[i]->inum, tempInode);
      if(tempInode->type == UFS_DIRECTORY) {
        string temp = "";
        temp.append(entries[i]->name);
        temp.append("/");
        temp.append("\n");
        bodyString.append(temp);
      } else {
        string temp ="";
        temp.append(entries[i]->name);
        temp.append("\n");
        bodyString.append(temp);
      }
    }
    response->setBody(bodyString);
  } else {
    response->setBody(buffer);
  }

  //response->setBody("");
}

void DistributedFileSystemService::put(HTTPRequest *request, HTTPResponse *response) {
  vector<string> list = request->getPathComponents();
  int inodeNum =0;
  for(unsigned long int i =0; i < list.size(); i++) {
    int result;
    if(strcmp(list[i].c_str(),"ds3") == 0 ) {
      result = 0;
    } else {
      result = fileSystem->lookup(inodeNum, list[i]);
    }
    if(result >= 0) {
      inodeNum = result;
    } else {
      int createResult;
      if(i != list.size()- 1) {
        createResult =  fileSystem->create(inodeNum, UFS_DIRECTORY, list[i]);
      } else { //i is the last element in list
        createResult = fileSystem->create(inodeNum, UFS_REGULAR_FILE, list[i]);
      } 
      if(createResult < 0) {
        throw ClientError::conflict;
      } else {
        inodeNum = createResult;
      }
    }
    
  }
  inode_t *inode = new inode_t;
  int status = fileSystem->stat(inodeNum,inode);
  if(status < 0) {
    throw ClientError::badRequest();
  }
  
  string content = request->getBody();
  fileSystem->write(inodeNum,content.c_str(), content.size() + 1);
  
  response->setBody("");
  
}

void DistributedFileSystemService::del(HTTPRequest *request, HTTPResponse *response) {
  vector<string> list = request->getPathComponents();
  int inodeNum = 0;
  for(unsigned int i = 0; i < list.size() - 1; i++) {
    int result;
    if(strcmp(list[i].c_str(), "ds3") == 0) {
      result = 0;
    } else {
      result = fileSystem->lookup(inodeNum, list[i]);
    }
    if(result >= 0) {
      inodeNum = result;
    } else {
      throw ClientError::notFound();
    }
  }
  string deleteElement;
  if(strcmp(list[list.size() - 1].c_str(), "ds3") != 0) {
    deleteElement = list[list.size() - 1];
  } else {
    throw ClientError::badRequest();
  }
  int check = fileSystem->lookup(inodeNum, list[list.size() - 1]); //checking to see if thing to delete exists
  if(check < 0) {
    throw ClientError::notFound();
  }
  int status = fileSystem->unlink(inodeNum, list[list.size() -1]);
  if(status < 0) {
    throw ClientError::badRequest();
  }
  response->setBody("");
}
