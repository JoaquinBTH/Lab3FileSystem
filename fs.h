#include <iostream>
#include <cstdint>
#include "disk.h"

//Self includes
#include <string.h>  //Strncpy etc...
#include <vector>

#ifndef __FS_H__
#define __FS_H__

#define ROOT_BLOCK 0
#define FAT_BLOCK 1
#define FAT_FREE 0
#define FAT_EOF -1

#define TYPE_FILE 0
#define TYPE_DIR 1
#define READ 0x04
#define WRITE 0x02
#define EXECUTE 0x01

struct dir_entry {
    char file_name[56]; // name of the file / sub-directory
    uint32_t size; // size of the file in bytes
    uint16_t first_blk; // index in the FAT for the first block of the file
    uint8_t type; // directory (1) or file (0)
    uint8_t access_rights; // read (0x04), write (0x02), execute (0x01)
};

//Self made struct
struct directory {
    int block;
    std::string name;
    std::string parent;

    directory() : block(0), name(""), parent("") { }

    directory(const int b, const std::string n, const std::string p) : block(b), name(n), parent(p) { }
};

class FS {
private:
    Disk disk;
    // size of a FAT entry is 2 bytes
    int16_t fat[BLOCK_SIZE/2];

    //Self-made variables
    int currentDirectoryIndex = 0;
    std::vector<directory> *directoryList;

    //Self-made functions
    void updateFat();
    void clearDiskBlock(int blk);
    bool fileExists(int dir, std::string fileName);
    bool directoryExists(int dir, std::string dirName, int& dirBlock);
    bool fileReadable(std::string fileName, int directoryIndex, int &first_blk);
    std::string readFile(std::string fileName, int directoryIndex);
    int copyFile(std::string fileData, std::string destName, int destBlock);
    int moveFile(std::string fileName, std::string newFileName, int sourceBlock, int destBlock, bool rename);
    std::string getAccessRightsAndFirstBlk(std::string fileName, int directoryIndex, int& first_blk);
    void directoryParser(std::string input, std::string& parent, std::string& fullName, std::string& isolatedName);

public:
    FS();
    ~FS();
    // formats the disk, i.e., creates an empty file system
    int format();
    // create <filepath> creates a new file on the disk, the data content is
    // written on the following rows (ended with an empty row)
    int create(std::string filepath);
    // cat <filepath> reads the content of a file and prints it on the screen
    int cat(std::string filepath);
    // ls lists the content in the current directory (files and sub-directories)
    int ls();

    // cp <sourcepath> <destpath> makes an exact copy of the file
    // <sourcepath> to a new file <destpath>
    int cp(std::string sourcepath, std::string destpath);
    // mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
    // or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
    int mv(std::string sourcepath, std::string destpath);
    // rm <filepath> removes / deletes the file <filepath>
    int rm(std::string filepath);
    // append <filepath1> <filepath2> appends the contents of file <filepath1> to
    // the end of file <filepath2>. The file <filepath1> is unchanged.
    int append(std::string filepath1, std::string filepath2);

    // mkdir <dirpath> creates a new sub-directory with the name <dirpath>
    // in the current directory
    int mkdir(std::string dirpath);
    // cd <dirpath> changes the current (working) directory to the directory named <dirpath>
    int cd(std::string dirpath);
    // pwd prints the full path, i.e., from the root directory, to the current
    // directory, including the current directory name
    int pwd();

    // chmod <accessrights> <filepath> changes the access rights for the
    // file <filepath> to <accessrights>.
    int chmod(std::string accessrights, std::string filepath);
};

#endif // __FS_H__
