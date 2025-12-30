//
//  hostfs.cpp
//  xcode-yarg
//
//  Created by dlm on 12/12/2025.
//

extern "C" {
#include "files.h"
#include "string.h"
}

#include <string>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <fstream>

#define STRINGIFY(s) #s
#define STRING(s) STRINGIFY(s)

using namespace std;

// access env var PROJECT_DIR /Users/lorin/Documents/Personal/Dev/yarg/yarg-lang/xcode-yarg
// remove xcode-yarg to get the git root

char *readFile(const char* path)
{
    filesystem::path filePath;
    if (strnlen(path, 1) > 0 && path[0] != '/')
    {
#ifdef PROJECT_DIR
        filePath = STRING(PROJECT_DIR);
        filePath = filePath.parent_path();
#else
        filePath = filesystem::current_path();
#endif
        
        filePath /= path;
    }
    else
    {
        filePath = path;
    }
    
    char *fileContents{0};
    uintmax_t fileSize{0};
    if (filesystem::is_regular_file(filePath))
    {
        error_code ec;
        fileSize = filesystem::file_size(filePath, ec);
        if (!ec && fileSize > 0)
        {
            fileContents = static_cast<char *>(malloc(fileSize));
        }
    }
    
    if (fileContents != 0)
    {
        ifstream fileStream{filePath, std::ios::binary};
        if (fileStream.is_open())
        {
            fileStream.read(fileContents, fileSize); // binary input
            streamsize numRead{fileStream.gcount()};
            fileContents[numRead] = '\0';
        }
        else
        {
            free(fileContents);
            fileContents = 0;
        }
    }

    return fileContents;
}
