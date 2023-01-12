#include <iostream>
#include "fs.h"

// Additional include
#include <algorithm> //std::copy, std::min
#include <sstream>   //std::istringstream

FS::FS()
{
    directoryList = new std::vector<directory>();

    // Take out the FAT
    char FATtext[BLOCK_SIZE];
    disk.read(FAT_BLOCK, (uint8_t *)&FATtext);
    char issText[BLOCK_SIZE + 1];
    memcpy(issText, FATtext, BLOCK_SIZE);
    issText[BLOCK_SIZE] = '\0';
    std::istringstream iss(issText);
    std::string word;
    int index = 0;
    while (iss >> std::ws && std::getline(iss, word))
    {
        fat[index] = std::stoi(word);
        index++;
    }

    // Take out all the directories stored in the data from before.
    directoryList->clear();
    directoryList->push_back(directory(ROOT_BLOCK, "/", ""));

    dir_entry currentEntry;
    std::string dirName;
    std::string parentName;

    char directoryText[BLOCK_SIZE];
    for (int i = 0; i < directoryList->size(); i++)
    {
        iss.clear();
        memset(directoryText, '\0', BLOCK_SIZE);
        disk.read(directoryList->at(i).block, (uint8_t *)&directoryText);
        memset(issText, '\0', BLOCK_SIZE + 1);
        memcpy(issText, directoryText, BLOCK_SIZE);
        issText[BLOCK_SIZE] = '\0';
        iss.str(issText);
        word = "";
        index = 0;

        while (iss >> std::ws && std::getline(iss, word))
        {
            switch (index)
            {
            case 0:
                strncpy(currentEntry.file_name, word.c_str(), sizeof(currentEntry.file_name) - 1);
                break;
            case 1:
                currentEntry.size = std::stoi(word);
                break;
            case 2:
                currentEntry.first_blk = std::stoi(word);
                break;
            case 3:
                currentEntry.type = std::stoi(word);
                break;
            case 4:
                currentEntry.access_rights = std::stoi(word);
                break;
            }

            if (index == 4 && (int)currentEntry.type == 1)
            {
                dirName = "";
                parentName = "";
                for (int i = 0; currentEntry.file_name[i] != '\0'; i++)
                {
                    dirName.append(1, currentEntry.file_name[i]);
                }
                if (directoryList->at(i).block == ROOT_BLOCK)
                {
                    dirName = directoryList->at(i).name + dirName;
                }
                else
                {
                    dirName = directoryList->at(i).name + "/" + dirName;
                }
                parentName = directoryList->at(i).name;

                directoryList->push_back(directory((int)currentEntry.first_blk, dirName, parentName));

                index = 0;
            }
            else if (index == 4)
            {
                index = 0;
            }
            else
            {
                index++;
            }
        }
    }
}

FS::~FS()
{
}

// Updates the FAT block on the disk.
void FS::updateFat()
{
    clearDiskBlock(FAT_BLOCK);
    std::string FAT = "";
    for (int i = 0; i < (BLOCK_SIZE / 2); i++)
    {
        FAT += std::to_string(fat[i]) + "\n";
    }
    disk.write(FAT_BLOCK, (uint8_t *)FAT.c_str());
}

// Clears a block on the disk.
void FS::clearDiskBlock(int blk)
{
    char blankBlock[BLOCK_SIZE];
    memset(blankBlock, '\0', BLOCK_SIZE);
    disk.write(blk, (uint8_t *)&blankBlock);
}

// Checks if a file exists in the directory or not.
bool FS::fileExists(int dir, std::string fileName)
{
    bool rValue = false;

    // Get the data from the current directory and convert it to a string.
    char directoryText[BLOCK_SIZE];
    disk.read(dir, (uint8_t *)&directoryText);
    char issText[BLOCK_SIZE + 1];
    memcpy(issText, directoryText, BLOCK_SIZE);
    issText[BLOCK_SIZE] = '\0';
    std::string dirText = issText; // String needed for .find function.

    // Define stringstream to read each word after the position of fileName.
    std::istringstream iss(issText);
    iss.seekg(dirText.find(fileName), std::ios_base::beg);
    std::string type;
    for (int i = 0; i < 4; i++)
    {
        iss >> type;
    }

    // Check if the fileName exists in the directory
    if ((dirText.find(fileName) != std::string::npos) && type == "0")
    {
        rValue = true;
    }

    return rValue;
}

// Checks if a directory with dirName exists in the dir block.
bool FS::directoryExists(int dir, std::string dirName, int &dirBlock)
{
    bool rValue = false;

    // Get the data from the current directory and convert it to a string.
    char directoryText[BLOCK_SIZE];
    disk.read(dir, (uint8_t *)&directoryText);
    char issText[BLOCK_SIZE + 1];
    memcpy(issText, directoryText, BLOCK_SIZE);
    issText[BLOCK_SIZE] = '\0';

    // Define stringstream to read each word after the position of fileName.
    std::istringstream iss(issText);
    if (iss.str().find(dirName) != std::string::npos)
    {
        iss.seekg(iss.str().find(dirName), std::ios_base::beg);
        std::string word;
        for (int i = 0; i < 3; i++)
        {
            iss >> word;
        }
        dirBlock = std::stoi(word);
        iss >> word;

        if (word == "1") // If the type is directory
        {
            rValue = true;
        }
    }

    return rValue;
}

// Checks if a file has Read access or not.
bool FS::fileReadable(std::string fileName, int directoryIndex, int &first_blk)
{
    bool rValue = false;

    // Get the data from the current directory and convert it to a string.
    char directoryText[BLOCK_SIZE];
    disk.read(directoryList->at(directoryIndex).block, (uint8_t *)&directoryText);
    char issText[BLOCK_SIZE + 1];
    memcpy(issText, directoryText, BLOCK_SIZE);
    issText[BLOCK_SIZE] = '\0';
    std::string dirText = issText; // String needed for .find function.

    // Define stringstream to read each word after the position of fileName.
    std::istringstream iss(issText);
    iss.seekg(dirText.find(fileName), std::ios_base::beg);

    std::string word;
    iss >> word; // FileName
    iss >> word; // Size
    iss >> word; // First_blk
    first_blk = std::stoi(word);
    iss >> word; // Type
    iss >> word; // Access_right

    if (std::stoi(word) >= 4) // Has to be same or larger than 0x04 in order to have Read access.
    {
        rValue = true;
    }

    return rValue;
}

// Reads all data from a file if it has read access.
std::string FS::readFile(std::string fileName, int directoryIndex)
{
    std::string rString = "";
    int blk;
    if (fileReadable(fileName, directoryIndex, blk))
    {
        // Add content to rString until FAT_EOF is reached.
        char diskContent[BLOCK_SIZE];
        char addContent[BLOCK_SIZE + 1];
        bool eof = false;
        while (!eof)
        {
            disk.read(blk, (uint8_t *)&diskContent);
            memcpy(addContent, diskContent, BLOCK_SIZE);
            addContent[BLOCK_SIZE] = '\0';
            std::istringstream iss(addContent);

            std::string word;
            while (iss >> word)
            {
                rString += word + iss.str()[iss.tellg()];
            }
            word.pop_back(); // Erase the last space.

            if (fat[blk] == FAT_EOF)
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

// Copies the fileData into the block of destDir with destName as the file name.
int FS::copyFile(std::string fileData, std::string destName, int destBlock)
{
    // Check if the fileName is too big for the buffer.
    if (destName.size() > 55)
    {
        std::cout << "Error: Filename too big" << std::endl;
        return -4;
    }

    // Check if the directory is full or not (64 entries max per directory).
    char directoryText[BLOCK_SIZE];
    disk.read(destBlock, (uint8_t *)&directoryText);
    char issText[BLOCK_SIZE + 1];
    memcpy(issText, directoryText, BLOCK_SIZE);
    issText[BLOCK_SIZE] = '\0';
    std::istringstream iss(issText);
    std::string word;
    int entries = 0;
    while (iss >> word)
    {
        entries++;
    }
    if (entries / 5 > 63)
    {
        std::cout << "Error: Directory full" << std::endl;
        return -5;
    }

    /*
        1. Check how many disk blocks the file will require.
        2. Search through the disk if the amount of free disk blocks available satisfy the condition.
        3. Create a dir_entry representing the file
        4. Write the dir_entry to the disk block/blocks.
        5. Update the directory block the file is located in.
        6. Update the FAT on the disk.
    */

    // 1.
    int reqDiskBlocks = (fileData.size() / BLOCK_SIZE) + 1;

    // 2.
    int freeDiskBlockIndex[reqDiskBlocks];
    for (int i = 0; i < reqDiskBlocks; i++)
    {
        freeDiskBlockIndex[i] = 0;
    }
    int arrayIndex = 0;
    for (int i = 2; i < (BLOCK_SIZE / 2); i++) // First two blocks are never available.
    {
        if (fat[i] == FAT_FREE)
        {
            freeDiskBlockIndex[arrayIndex] = i;
            arrayIndex++;

            if (freeDiskBlockIndex[(reqDiskBlocks - 1)] != 0)
            {
                break;
            }
        }
    }

    if (freeDiskBlockIndex[(reqDiskBlocks - 1)] == 0) // Amount of free disk blocks required couldn't be found.
    {
        std::cout << "Error: Not enough free disk slots" << std::endl;
        return -6;
    }

    // 3.
    dir_entry fileEntry;
    strncpy(fileEntry.file_name, destName.c_str(), sizeof(fileEntry.file_name) - 1);
    fileEntry.size = fileData.size();
    fileEntry.first_blk = freeDiskBlockIndex[0];
    fileEntry.type = TYPE_FILE;
    fileEntry.access_rights = READ + WRITE;

    // 4.
    char blockData[BLOCK_SIZE];      // The block data to transfer.
    int totalFileDataTransfered = 0; // Keeps track of how much file data has been transfered.
    for (int i = 0; i < reqDiskBlocks; i++)
    {
        memset(blockData, ' ', BLOCK_SIZE);
        int fileDataTransfered = std::min(BLOCK_SIZE, static_cast<int>(fileData.size() - totalFileDataTransfered));
        std::copy(fileData.begin() + totalFileDataTransfered, fileData.begin() + totalFileDataTransfered + fileDataTransfered, blockData);
        totalFileDataTransfered += fileDataTransfered;

        disk.write(freeDiskBlockIndex[i], (uint8_t *)blockData);
    }

    // 5.
    std::string dirNew;
    for (int i = 0; fileEntry.file_name[i] != '\0'; i++)
    {
        dirNew.append(1, fileEntry.file_name[i]);
    }
    dirNew += '\n' + std::to_string(fileEntry.size) + '\n' + std::to_string(fileEntry.first_blk) + '\n' +
              std::to_string(fileEntry.type) + '\n' + std::to_string(fileEntry.access_rights) + '\n';
    char dirOriginal[BLOCK_SIZE];
    disk.read(destBlock, (uint8_t *)&dirOriginal);
    dirNew += dirOriginal;
    clearDiskBlock(destBlock);
    disk.write(destBlock, (uint8_t *)dirNew.c_str());

    // 6.
    for (int i = 0; i < reqDiskBlocks; i++)
    {
        if (i == (reqDiskBlocks - 1))
        {
            fat[freeDiskBlockIndex[i]] = FAT_EOF;
        }
        else
        {
            fat[freeDiskBlockIndex[i]] = fat[freeDiskBlockIndex[(i + 1)]];
        }
    }
    updateFat();

    return 0;
}

// Moves or renames a file and puts it in disk block representing destDir.
int FS::moveFile(std::string fileName, std::string newFileName, int sourceBlock, int destBlock, bool rename)
{
    // Check if the newFileName is too big for the buffer.
    if (newFileName.size() > 55)
    {
        std::cout << "Error: Filename too big" << std::endl;
        return -3;
    }

    if (rename) // Reconstruct the directory and change the fileName to newFileName
    {
        char directoryText[BLOCK_SIZE];
        disk.read(sourceBlock, (uint8_t *)&directoryText);
        char issText[BLOCK_SIZE + 1];
        memcpy(issText, directoryText, BLOCK_SIZE);
        issText[BLOCK_SIZE] = '\0';

        std::istringstream iss(issText);
        std::string word;
        std::string dirNew;

        while (iss >> word)
        {
            if (word == fileName)
            {
                dirNew += newFileName + "\n";
            }
            else
            {
                dirNew += word + "\n";
            }
        }

        clearDiskBlock(sourceBlock);
        disk.write(sourceBlock, (uint8_t *)dirNew.c_str());
    }
    else // Reconstruct the directory that we're taking from to not have the file we want to move and then reconstruct the destDir to have that file instead.
    {
        // First check if the destDir has enough space
        char directoryText[BLOCK_SIZE];
        disk.read(destBlock, (uint8_t *)&directoryText);
        char issText[BLOCK_SIZE + 1];
        memcpy(issText, directoryText, BLOCK_SIZE);
        issText[BLOCK_SIZE] = '\0';
        std::istringstream iss(issText);
        std::string word;
        int entries = 0;
        while (iss >> word)
        {
            entries++;
        }
        if (entries / 5 > 63)
        {
            std::cout << "Error: Destination directory full" << std::endl;
            return -4;
        }

        // Now reconstruct currentDirectory to not have the file and keep track of the file entry.
        iss.clear();
        memset(directoryText, ' ', BLOCK_SIZE);
        memset(issText, ' ', BLOCK_SIZE + 1);
        disk.read(sourceBlock, (uint8_t *)&directoryText);
        memcpy(issText, directoryText, BLOCK_SIZE);
        issText[BLOCK_SIZE] = '\0';

        iss.str(issText);
        word = "";
        std::string dirNew;
        std::string fileEntryInfo;
        int index = 0;

        // If fileName is found, put the file info into fileEntryInfo, otherwise fill the dirNew.
        while (iss >> word)
        {
            if (word == fileName)
            {
                fileEntryInfo += word + "\n";
                index++;
            }
            else if (index == 0)
            {
                dirNew += word + "\n";
            }
            else
            {
                fileEntryInfo += word + "\n";
                index++;
                if (index == 5)
                {
                    index = 0;
                }
            }
        }

        clearDiskBlock(sourceBlock);
        disk.write(sourceBlock, (uint8_t *)dirNew.c_str());

        // Now reconstruct the destDir to have the file in it.
        char dirOriginal[BLOCK_SIZE];
        disk.read(destBlock, (uint8_t *)&dirOriginal);
        fileEntryInfo += dirOriginal;
        clearDiskBlock(destBlock);
        disk.write(destBlock, (uint8_t *)fileEntryInfo.c_str());
    }

    return 0;
}

// Scans the currentDirectory until the file with fileName is found and returns its read/write/execute rights.
std::string FS::getAccessRightsAndFirstBlk(std::string fileName, int directoryIndex, int &first_blk)
{
    std::string rValue = "";

    char directoryText[BLOCK_SIZE];
    disk.read(directoryList->at(directoryIndex).block, (uint8_t *)&directoryText);
    char issText[BLOCK_SIZE + 1];
    memcpy(issText, directoryText, BLOCK_SIZE);
    issText[BLOCK_SIZE] = '\0';

    std::istringstream iss(issText);
    iss.seekg(iss.str().find(fileName), std::ios_base::beg);
    std::string word;
    iss >> word; // Name
    iss >> word; // Size
    iss >> word; // FirstBlk
    first_blk = std::stoi(word);
    iss >> word; // Type
    iss >> word; // AccessRights

    switch (std::stoi(word))
    {
    case 0:
        rValue = "---";
        break;
    case 1:
        rValue = "--X";
        break;
    case 2:
        rValue = "-W-";
        break;
    case 3:
        rValue = "-WX";
        break;
    case 4:
        rValue = "R--";
        break;
    case 5:
        rValue = "R-X";
        break;
    case 6:
        rValue = "RW-";
        break;
    case 7:
        rValue = "RWX";
        break;
    }

    return rValue;
}

// Parses input into parent, fullName and isolatedName so they can be used to compare or add to the directoryList.
void FS::directoryParser(std::string input, std::string &parent, std::string &fullName, std::string &isolatedName)
{
    std::vector<std::string> parts;
    std::string currentString = "";

    for (int i = 0; i < input.size(); i++)
    {
        if (input[i] == '/') // Whenever "/" is encountered.
        {
            if (currentString == ".." && parts.size() > 0) // If ".." was found and we can go back one directory, then do so.
            {
                parts.pop_back();
            }
            else if (currentString != "") // If currentString is not empty we add it to the parts.
            {
                parts.push_back(currentString);
            }
            currentString = "";
        }
        else // Add letter to the string.
        {
            currentString += input[i];
        }
    }

    if (currentString == ".." && parts.size() > 0)
    {
        parts.pop_back();
    }
    else if (currentString != "")
    {
        parts.push_back(currentString);
    }

    parent = "/";
    if (parts.size() > 0)
    {
        for (int i = 0; i < parts.size() - 1; i++)
        {
            if (i == 0)
            {
                parent += parts[i];
            }
            else
            {
                parent += "/" + parts[i];
            }
        }

        isolatedName = parts[parts.size() - 1];

        if (parent[parent.size() - 1] != '/') // Parent isn't Root
        {
            fullName = parent + "/" + isolatedName;
        }
        else // Parent is root
        {
            fullName = parent + isolatedName;
        }
    }
    else
    {
        parent = "";
        fullName = "/";
        isolatedName = "/";
    }
}

// formats the disk, i.e., creates an empty file system
int FS::format()
{
    // Initialize all the blocks in the FAT as free except block 0 and block 1
    for (int i = 0; i < BLOCK_SIZE / 2; i++)
    {
        if (i != 0 && i != 1)
        {
            fat[i] = FAT_FREE;
        }
        else
        {
            fat[i] = FAT_EOF;
        }

        // Initialize the disk
        clearDiskBlock(i);
    }

    updateFat();
    currentDirectoryIndex = 0;
    directoryList->clear();
    directoryList->push_back(directory(ROOT_BLOCK, "/", ""));

    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int FS::create(std::string filepath)
{
    if (filepath[0] != '/') // Add a slash if the first letter is missing it for similar parsing.
    {
        filepath = "/" + filepath;
        if (directoryList->at(currentDirectoryIndex).block != ROOT_BLOCK)
        {
            filepath = directoryList->at(currentDirectoryIndex).name + filepath;
        }
    }

    std::string fullFilePath;
    std::string parentDirectory;
    std::string fileName;

    directoryParser(filepath, parentDirectory, fullFilePath, fileName);

    // Find the directory we want to create the file in.
    bool found = false;
    int directoryIndex;
    for (int i = 0; i < directoryList->size(); i++)
    {
        if (parentDirectory == directoryList->at(i).name)
        {
            directoryIndex = i;
            found = true;
            break;
        }
    }

    if (!found || fileName.find("..") != std::string::npos)
    {
        std::cout << "Error: Directory doesn't exist" << std::endl;
        return -1;
    }

    int ref;
    // Check if no file or directory with the same name exists.
    if (!fileExists(directoryList->at(directoryIndex).block, fileName) && !directoryExists(directoryList->at(directoryIndex).block, fileName, ref))
    {
        // Check if the fileName is too big for the buffer.
        if (fileName.size() > 55)
        {
            std::cout << "Error: Filename too big" << std::endl;
            return -3;
        }

        // Check if the directory is full or not (64 entries max per directory).
        char directoryText[BLOCK_SIZE];
        disk.read(directoryList->at(directoryIndex).block, (uint8_t *)&directoryText);
        char issText[BLOCK_SIZE + 1];
        memcpy(issText, directoryText, BLOCK_SIZE);
        issText[BLOCK_SIZE] = '\0';
        std::istringstream iss(issText);
        std::string word;
        int entries = 0;
        while (iss >> word)
        {
            entries++;
        }
        if (entries / 5 > 63)
        {
            std::cout << "Error: Directory full" << std::endl;
            return -4;
        }

        // Let the user fill up the file data until it notices an empty line.
        std::string fileData;
        std::string input;
        while (std::getline(std::cin, input))
        {
            if (input == "")
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

        // 1.
        int reqDiskBlocks = (fileData.size() / BLOCK_SIZE) + 1;

        // 2.
        int freeDiskBlockIndex[reqDiskBlocks];
        for (int i = 0; i < reqDiskBlocks; i++)
        {
            freeDiskBlockIndex[i] = 0;
        }
        int arrayIndex = 0;
        for (int i = 2; i < (BLOCK_SIZE / 2); i++) // First two blocks are never available.
        {
            if (fat[i] == FAT_FREE)
            {
                freeDiskBlockIndex[arrayIndex] = i;
                arrayIndex++;

                if (freeDiskBlockIndex[(reqDiskBlocks - 1)] != 0)
                {
                    break;
                }
            }
        }

        if (freeDiskBlockIndex[(reqDiskBlocks - 1)] == 0) // Amount of free disk blocks required couldn't be found.
        {
            std::cout << "Error: Not enough free disk slots" << std::endl;
            return -5;
        }

        // 3.
        dir_entry fileEntry;
        strncpy(fileEntry.file_name, fileName.c_str(), sizeof(fileEntry.file_name) - 1);
        fileEntry.size = fileData.size();
        fileEntry.first_blk = freeDiskBlockIndex[0];
        fileEntry.type = TYPE_FILE;
        fileEntry.access_rights = READ + WRITE;

        // 4.
        char blockData[BLOCK_SIZE];      // The block data to transfer.
        int totalFileDataTransfered = 0; // Keeps track of how much file data has been transfered.
        for (int i = 0; i < reqDiskBlocks; i++)
        {
            memset(blockData, ' ', BLOCK_SIZE);
            int fileDataTransfered = std::min(BLOCK_SIZE, static_cast<int>(fileData.size() - totalFileDataTransfered));
            std::copy(fileData.begin() + totalFileDataTransfered, fileData.begin() + totalFileDataTransfered + fileDataTransfered, blockData);
            totalFileDataTransfered += fileDataTransfered;

            disk.write(freeDiskBlockIndex[i], (uint8_t *)blockData);
        }

        // 5.
        std::string dirNew;
        for (int i = 0; fileEntry.file_name[i] != '\0'; i++)
        {
            dirNew.append(1, fileEntry.file_name[i]);
        }
        dirNew += '\n' + std::to_string(fileEntry.size) + '\n' + std::to_string(fileEntry.first_blk) + '\n' +
                  std::to_string(fileEntry.type) + '\n' + std::to_string(fileEntry.access_rights) + '\n';
        char dirOriginal[BLOCK_SIZE];
        disk.read(directoryList->at(directoryIndex).block, (uint8_t *)&dirOriginal);
        dirNew += dirOriginal;
        clearDiskBlock(directoryList->at(directoryIndex).block);
        disk.write(directoryList->at(directoryIndex).block, (uint8_t *)dirNew.c_str());

        // 6.
        for (int i = 0; i < reqDiskBlocks; i++)
        {
            if (i == (reqDiskBlocks - 1))
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
        return -2;
    }

    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int FS::cat(std::string filepath)
{
    if (filepath[0] != '/') // Add a slash if the first letter is missing it for similar parsing.
    {
        filepath = "/" + filepath;
        if (directoryList->at(currentDirectoryIndex).block != ROOT_BLOCK)
        {
            filepath = directoryList->at(currentDirectoryIndex).name + filepath;
        }
    }

    std::string fullFilePath;
    std::string parentDirectory;
    std::string fileName;

    directoryParser(filepath, parentDirectory, fullFilePath, fileName);

    // Find the directory index
    bool found = false;
    int directoryIndex;
    for (int i = 0; i < directoryList->size(); i++)
    {
        if (parentDirectory == directoryList->at(i).name)
        {
            directoryIndex = i;
            found = true;
            break;
        }
    }

    if (!found)
    {
        std::cout << "Error: Directory doesn't exist" << std::endl;
        return -1;
    }

    // Check if a file with the name exists
    if (fileExists(directoryList->at(directoryIndex).block, fileName))
    {
        std::string fileContent = readFile(fileName, directoryIndex);
        if (fileContent != "")
        {
            std::cout << fileContent;
        }
        else
        {
            std::cout << "Error: File not readable." << std::endl;
            return -3;
        }
    }
    else
    {
        std::cout << "Error: File doesn't exist." << std::endl;
        return -2;
    }

    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int FS::ls()
{
    std::string content = "Name\tSize\tF_Block\tType\tRWX\n";

    char directoryText[BLOCK_SIZE];
    disk.read(directoryList->at(currentDirectoryIndex).block, (uint8_t *)&directoryText);
    char issText[BLOCK_SIZE + 1];
    memcpy(issText, directoryText, BLOCK_SIZE);
    issText[BLOCK_SIZE] = '\0'; // Stringstream uses null termination, otherwise unwanted characters can get added.

    std::istringstream iss(issText);
    std::string word;
    int index = 0;
    while (iss >> std::ws && std::getline(iss, word))
    {
        switch (index)
        {
        case 0:
            content += word + "\t"; // Name
            break;
        case 1:
            if (std::stoi(word) == 0)
            {
                content += "-\t"; // Size Dir
            }
            else
            {
                content += word + "\t"; // Size File
            }
            break;
        case 2:
            content += word + "\t"; // InitBlock
            break;
        case 3:
            if (word == "0")
            {
                content += "F\t"; // Type: File
            }
            else
            {
                content += "D\t"; // Type: Directory
            }
            break;
        case 4:
            int Rights = std::stoi(word);
            switch (Rights)
            {
            case 1:
                content += "--X\n"; // Execute
                break;
            case 2:
                content += "-W-\n"; // Write
                break;
            case 3:
                content += "-WX\n"; // Write-Execute
                break;
            case 4:
                content += "R--\n"; // Read
                break;
            case 5:
                content += "R-X\n"; // Read-Execute
                break;
            case 6:
                content += "RW-\n"; // Read-Write
                break;
            case 7:
                content += "RWX\n"; // Read-Write-Execute
                break;
            }
            break;
        }
        index++;
        if (index > 4)
        {
            index = 0;
        }
    }

    std::cout << content;
    return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int FS::cp(std::string sourcepath, std::string destpath)
{
    if (sourcepath[0] != '/') // Add a slash if the first letter is missing it for similar parsing.
    {
        sourcepath = "/" + sourcepath;
        if (directoryList->at(currentDirectoryIndex).block != ROOT_BLOCK)
        {
            sourcepath = directoryList->at(currentDirectoryIndex).name + sourcepath;
        }
    }

    if (destpath[0] != '/') // Add a slash if the first letter is missing it for similar parsing.
    {
        destpath = "/" + destpath;
        if (directoryList->at(currentDirectoryIndex).block != ROOT_BLOCK)
        {
            destpath = directoryList->at(currentDirectoryIndex).name + destpath;
        }
    }

    std::string sourceFullFilePath;
    std::string sourceParentDirectory;
    std::string sourceFileName;
    std::string destFullFilePath;
    std::string destParentDirectory;
    std::string destFileName;

    directoryParser(sourcepath, sourceParentDirectory, sourceFullFilePath, sourceFileName);
    directoryParser(destpath, destParentDirectory, destFullFilePath, destFileName);

    // Find the directory index.
    bool sourceDirFound = false;
    int sourceDirectoryIndex;
    for (int i = 0; i < directoryList->size(); i++)
    {
        if (sourceParentDirectory == directoryList->at(i).name)
        {
            sourceDirectoryIndex = i;
            sourceDirFound = true;
        }
    }

    if (!sourceDirFound)
    {
        std::cout << "Error: Directory doesn't exist" << std::endl;
        return -1;
    }

    /*
        1. Check if source file exists
        2. Check if the destpath is a directory or not
        3. If it is then create a copy of the file to that directory using the sourceFileName
        4. If it isn't then we check if a file or directory with the same name exists, otherwise we create a copy in the same directory with the destFileName.
    */

    // 1.
    if (!fileExists(directoryList->at(sourceDirectoryIndex).block, sourceFileName))
    {
        std::cout << "Error: Attempt to copy non-existing file" << std::endl;
        return -2;
    }

    // 2.
    bool directoryBool = false;
    int destDirectoryIndex;
    for (int i = 0; i < directoryList->size(); i++)
    {
        if (destFullFilePath == directoryList->at(i).name) // If the fullFilePath is a directory
        {
            directoryBool = true;
            destDirectoryIndex = i;
        }
    }

    // 3.
    if (directoryBool) // Guaranteed to copy file into another directory using the original file name
    {
        int ref;
        if (!fileExists(directoryList->at(destDirectoryIndex).block, sourceFileName) && !directoryExists(directoryList->at(sourceDirectoryIndex).block, sourceFileName, ref))
        {
            std::string fileData = readFile(sourceFileName, sourceDirectoryIndex);
            if (fileData == "")
            {
                std::cout << "Error: Read access denied" << std::endl;
                return -4;
            }
            return copyFile(fileData, sourceFileName, directoryList->at(destDirectoryIndex).block);
        }
        else
        {
            std::cout << "Error: Destination name already taken" << std::endl;
            return -3;
        }
    }
    else // Copy file into same directory but with different name.
    {
        // 4.
        int ref;
        if (!fileExists(directoryList->at(sourceDirectoryIndex).block, destFileName) && !directoryExists(directoryList->at(sourceDirectoryIndex).block, destFileName, ref))
        {
            std::string fileData = readFile(sourceFileName, sourceDirectoryIndex);
            if (fileData == "")
            {
                std::cout << "Error: Read access denied" << std::endl;
                return -4;
            }
            // 5.
            return copyFile(fileData, destFileName, directoryList->at(currentDirectoryIndex).block);
        }
        else
        {
            std::cout << "Error: Destination name already taken" << std::endl;
            return -3;
        }
    }
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int FS::mv(std::string sourcepath, std::string destpath)
{
    if (sourcepath[0] != '/') // Add a slash if the first letter is missing it for similar parsing.
    {
        sourcepath = "/" + sourcepath;
        if (directoryList->at(currentDirectoryIndex).block != ROOT_BLOCK)
        {
            sourcepath = directoryList->at(currentDirectoryIndex).name + sourcepath;
        }
    }

    if (destpath[0] != '/') // Add a slash if the first letter is missing it for similar parsing.
    {
        destpath = "/" + destpath;
        if (directoryList->at(currentDirectoryIndex).block != ROOT_BLOCK)
        {
            destpath = directoryList->at(currentDirectoryIndex).name + destpath;
        }
    }

    std::string sourceFullFilePath;
    std::string sourceParentDirectory;
    std::string sourceFileName;
    std::string destFullFilePath;
    std::string destParentDirectory;
    std::string destFileName;

    directoryParser(sourcepath, sourceParentDirectory, sourceFullFilePath, sourceFileName);
    directoryParser(destpath, destParentDirectory, destFullFilePath, destFileName);

    // Find the directory index.
    bool sourceDirFound = false;
    int sourceDirectoryIndex;
    for (int i = 0; i < directoryList->size(); i++)
    {
        if (sourceParentDirectory == directoryList->at(i).name)
        {
            sourceDirectoryIndex = i;
            sourceDirFound = true;
        }
    }

    if (!sourceDirFound)
    {
        std::cout << "Error: Directory doesn't exist" << std::endl;
        return -1;
    }

    // Check if sourcefile exists.
    if (!fileExists(directoryList->at(sourceDirectoryIndex).block, sourceFileName))
    {
        std::cout << "Error: Attempt to move/rename non-existing file" << std::endl;
        return -2;
    }

    // Check if destination is a directory
    bool directoryBool = false;
    int destDirectoryIndex;
    for (int i = 0; i < directoryList->size(); i++)
    {
        if (destFullFilePath == directoryList->at(i).name) // If the fullFilePath is a directory
        {
            directoryBool = true;
            destDirectoryIndex = i;
        }
    }

    if (directoryBool) // Move file to another directory
    {
        int ref;
        // Check if destination name is taken
        if (!fileExists(directoryList->at(destDirectoryIndex).block, sourceFileName) && !directoryExists(directoryList->at(sourceDirectoryIndex).block, sourceFileName, ref))
        {
            return moveFile(sourceFileName, "", directoryList->at(sourceDirectoryIndex).block, directoryList->at(destDirectoryIndex).block, false);
        }
        else
        {
            std::cout << "Error: Destination name already taken" << std::endl;
            return -3;
        }
    }
    else // Rename the file within the same directory
    {
        int ref;
        if (!fileExists(directoryList->at(sourceDirectoryIndex).block, destFileName) && !directoryExists(directoryList->at(sourceDirectoryIndex).block, destFileName, ref))
        {
            return moveFile(sourceFileName, destFileName, directoryList->at(sourceDirectoryIndex).block, directoryList->at(sourceDirectoryIndex).block, true);
        }
        else
        {
            std::cout << "Error: Destination name already taken" << std::endl;
            return -2;
        }
    }

    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int FS::rm(std::string filepath)
{
    if (filepath[0] != '/') // Add a slash if the first letter is missing it for similar parsing.
    {
        filepath = "/" + filepath;
        if (directoryList->at(currentDirectoryIndex).block != ROOT_BLOCK)
        {
            filepath = directoryList->at(currentDirectoryIndex).name + filepath;
        }
    }

    std::string fullFilePath;
    std::string parentDirectory;
    std::string fileName;

    directoryParser(filepath, parentDirectory, fullFilePath, fileName);

    // Find the directory index
    bool found = false;
    int directoryIndex;
    for (int i = 0; i < directoryList->size(); i++)
    {
        if (parentDirectory == directoryList->at(i).name)
        {
            directoryIndex = i;
            found = true;
            break;
        }
    }

    if (!found)
    {
        std::cout << "Error: Directory doesn't exist" << std::endl;
        return -1;
    }

    if (fullFilePath == "/")
    {
        std::cout << "Error: Attempt to delete root directory" << std::endl;
        return -2;
    }

    // Check if it's a directory that we want to remove
    bool directoryBool = false;
    for (int i = 0; i < directoryList->size(); i++)
    {
        if (fullFilePath == directoryList->at(i).name) // If the fullFilePath is a directory
        {
            directoryBool = true;
            directoryIndex = i;
        }
    }

    if (directoryBool)
    {
        // Check if directory is empty
        char directoryText[BLOCK_SIZE];
        disk.read(directoryList->at(directoryIndex).block, (uint8_t *)&directoryText);
        char issText[BLOCK_SIZE + 1];
        memcpy(issText, directoryText, BLOCK_SIZE);
        issText[BLOCK_SIZE] = '\0';
        std::istringstream iss(issText);
        std::string word;
        while (iss >> word)
        {
            std::cout << "Error: Directory isn't empty" << std::endl;
            return -3;
        }

        // Reconstruct the parent directory to not have the directory
        iss.clear();
        memset(issText, '\0', BLOCK_SIZE + 1);
        memset(directoryText, '\0', BLOCK_SIZE);
        int parentIndex = 0;
        for (int i = 0; i < directoryList->size(); i++)
        {
            if (directoryList->at(directoryIndex).parent == directoryList->at(i).name)
            {
                parentIndex = i;
                break;
            }
        }
        disk.read(directoryList->at(parentIndex).block, (uint8_t *)&directoryText);
        memcpy(issText, directoryText, BLOCK_SIZE);
        issText[BLOCK_SIZE] = '\0';

        iss.str(issText);
        word = "";
        std::string dirNew;
        int index = 0;

        while (iss >> word)
        {
            if (word == fileName)
            {
                index++;
            }
            else if (index == 0)
            {
                dirNew += word + "\n";
            }
            else
            {
                index++;
                if (index == 5)
                {
                    index = 0;
                }
            }
        }

        clearDiskBlock(directoryList->at(parentIndex).block);
        disk.write(directoryList->at(parentIndex).block, (uint8_t *)dirNew.c_str());

        // Update FAT
        fat[directoryList->at(directoryIndex).block] = FAT_FREE;
        updateFat();

        // Remove the directory from directory list and if it's the currentDirectory, switch to root directory
        if (directoryList->at(currentDirectoryIndex).name == fullFilePath)
        {
            for (int i = 0; i < directoryList->size(); i++)
            {
                if (directoryList->at(i).name == "/")
                {
                    currentDirectoryIndex = i;
                }
            }
        }
        directoryList->erase(directoryList->begin() + directoryIndex);
    }
    else // Remove file
    {
        // Check if file exists.
        if (!fileExists(directoryList->at(directoryIndex).block, fileName))
        {
            std::cout << "Error: Attempt to remove non-existing file" << std::endl;
            return -4;
        }

        // Reconstruct the directory to not include the file but keep track of first block so we can free up all the FAT slots used.
        char directoryText[BLOCK_SIZE];
        disk.read(directoryList->at(directoryIndex).block, (uint8_t *)&directoryText);
        char issText[BLOCK_SIZE + 1];
        memcpy(issText, directoryText, BLOCK_SIZE);
        issText[BLOCK_SIZE] = '\0';

        std::istringstream iss(issText);
        std::string word;
        std::string dirNew;
        int index = 0;
        int firstBlk = 0;

        while (iss >> word)
        {
            if (word == fileName)
            {
                index++;
            }
            else if (index == 0)
            {
                dirNew += word + "\n";
            }
            else
            {
                index++;
                if (index == 3) // First block
                {
                    firstBlk = std::stoi(word);
                }
                else if (index == 5)
                {
                    index = 0;
                }
            }
        }

        clearDiskBlock(directoryList->at(directoryIndex).block);
        disk.write(directoryList->at(directoryIndex).block, (uint8_t *)dirNew.c_str());

        // Update the fat slots used.
        bool cleared = false;
        while (cleared == false)
        {
            if (fat[firstBlk] == FAT_EOF) // Last slot for the file
            {
                fat[firstBlk] = FAT_FREE;
                cleared = true;
            }
            else
            {
                int nextBlk = fat[firstBlk];
                fat[firstBlk] = FAT_FREE;
                firstBlk = nextBlk;
            }
        }

        updateFat();
    }

    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int FS::append(std::string filepath1, std::string filepath2)
{
    if (filepath1[0] != '/') // Add a slash if the first letter is missing it for similar parsing.
    {
        filepath1 = "/" + filepath1;
        if (directoryList->at(currentDirectoryIndex).block != ROOT_BLOCK)
        {
            filepath1 = directoryList->at(currentDirectoryIndex).name + filepath1;
        }
    }

    if (filepath2[0] != '/') // Add a slash if the first letter is missing it for similar parsing.
    {
        filepath2 = "/" + filepath2;
        if (directoryList->at(currentDirectoryIndex).block != ROOT_BLOCK)
        {
            filepath2 = directoryList->at(currentDirectoryIndex).name + filepath2;
        }
    }

    std::string sourceFullFilePath;
    std::string sourceParentDirectory;
    std::string sourceFileName;
    std::string destFullFilePath;
    std::string destParentDirectory;
    std::string destFileName;

    directoryParser(filepath1, sourceParentDirectory, sourceFullFilePath, sourceFileName);
    directoryParser(filepath2, destParentDirectory, destFullFilePath, destFileName);

    // Find the directory index.
    bool sourceDirFound = false;
    int sourceDirectoryIndex;
    bool destDirFound = false;
    int destDirectoryIndex;
    for (int i = 0; i < directoryList->size(); i++)
    {
        if (sourceParentDirectory == directoryList->at(i).name)
        {
            sourceDirectoryIndex = i;
            sourceDirFound = true;
        }
        if (destParentDirectory == directoryList->at(i).name)
        {
            destDirectoryIndex = i;
            destDirFound = true;
        }
    }

    if (!sourceDirFound || !destDirFound)
    {
        std::cout << "Error: Directory doesn't exist" << std::endl;
        return -1;
    }

    // Check if both files exist
    if (fileExists(directoryList->at(sourceDirectoryIndex).block, sourceFileName) && fileExists(directoryList->at(destDirectoryIndex).block, destFileName))
    {
        // Check if we can read the contents of file1
        std::string file1Content = readFile(sourceFileName, sourceDirectoryIndex);
        int newSize = file1Content.size();
        if (file1Content == "")
        {
            std::cout << "Error: File1 doesn't have read rights or is empty." << std::endl;
            return -3;
        }

        // Check if we have write access on file2
        int block;
        if (getAccessRightsAndFirstBlk(destFileName, destDirectoryIndex, block).find("W") == std::string::npos)
        {
            std::cout << "Error: File2 doesn't have write access." << std::endl;
            return -4;
        }

        // Find the last block used by file2.
        while (fat[block] != FAT_EOF)
        {
            block = fat[block];
        }

        // Read the data on the EOF block.
        char eofBlockData[BLOCK_SIZE];
        disk.read(block, (uint8_t *)&eofBlockData);
        char issText[BLOCK_SIZE + 1];
        memcpy(issText, eofBlockData, BLOCK_SIZE);
        issText[BLOCK_SIZE] = '\0';

        std::istringstream iss(issText);
        std::string word;
        std::string contents = "";
        while (iss >> word)
        {
            contents += word + " ";
        }
        contents[contents.size() - 1] = '\n';

        // Get the size of the contents so we know how much to add to it until it's full
        int dataUntilFull = BLOCK_SIZE - contents.size();
        contents += file1Content.substr(0, dataUntilFull); // Adds the letters required to fill up the block to the contents string
        file1Content.erase(0, dataUntilFull);              // Erase the letters used up from file1 content.

        // Write the new data to the block.
        clearDiskBlock(block);
        disk.write(block, (uint8_t *)contents.c_str());

        if (file1Content.size() > 0) // If file1 still has content to append find new blocks.
        {
            int reqDiskBlocks = (file1Content.size() / BLOCK_SIZE) + 1;

            int freeDiskBlockIndex[reqDiskBlocks];
            for (int i = 0; i < reqDiskBlocks; i++)
            {
                freeDiskBlockIndex[i] = 0;
            }
            int arrayIndex = 0;
            for (int i = 2; i < (BLOCK_SIZE / 2); i++) // First two blocks are never available.
            {
                if (fat[i] == FAT_FREE)
                {
                    freeDiskBlockIndex[arrayIndex] = i;
                    arrayIndex++;

                    if (freeDiskBlockIndex[(reqDiskBlocks - 1)] != 0)
                    {
                        break;
                    }
                }
            }

            if (freeDiskBlockIndex[(reqDiskBlocks - 1)] == 0) // Amount of free disk blocks required couldn't be found.
            {
                std::cout << "Error: Not enough free disk blocks" << std::endl;
                return -5;
            }

            char blockData[BLOCK_SIZE];      // The block data to transfer.
            int totalFileDataTransfered = 0; // Keeps track of how much file data has been transfered.
            for (int i = 0; i < reqDiskBlocks; i++)
            {
                memset(blockData, ' ', BLOCK_SIZE);
                int fileDataTransfered = std::min(BLOCK_SIZE, static_cast<int>(file1Content.size() - totalFileDataTransfered));
                std::copy(file1Content.begin() + totalFileDataTransfered, file1Content.begin() + totalFileDataTransfered + fileDataTransfered, blockData);
                totalFileDataTransfered += fileDataTransfered;

                disk.write(freeDiskBlockIndex[i], (uint8_t *)blockData);
            }

            // Update the FAT since more blocks were added
            fat[block] = freeDiskBlockIndex[0];
            for (int i = 0; i < reqDiskBlocks; i++)
            {
                if (i == (reqDiskBlocks - 1))
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

        // Update the directory entry to match the new size of the file.
        iss.clear();
        char directoryText[BLOCK_SIZE];
        disk.read(directoryList->at(destDirectoryIndex).block, (uint8_t *)&directoryText);
        memset(issText, ' ', BLOCK_SIZE + 1);
        memcpy(issText, directoryText, BLOCK_SIZE);
        issText[BLOCK_SIZE] = '\0';

        iss.str(issText);
        word = "";
        std::string dirNew;
        int index = 0;

        while (iss >> word)
        {
            if (word == destFileName) // Finds the name
            {
                dirNew += word + "\n";
                index++;
            }
            else if (index == 0) // Everything else
            {
                dirNew += word + "\n";
            }
            else // Size comes after name
            {
                newSize += std::stoi(word); // File1Content size + Current file size
                dirNew += std::to_string(newSize) + "\n";
                index = 0;
            }
        }

        clearDiskBlock(directoryList->at(destDirectoryIndex).block);
        disk.write(directoryList->at(destDirectoryIndex).block, (uint8_t *)dirNew.c_str());
    }
    else
    {
        std::cout << "Error: One or both of the files do not exist or they could be a directory" << std::endl;
        return -2;
    }
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int FS::mkdir(std::string dirpath)
{
    // Parse the name because it can be like "d1", "/d1", "d1/d2/../d3", "/d1/d2/../d3" and it should still be able to keep track of the correct name.
    std::string parentName;
    std::string dirFullName;
    std::string dirName;

    if (dirpath[0] != '/') // Add a slash if the first letter is missing it for similar parsing.
    {
        dirpath = "/" + dirpath;
    }

    if (directoryList->at(currentDirectoryIndex).block != ROOT_BLOCK)
    {
        dirpath = directoryList->at(currentDirectoryIndex).name + dirpath;
    }

    directoryParser(dirpath, parentName, dirFullName, dirName);

    bool parentExists = false;
    int parentBlock = ROOT_BLOCK;
    for (int i = 0; i < directoryList->size(); i++)
    {
        if (parentName == directoryList->at(i).name)
        {
            parentBlock = directoryList->at(i).block;
            parentExists = true;
            break;
        }
    }

    if (parentExists)
    {
        int throwaway;
        if (directoryExists(parentBlock, dirName, throwaway) || fileExists(parentBlock, dirName))
        {
            std::cout << "Error: Directory or File with same name already exists" << std::endl;
            return -2;
        }

        // Check if the dirName is too big for the buffer.
        if (dirName.size() > 55)
        {
            std::cout << "Error: Dirname too big" << std::endl;
            return -3;
        }

        // Check if the directory is full or not (64 entries max per directory).
        char directoryText[BLOCK_SIZE];
        disk.read(parentBlock, (uint8_t *)&directoryText);
        char issText[BLOCK_SIZE + 1];
        memcpy(issText, directoryText, BLOCK_SIZE);
        issText[BLOCK_SIZE] = '\0';
        std::istringstream iss(issText);
        std::string word;
        int entries = 0;
        while (iss >> word)
        {
            entries++;
        }
        if (entries / 5 > 63)
        {
            std::cout << "Error: Directory full" << std::endl;
            return -4;
        }

        // Assign a free disk block to the directory.
        int diskBlockIndex = 0;
        for (int i = 2; i < BLOCK_SIZE / 2; i++)
        {
            if (fat[i] == FAT_FREE)
            {
                diskBlockIndex = i;
                break;
            }
        }

        if (diskBlockIndex == 0)
        {
            std::cout << "Error: No free disk blocks found" << std::endl;
            return -5;
        }

        // Create a dir_entry of the type directory
        dir_entry directoryEntry;
        strncpy(directoryEntry.file_name, dirName.c_str(), sizeof(directoryEntry.file_name) - 1);
        directoryEntry.size = 0;
        directoryEntry.first_blk = diskBlockIndex;
        directoryEntry.type = 1;
        directoryEntry.access_rights = READ + WRITE;

        // Add the dir_entry to the parent directory
        std::string dirNew;
        for (int i = 0; directoryEntry.file_name[i] != '\0'; i++)
        {
            dirNew.append(1, directoryEntry.file_name[i]);
        }
        dirNew += '\n' + std::to_string(directoryEntry.size) + '\n' + std::to_string(directoryEntry.first_blk) + '\n' +
                  std::to_string(directoryEntry.type) + '\n' + std::to_string(directoryEntry.access_rights) + '\n';
        char dirOriginal[BLOCK_SIZE];
        disk.read(parentBlock, (uint8_t *)&dirOriginal);
        dirNew += dirOriginal;
        clearDiskBlock(parentBlock);
        disk.write(parentBlock, (uint8_t *)dirNew.c_str());

        // Update the fat
        fat[diskBlockIndex] = FAT_EOF;
        updateFat();

        // Add the new directory to the list of directories that exist.
        directoryList->push_back(directory(diskBlockIndex, dirFullName, parentName));
    }
    else
    {
        std::cout << "Error: Cannot create a directory in a non-existant parent directory" << std::endl;
        return -1;
    }

    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int FS::cd(std::string dirpath)
{
    if (dirpath[0] != '/') // Add a slash if the first letter is missing it for similar parsing.
    {
        dirpath = "/" + dirpath;
        if (directoryList->at(currentDirectoryIndex).block != ROOT_BLOCK)
        {
            dirpath = directoryList->at(currentDirectoryIndex).name + dirpath;
        }
    }

    std::string dirFullName;
    std::string parent;
    std::string dir;

    directoryParser(dirpath, parent, dirFullName, dir);

    // Find the directory we want to switch to and change the currentDirectory to match that.
    bool found = false;
    for (int i = 0; i < directoryList->size(); i++)
    {
        if (dirFullName == directoryList->at(i).name)
        {
            if (directoryList->at(i).name == directoryList->at(currentDirectoryIndex).name)
            {
                std::cout << "Error: You're already located in this directory" << std::endl;
                return -1;
            }
            currentDirectoryIndex = i;
            found = true;
            break;
        }
    }

    if (!found)
    {
        std::cout << "Error: Directory doesn't exist" << std::endl;
        return -2;
    }

    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int FS::pwd()
{
    std::cout << directoryList->at(currentDirectoryIndex).name << std::endl;

    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int FS::chmod(std::string accessrights, std::string filepath)
{
    if (filepath[0] != '/') // Add a slash if the first letter is missing it for similar parsing.
    {
        filepath = "/" + filepath;
        if (directoryList->at(currentDirectoryIndex).block != ROOT_BLOCK)
        {
            filepath = directoryList->at(currentDirectoryIndex).name + filepath;
        }
    }

    std::string fullFilePath;
    std::string parentDirectory;
    std::string fileName;

    directoryParser(filepath, parentDirectory, fullFilePath, fileName);

    // Find the directory we want to create the file in.
    bool found = false;
    int directoryIndex;
    for (int i = 0; i < directoryList->size(); i++)
    {
        if (parentDirectory == directoryList->at(i).name)
        {
            directoryIndex = i;
            found = true;
            break;
        }
    }

    if (!found || fileName.find("..") != std::string::npos)
    {
        std::cout << "Error: Directory doesn't exist" << std::endl;
        return -1;
    }

    // Check if the file exists in the currentDirectory
    if (fileExists(directoryList->at(directoryIndex).block, fileName))
    {
        // Get the integer value for the accessrights.
        int accessint;
        try
        {
            accessint = std::stoi(accessrights);
        }
        catch (const std::exception &e)
        {
            std::cout << "Error: Accessrights has the wrong format" << std::endl;
            return -3;
        }

        if (accessint < 1 || accessint > 7)
        {
            std::cout << "Error: Invalid access rights" << std::endl;
            return -4;
        }

        // Reconstruct the currentDirectory with the new access rights for the file.
        char directoryText[BLOCK_SIZE];
        disk.read(directoryList->at(directoryIndex).block, (uint8_t *)&directoryText);
        char issText[BLOCK_SIZE + 1];
        memcpy(issText, directoryText, BLOCK_SIZE);
        issText[BLOCK_SIZE] = '\0';

        std::istringstream iss(issText);
        std::string word;
        std::string dirNew;
        int index = 0;

        while (iss >> word)
        {
            if (word == fileName)
            {
                dirNew += word + "\n";
                index++;
            }
            else if (index == 0)
            {
                dirNew += word + "\n";
            }
            else
            {
                index++;
                if (index == 5) // Access rights
                {
                    dirNew += std::to_string(accessint) + "\n";
                    index = 0;
                }
                else
                {
                    dirNew += word + "\n";
                }
            }
        }

        clearDiskBlock(directoryList->at(directoryIndex).block);
        disk.write(directoryList->at(directoryIndex).block, (uint8_t *)dirNew.c_str());
    }
    else
    {
        std::cout << "Error: Attempt to chmod on non-existing file" << std::endl;
        return -2;
    }

    return 0;
}
