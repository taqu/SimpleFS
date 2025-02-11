#include "simplefs.h"
#include <cassert>
#include <cstring>
#ifdef _DEBUG
#include <cstdio>
#endif
#include <mimalloc.h>
#include <xxhash.h>

#define SFS_MALLOC(size) ::mi_malloc(size)
#define SFS_FREE(ptr) ::mi_free(ptr)

namespace sfs
{
//--- Array
//--------------------------------------------------------
template<class T>
Array<T>::Array()
    : capacity_(0)
    , size_(0)
    , items_(nullptr)
{
}

template<class T>
Array<T>::~Array()
{
    for(u32 i=0; i<size_; ++i){
        items_[i].~T();
    }
    capacity_ = 0;
    size_ = 0;
    SFS_FREE(items_);
    items_ = nullptr;
}

template<class T>
u32 Array<T>::capacity() const
{
    return capacity_;
}

template<class T>
u32 Array<T>::size() const
{
    return size_;
}

template<class T>
void Array<T>::clear()
{
    size_ = 0;
}

template<class T>
const T& Array<T>::operator[](u32 index) const
{
    return items_[index];
}

template<class T>
T& Array<T>::operator[](u32 index)
{
    return items_[index];
}

template<class T>
void Array<T>::push_back(const T& x)
{
    if(capacity_ <= size_) {
        expand();
    }
    new(&items_[size_]) T(x);
    ++size_;
}

template<class T>
void Array<T>::pop_back()
{
    assert(0<size_);
    --size_;
    items_[size_].~T();
}

template<class T>
void Array<T>::expand()
{
    u32 capacity = capacity_ + 16;
    T* items = (T*)SFS_MALLOC(sizeof(T) * capacity);
    for(u32 i=0; i<size_; ++i){
        new(&items[i]) T(items_[i]);
        items_[i].~T();
    }
    SFS_FREE(items_);
    capacity_ = capacity;
    items_ = items;
}

//--- Builder
//--------------------------------------------------------
namespace 
    {
    bool is_hidden(const std::filesystem::directory_entry& entry)
        {
        return entry.path().filename() == "."
            || entry.path().filename() == ".."
            || entry.path().filename().u8string()[0] == '.';
        }
    }

Builder::Builder()
{
}

Builder::~Builder()
{
}

bool Builder::build(const char* root, const char* outfile)
{
    assert(nullptr != root);
    assert(nullptr != outfile);
    using namespace std::filesystem;
    files_.clear();
    names_.clear();
    filepath_.clear();
    for(const directory_entry& x: directory_iterator(root)) {
        if(x.is_regular_file()) {
            build_file(x);
        } else if(x.is_directory() && !is_hidden(x)) {
            build_directory(x);
        }
    }
    u32 name_top = static_cast<u32>(sizeof(Header) + files_.size() * sizeof(File));
    u64 data_top = names_.size() + name_top;
    for(u32 i = 0; i < files_.size(); ++i) {
        files_[i].name_offset_ += name_top;
        files_[i].offset_ += data_top;
    }
    return compress(outfile);
}

void Builder::build_file(const std::filesystem::directory_entry& entry)
{
    std::u8string name = entry.path().filename().u8string();
    File file;
    file.size_ = entry.file_size();
    file.offset_ = 0;
    file.num_children_ = 0;
    file.child_start_ = 0;
    file.name_offset_ = static_cast<u32>(names_.size());
    file.name_length_ = static_cast<u16>(name.length());
    file.type_ = static_cast<u8>(Type::File);
    file.compression_ = 0;
    for(size_t i = 0; i < name.length(); ++i) {
        names_.push_back(name[i]);
    }
    files_.push_back(file);
    std::filesystem::path absolutepath = std::filesystem::absolute(entry.path());
    filepath_.push_back(absolutepath);
#ifdef _DEBUG
    printf("file %s: size:%lld\n", (const char*)absolutepath.u8string().c_str(), entry.file_size());
#endif
}

void Builder::build_directory(const std::filesystem::directory_entry& entry)
{
    using namespace std::filesystem;
    u32 num_children = 0;
    for(const directory_entry& x: directory_iterator(entry)) {
        if(x.is_regular_file()) {
            ++num_children;
        }
    }

    std::u8string name = entry.path().filename().u8string();
    File file;
    file.size_ = entry.file_size();
    file.offset_ = 0;
    file.num_children_ = num_children;
    file.child_start_ = static_cast<u32>(files_.size());
    file.name_offset_ = static_cast<u32>(names_.size());
    file.name_length_ = static_cast<u16>(name.length());
    file.type_ = static_cast<u8>(Type::Directory);
    file.compression_ = 0;
    for(size_t i = 0; i < name.length(); ++i) {
        names_.push_back(name[i]);
    }
    files_.push_back(file);
    std::filesystem::path absolutepath = std::filesystem::absolute(entry.path());
    filepath_.push_back(absolutepath);
#ifdef _DEBUG
    printf("dict %s\n", (const char*)absolutepath.u8string().c_str());
#endif

    for(const directory_entry& x: directory_iterator(entry)) {
        if(x.is_regular_file()) {
            build_file(x);
        }
    }
    for(const directory_entry& x: directory_iterator(entry)) {
        if(x.is_directory() && !is_hidden(x)) {
            build_directory(x);
        }
    }
}

bool Builder::compress(const char* file)
{
    assert(nullptr != file);
    FILE* f = fopen(file, "wb");
    if(nullptr == f){
        return false;
    }
    XXH32_state_t* xx32_state = XXH32_createState();
    XXH32_reset(xx32_state, HashSeed);
    for(u32 i=0; i<files_.size(); ++i){
        File& entry = files_[i];
    }
    XXH32_freeState(xx32_state);
    fclose(f);
    return true;
}

//--- PhySF
//-------------------------------------------------------------------
PhySF::PhySF()
{
}

PhySF::~PhySF()
{
}
bool PhySF::open(const char* filepath)
{
    assert(nullptr != filepath);
    std::filesystem::path path(filepath);
    if(!std::filesystem::exists(path)){
        return false;
    }
    if(!std::filesystem::is_directory(path)){
        return false;
    }
    root_ = path;
    return true;
}

void PhySF::close()
{
}
} // namespace sfs
