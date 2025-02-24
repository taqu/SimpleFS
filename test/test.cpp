#include "catch_amalgamated.hpp"
#include "../simplefs.h"
#include <iostream>

#define EQ_FLOAT(x0, x1) CHECK(std::abs(x0-x1)<1.0e-7f)

TEST_CASE("PhySF" "[physical]")
{
    sfs::PhyFS phyfs;
    if(!phyfs.open("data")){
        return;
    }
    sfs::IFile* file = phyfs.open_file("/");
    for(auto&& itr = file->begin(); itr; ++itr){
        printf("%s %d\n", &(*itr).filename()[0], (*itr).original_size());
        if(!itr->is_file()){
            for(auto&& itr2 = itr->begin(); itr2; ++itr2) {
                printf("  %s %d\n", &(*itr2).filename()[0], (*itr2).original_size());
            }
        }
    }
    file->close();
    phyfs.close();
}

TEST_CASE("Builder" "[build]")
{
    sfs::Builder builder;
    sfs::Builder::Param param;
    builder.build("data", "out.pac", param);
}

std::ostream& print(std::ostream& os, std::u8string_view str)
{
    for(size_t i=0; i<str.length(); ++i){
        os << (char)str[i];
    }
    return os;
}

TEST_CASE("PacFS" "[pack]")
{
    sfs::PacFS pacfs;
    if(!pacfs.open("out.pac")){
        return;
    }
    sfs::IFile* file = pacfs.open_file("/");
    for(auto&& itr = file->begin(); itr; ++itr){
        print(std::cout, itr->filename());
        std::cout << "orig: " << itr->original_size() << " comp: " << itr->compressed_size() << std::endl;
        if(!itr->is_file()){
            for(auto&& itr2 = itr->begin(); itr2; ++itr2) {
                std::cout << "  ";
                print(std::cout, itr2->filename());
                std::cout << "orig: " << itr2->original_size() << " comp: " << itr2->compressed_size() << std::endl;
                if(itr2->is_file()){
                    char* buffer = (char*)::malloc(itr2->original_size()+1);
                    uint32_t r = itr2->read(buffer);
                    buffer[itr2->original_size()] = '\0';
                    printf("%s\n", buffer);
                    ::free(buffer);
                }
            }
        }else{
            char* buffer = (char*)::malloc(itr->original_size()+1);
            uint32_t r = itr->read(buffer);
            buffer[itr->original_size()] = '\0';
            printf("%s\n", buffer);
            ::free(buffer);
        }
    }
    file->close();
    pacfs.close();
}

