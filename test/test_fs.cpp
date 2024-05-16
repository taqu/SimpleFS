#include "catch_wrap.hpp"
#include "../SimpleFS.h"
#include <iostream>

TEST_CASE("FileSystem::Constuct", "[SimpleFS]")
{
    using namespace sfs;
    FileSystem fs;
    fs.mountOS(".");
    IDirectoryHandle* root = fs.openDirectory(nullptr);
    FileIterator itr = root->begin();
    for(; itr.next();){
        std::cout << itr.path() << std::endl;
    }
    itr.close();
    fs.closeDirectory(root);
}