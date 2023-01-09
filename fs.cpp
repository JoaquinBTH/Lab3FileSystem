#include <iostream>
#include "fs.h"

//Additional include
#include <string.h> //Strcpy etc...
#include <algorithm> //std::copy, std::min
#include <sstream> //std::istringstream

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n"; //Remove
}

FS::~FS()
{

}

//Updates the FAT block on the disk.
void FS::updateFat()
{
    std::string FAT = "";
    for(int i = 0; i < (BLOCK_SIZE / 2); i++)
    {
        FAT += std::to_string(fat[i]) + "\n";
    }
    disk.write(FAT_BLOCK, (uint8_t *)FAT.c_str());
}

//Clears a block on the disk.
void FS::clearDiskBlock(int blk)
{
    char blankBlock[BLOCK_SIZE];
    memset(blankBlock, ' ', BLOCK_SIZE);
    disk.write(blk, (uint8_t *)blankBlock);
}

//Checks if a file exists in the directory or not.
bool FS::fileExists(std::string fileName)
{
    bool rValue = false;

    //Get the data from the current directory and convert it to a string.
    std::string directoryText;
    disk.read(ROOT_BLOCK, (uint8_t *)&directoryText);

    //Check if the fileName exists in the directory
    if(directoryText.find(fileName) != std::string::npos)
    {
        rValue = true;
    }

    return rValue;
}

//Checks if a file has Read access or not.
bool FS::fileReadable(std::string fileName, int &first_blk)
{
    bool rValue = false;

    //Get the data from the current directory and convert it to a string.
    std::string directoryText;
    disk.read(ROOT_BLOCK, (uint8_t *)&directoryText);

    //Define stringstream to read each word after the position of fileName.
    std::istringstream iss(directoryText);
    iss.seekg(directoryText.find(fileName), std::ios_base::beg);

    std::string word;
    iss >> word; //FileName
    iss >> word; //Size
    iss >> word; //First_blk
    first_blk = std::stoi(word);
    iss >> word; //Type
    iss >> word; //Access_right

    if(std::stoi(word) >= 4) //Has to be same or larger than 0x04 in order to have Read access.
    {
        rValue = true;
    }

    return rValue;
}

//Reads all data from a file if it has read access.
std::string FS::readFile(std::string fileName)
{
    std::string rString = "";
    int blk;
    if(fileReadable(fileName, blk))
    {
        //Add content to rString until FAT_EOF is reached.
        std::string content;
        bool eof = false;
        while(!eof)
        {
            disk.read(blk, (uint8_t *)&content);
            rString += content;
            if(fat[blk] == FAT_EOF)
            {
                eof = true;
            }
            else
            {
                blk = fat[blk];
            }
        }
    }
    return rString;
}

// formats the disk, i.e., creates an empty file system
int
FS::format()
{
    std::cout << "FS::format()\n"; //Remove

    //Define a blank block for the initialization of the disk.
    char blankBlock[BLOCK_SIZE];
    memset(blankBlock, ' ', BLOCK_SIZE);

    //Initialize all the blocks in the FAT as free except block 0 and block 1
    for(int i = 0; i < BLOCK_SIZE / 2; i++)
    {
        if(i != 0 || i != 1)
        {
            fat[i] = FAT_FREE;
        }
        else
        {
            fat[i] = FAT_EOF;
        }

        //Initialize the disk
        clearDiskBlock(i);
    }


    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int
FS::create(std::string filepath)
{
    std::cout << "FS::create(" << filepath << ")\n"; //Remove

    //Check if no file with the same name exists.
    if(!fileExists(filepath))
    {
        //Let the user fill up the file data until it notices an empty line.
        std::string fileData;
        std::string input;
        while(std::getline(std::cin, input))
        {
            if(input == "")
            {
                break;
            }
            else
            {
                fileData += input + "\n";
            }
        }

        /*
            1. Check how many disk blocks the file will require.
            2. Search through the disk if the amount of free disk blocks available satisfy the condition.
            3. Create a dir_entry representing the file
            4. Write the dir_entry to the disk block/blocks.
            5. Update the directory block the file is located in.
            6. Update the FAT on the disk.
        */

        //1.
        int reqDiskBlocks = (fileData.size() / BLOCK_SIZE) + 1;

        //2.
        int freeDiskBlockIndex[reqDiskBlocks];
        int arrayIndex = 0;
        for(int i = 2; i < (BLOCK_SIZE / 2); i++) //First two blocks are never available.
        {
            if(fat[i] == FAT_FREE)
            {
                freeDiskBlockIndex[arrayIndex] = i;
                arrayIndex++;

                if(freeDiskBlockIndex[(reqDiskBlocks - 1)] != 0)
                {
                    break;
                }
            }
        }

        if(freeDiskBlockIndex[(reqDiskBlocks - 1)] == 0) //Amount of free disk blocks required couldn't be found.
        {
            std::cout << "Error: Not enough free disk slots" << std::endl;
            return -2;
        }

        //3.
        dir_entry fileEntry;
        strcpy(fileEntry.file_name, filepath.c_str());
        fileEntry.size = fileData.size();
        fileEntry.first_blk = freeDiskBlockIndex[0];
        fileEntry.type = TYPE_FILE;
        fileEntry.access_rights = READ + WRITE;

        //4.
        char blockData[BLOCK_SIZE]; //The block data to transfer.
        int totalFileDataTransfered = 0; //Keeps track of how much file data has been transfered.
        for(int i = 0; i < reqDiskBlocks; i++)
        {
            memset(blockData, ' ', BLOCK_SIZE);
            int fileDataTransfered = std::min(BLOCK_SIZE, static_cast<int>(fileData.size() - totalFileDataTransfered));
            std::copy(fileData.begin() + totalFileDataTransfered, fileData.begin() + totalFileDataTransfered + fileDataTransfered, blockData);
            totalFileDataTransfered += fileDataTransfered;

            disk.write(freeDiskBlockIndex[i], (uint8_t *)blockData);
        }   

        //5.
        std::string dir_addition = fileEntry.file_name + '\n' + std::to_string(fileEntry.size) + '\n' + std::to_string(fileEntry.first_blk) + '\n' +
                        std::to_string(fileEntry.type) + '\n' + std::to_string(fileEntry.access_rights) + '\n';
        char dir_original[BLOCK_SIZE];
        disk.read(ROOT_BLOCK, (uint8_t *)dir_original);
        dir_addition += dir_original;
        clearDiskBlock(ROOT_BLOCK);
        disk.write(ROOT_BLOCK, (uint8_t *)dir_addition.c_str());

        //6.
        for(int i = 0; i < reqDiskBlocks; i++)
        {
            if(i == (reqDiskBlocks - 1))
            {
                fat[freeDiskBlockIndex[i]] = FAT_EOF;
            }
            else
            {
                fat[freeDiskBlockIndex[i]] = fat[freeDiskBlockIndex[(i + 1)]];
            }
        }
        updateFat();
    }
    else
    {
        std::cout << "Error: File already exists." << std::endl;
        return -1;
    }

    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n"; //Remove

    //Check if a file with the name exists
    if(fileExists(filepath))
    {
        std::string fileContent = readFile(filepath);
        if(fileContent != "")
        {
            std::cout << fileContent << std::endl;
        }
        else
        {
            std::cout << "Error: File not readable." << std::endl;
            return -2;
        }
    }
    else
    {
        std::cout << "Error: File doesn't exist." << std::endl;
        return -1;
    }

    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{
    std::cout << "FS::ls()\n"; //Remove
    return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int
FS::cp(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::cp(" << sourcepath << "," << destpath << ")\n"; //Remove
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n"; //Remove
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(std::string filepath)
{
    std::cout << "FS::rm(" << filepath << ")\n"; //Remove
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(std::string filepath1, std::string filepath2)
{
    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n"; //Remove
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int
FS::mkdir(std::string dirpath)
{
    std::cout << "FS::mkdir(" << dirpath << ")\n"; //Remove
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(std::string dirpath)
{
    std::cout << "FS::cd(" << dirpath << ")\n"; //Remove
    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int
FS::pwd()
{
    std::cout << "FS::pwd()\n"; //Remove
    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int
FS::chmod(std::string accessrights, std::string filepath)
{
    std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n"; //Remove
    return 0;
}
