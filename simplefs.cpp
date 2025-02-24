#include "simplefs.h"
#include <cassert>
#include <cstring>
#ifdef _DEBUG
#    include <cstdio>
#endif
#include <lz4.h>
#include <lz4hc.h>
#include <mimalloc.h>
#include <xxhash.h>

#define SFS_MALLOC(size) ::mi_malloc(size)
#define SFS_FREE(ptr) ::mi_free(ptr)

#ifdef _MSC_VER
#ifndef SFS_FSEEK
#define SFS_FSEEK(f,p,o) _fseeki64((f),(p),(o))
#endif

#ifndef SFS_FTELL
#define SFS_FTELL(f) _ftelli64((f))
#endif

#else
#ifndef SFS_FSEEK
#define SFS_FSEEK(f,p,o) fseeko64((f),(p),(o))
#endif

#ifndef SFS_FTELL
#define SFS_FTELL(f) ftello64((f))
#endif
#endif

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
    for(u32 i = 0; i < size_; ++i) {
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
void Array<T>::resize(u32 newSize)
{
    if(capacity_ < newSize) {
        expand(newSize);
    }
    assert(newSize <= capacity_);
    for(u32 i = size_; i < newSize; ++i) {
        new(&items_[i]) T();
    }
    size_ = newSize;
}

template<class T>
void Array<T>::push_back(const T& x)
{
    if(capacity_ <= size_) {
        expand(capacity_ + 16);
    }
    new(&items_[size_]) T(x);
    ++size_;
}

template<class T>
void Array<T>::pop_back()
{
    assert(0 < size_);
    --size_;
    items_[size_].~T();
}

template<class T>
void Array<T>::insert(u32 index, const T& x)
{
    if(capacity_ <= size_) {
        expand(capacity_ + 16);
    }
    ++size_;
    for(u32 i = size_ - 1; index < i; --i) {
        items_[i] = std::move(items_[i - 1]);
    }
    items_[index] = x;
}

template<class T>
void Array<T>::remove_at(u32 index)
{
    assert(index < size_);
    items_[index].~T();
    for(u32 i = index + 1; i < size_; ++i) {
        items_[i - 1] = std::move(items_[i]);
    }
    --size_;
}

template<class T>
void Array<T>::expand(u32 newCapacity)
{
    u32 capacity = capacity_;
    while(capacity < newCapacity) {
        capacity += 16;
    }
    T* items = (T*)SFS_MALLOC(sizeof(T) * capacity);
    for(u32 i = 0; i < size_; ++i) {
        new(&items[i]) T(items_[i]);
        items_[i].~T();
    }
    SFS_FREE(items_);
    capacity_ = capacity;
    items_ = items;
}

//--- BufferPool
//--------------------------------------------------------
BufferPool::BufferPool()
    :size_(0)
    ,buffer_(nullptr)
{
}

BufferPool::~BufferPool()
{
    clear();
}

u32 BufferPool::size() const
{
    return size_;
}

void BufferPool::clear()
{
    size_ = 0;
    SFS_FREE(buffer_);
    buffer_ = nullptr;
}

void* BufferPool::get(u32 size)
{
    if(size<=size_){
        return buffer_;
    }
    SFS_FREE(buffer_);
    size_ = (size+15UL) & ~15UL;
    buffer_ = SFS_MALLOC(size_);
    return buffer_;
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

    u32 count_files(std::filesystem::directory_entry& entry)
    {
        using namespace std::filesystem;
        u32 count = 0;
        for(const directory_entry& x: directory_iterator(entry)) {
            if(x.is_regular_file()) {
                ++count;
            } else if(x.is_directory() && !is_hidden(x)) {
                ++count;
            }
            return count;
        }
    }
} // namespace

Builder::Builder()
{
}

Builder::~Builder()
{
}

bool Builder::build(const char* root, const char* outfile, const Param& param)
{
    assert(nullptr != root);
    assert(nullptr != outfile);
    using namespace std::filesystem;
    directory_entry root_entry(root);
    if(!root_entry.is_directory() || is_hidden(root_entry)) {
        return false;
    }
    files_.clear();
    names_.clear();
    filepath_.clear();
    files_.resize(1);
    filepath_.resize(1);
    add_directory(0, root_entry, u8"");
    return compress(outfile, param);
}

void Builder::add_file(u32 index, const std::filesystem::directory_entry& entry)
{
    assert(index < files_.size());
    std::u8string name = entry.path().filename().u8string();
    File& file = files_[index];
    file.size_offset_.original_size_ = static_cast<u32>(entry.file_size());
    file.size_offset_.offset_ = 0;
    file.name_offset_ = static_cast<u32>(names_.size());
    file.name_length_ = static_cast<u16>(name.length());
    file.type_ = static_cast<u8>(Type::File);
    file.compression_ = 0;
    for(size_t i = 0; i < name.length(); ++i) {
        names_.push_back(name[i]);
    }
    filepath_[index] = std::filesystem::absolute(entry.path());
#ifdef _DEBUG
    printf("file %s: size:%lld\n", (const char*)filepath_[index].u8string().c_str(), entry.file_size());
#endif
}

void Builder::add_directory(u32 index, const std::filesystem::directory_entry& entry, const std::u8string& name)
{
    assert(index < files_.size());
    using namespace std::filesystem;
    u32 num_children = 0;
    for(const directory_entry& x: directory_iterator(entry)) {
        if((x.is_regular_file() || x.is_directory()) && !is_hidden(x)) {
            ++num_children;
        }
    }

    File& file = files_[index];
    file.children_.num_children_ = num_children;
    file.children_.child_start_ = static_cast<u32>(files_.size());
    file.name_offset_ = static_cast<u32>(names_.size());
    file.name_length_ = static_cast<u16>(name.length());
    file.type_ = static_cast<u8>(Type::Directory);
    file.compression_ = 0;
    for(size_t i = 0; i < name.length(); ++i) {
        names_.push_back(name[i]);
    }
    filepath_[index] = absolute(entry.path());
#ifdef _DEBUG
    printf("dict %s\n", (const char*)filepath_[index].u8string().c_str());
#endif
    u64 newSize = file.children_.child_start_ + num_children;
    files_.resize(newSize);
    filepath_.resize(newSize);
    index = files_[index].children_.child_start_;
    for(const directory_entry& x: directory_iterator(entry)) {
        if(is_hidden(x)) {
            continue;
        }
        if(x.is_regular_file()) {
            add_file(index, x);
        } else if(x.is_directory()) {
            add_directory(index, x, x.path().filename().u8string());
        }
        ++index;
    }
}

namespace
{
    bool compress_internal(File& entry, const std::filesystem::path& filepath, u64& data_offset, FILE* archive, const Builder::Param& param)
    {
        assert(nullptr != archive);
        assert(entry.size_offset_.original_size_ <= 0x7FFF'FFFFUL);
#ifdef _MSC_VER
        FILE* file = nullptr;
        fopen_s(&file, (const char*)filepath.u8string().c_str(), "rb");
#else
        FILE* file = fopen((const char*)filepath.u8string().c_str(), "rb");
#endif
        if(nullptr == file) {
            return false;
        }
        u8* bytes = static_cast<u8*>(SFS_MALLOC(entry.size_offset_.original_size_));
        if(nullptr == bytes) {
            fclose(file);
            return false;
        }
        if(fread(bytes, entry.size_offset_.original_size_, 1, file) <= 0) {
            SFS_FREE(bytes);
            fclose(file);
            return false;
        }
        fclose(file);
        if(param.compression_ == Compression::Raw || entry.size_offset_.original_size_ <= param.minimum_size_to_compress_) {
            if(fwrite(bytes, entry.size_offset_.original_size_, 1, archive) <= 0) {
                SFS_FREE(bytes);
                return false;
            }
            entry.size_offset_.offset_ = data_offset;
            entry.size_offset_.compressed_size_ = entry.size_offset_.original_size_;
            entry.compression_ = (uint8_t)Compression::Raw;
            data_offset += entry.size_offset_.original_size_;
        } else {
            int32_t size_bound = LZ4_compressBound((int32_t)entry.size_offset_.original_size_);
            char* dst = static_cast<char*>(SFS_MALLOC(size_bound));
            if(nullptr == dst) {
                SFS_FREE(bytes);
                return false;
            }
            int32_t compressed_size = LZ4_compress_HC((const char*)bytes, (char*)dst, (int32_t)entry.size_offset_.original_size_, size_bound, LZ4HC_CLEVEL_OPT_MIN);
            if(0 == compressed_size) {
                SFS_FREE(dst);
                SFS_FREE(bytes);
                return false;
            }
            if(fwrite(dst, compressed_size, 1, archive) <= 0) {
                SFS_FREE(dst);
                SFS_FREE(bytes);
                return false;
            }
            entry.size_offset_.offset_ = data_offset;
            entry.size_offset_.compressed_size_ = compressed_size;
            entry.compression_ = (uint8_t)Compression::LZ4;
            data_offset += compressed_size;
            SFS_FREE(dst);
        }
        SFS_FREE(bytes);
        return true;
    }
} // namespace

bool Builder::compress(const char* file, const Param& param)
{
    assert(nullptr != file);
#ifdef _MSC_VER
    FILE* f = nullptr;
    fopen_s(&f, file, "w+b");
#else
    FILE* f = fopen(file, "w+b");
#endif
    if(nullptr == f) {
        return false;
    }
    Header header = {};
    header.magic_ = Magic;
    header.num_entries_ = static_cast<u32>(files_.size());
    header.name_ = sizeof(Header) + static_cast<u32>(sizeof(File) * files_.size());
    header.data_ = header.name_ + static_cast<u32>(names_.size());
    if(::fwrite(&header, sizeof(Header), 1, f) <= 0) {
        fclose(f);
        return false;
    }
    if(::fwrite(&files_[0], sizeof(File) * files_.size(), 1, f) <= 0) {
        fclose(f);
        return false;
    }
    if(::fwrite(&names_[0], names_.size(), 1, f) <= 0) {
        fclose(f);
        return false;
    }

    u64 data_offset = 0;
    for(u32 i = 0; i < files_.size(); ++i) {
        if(static_cast<u8>(Type::Directory) == files_[i].type_) {
            continue;
        }
        File& entry = files_[i];
        if(!compress_internal(entry, filepath_[i], data_offset, f, param)) {
            fclose(f);
            return false;
        }
    }
    fflush(f);
    XXH64_state_t* hash_state = XXH64_createState();
    XXH64_reset(hash_state, HashSeed);
    fseek(f, sizeof(Header), SEEK_SET);
    for(;;){
        static constexpr size_t BufferSize = 16*1024;
        char buffer[BufferSize];
        size_t r = fread(buffer, 1, BufferSize, f);
        if(ferror(f)){
            XXH64_freeState(hash_state);
            fclose(f);
            return false;
        }
        if(0<r){
            XXH64_update(hash_state, buffer, r);
        }
        if(feof(f)){
            break;
        }
    }

    header.hash_ = XXH64_digest(hash_state);
    XXH64_freeState(hash_state);
    hash_state = nullptr;
    fseek(f, 0, SEEK_SET);
    if(::fwrite(&header, sizeof(Header), 1, f) <= 0) {
        return false;
    }
    if(::fwrite(&files_[0], sizeof(File) * files_.size(), 1, f) <= 0) {
        return false;
    }
    fclose(f);
    return true;
}

//--- DirectoryIterator
//-------------------------------------------------------------------
DirectoryIterator::DirectoryIterator(IFile* parent, IFile* file, u32 index)
    : parent_(parent)
    , file_(file)
    , index_(index)
{
    assert(nullptr != parent_);
}

DirectoryIterator::~DirectoryIterator()
{
    parent_ = nullptr;
    if(nullptr != file_) {
        file_->close();
        file_ = nullptr;
    }
    index_ = 0;
}

DirectoryIterator::DirectoryIterator(DirectoryIterator&& other)
    : parent_(other.parent_)
    , file_(other.file_)
    , index_(other.index_)
{
    other.parent_ = nullptr;
    other.file_ = nullptr;
    other.index_ = 0;
}

DirectoryIterator& DirectoryIterator::operator=(DirectoryIterator&& other)
{
    if(this != &other) {
        parent_ = other.parent_;
        file_ = other.file_;
        index_ = other.index_;
        other.parent_ = nullptr;
        other.file_ = nullptr;
        other.index_ = 0;
    }
    return *this;
}

DirectoryIterator::operator bool() const
{
    return nullptr != file_;
}

IFile& DirectoryIterator::operator*()
{
    assert(nullptr != file_);
    return *file_;
}

IFile* DirectoryIterator::operator->()
{
    assert(nullptr != file_);
    return file_;
}

void DirectoryIterator::operator++()
{
    assert(nullptr != file_);
    parent_->next(*this);
}

namespace
{
    size_t name_length(const char* begin, const char* end)
    {
        const char* c = begin;
        while(c < end) {
            if('/' == c[0] || '\0' == c[0]) {
                break;
            }
            ++c;
        }
        return static_cast<size_t>(std::distance(begin, c));
    }

    bool equals(const std::filesystem::directory_entry& entry, size_t len, const char* filename)
    {
        std::u8string name = entry.path().filename().u8string();
        if(len != name.length()) {
            return false;
        }
        for(size_t i = 0; i < len; ++i) {
            if(name[i] != filename[i]) {
                return false;
            }
        }
        return true;
    }

    bool equals(size_t len0, const char* x0, size_t len1, const char* x1)
    {
        if(len0 != len1){
            return false;
        }
        for(size_t i = 0; i < len0; ++i) {
            if(x0[i] != x1[i]){
                return false;
            }
        }
        return true;
    }
} // namespace

//--- PhyFile
//-------------------------------------------------------------------
PhyFile::PhyFile()
    : fs_(nullptr)
    , is_file_(false)
    , size_(0)
{
}

PhyFile::~PhyFile()
{
    is_file_ = false;
    size_ = 0;
}

void PhyFile::close()
{
    if(nullptr != fs_) {
        fs_->close_file(this);
        fs_ = nullptr;
    }
}

u32 PhyFile::original_size() const
{
    return size_;
}

u32 PhyFile::compressed_size() const
{
    return size_;
}

bool PhyFile::is_file() const
{
    return is_file_;
}

u32 PhyFile::num_children() const
{
    return children_.size();
}

DirectoryIterator PhyFile::begin()
{
    using namespace std::filesystem;
    if(children_.size()<=0){
        return DirectoryIterator(this, nullptr, 0);
    }
    PhyFile* file = fs_->pop();
    file->initialize(fs_, children_[0]);
    return DirectoryIterator(this, file, 0);
}

void PhyFile::next(DirectoryIterator& itr)
{
    using namespace std::filesystem;
    assert(nullptr != itr.parent_);
    if(nullptr == itr.file_) {
        return;
    }
    itr.file_->close();
    itr.file_ = nullptr;
    u32 next = itr.index_ + 1;
    if(children_.size()<=next){
        return;
    }
    PhyFile* file = fs_->pop();
    file->initialize(fs_, children_[next]);
    itr.file_ = file;
    itr.index_ = next;
}

std::u8string_view PhyFile::filename() const
{
    return filename_;
}

u32 PhyFile::read(void* dst)
{
    assert(nullptr != fs_);

    #ifdef _MSC_VER
    FILE* file = nullptr;
    fopen_s(&file, (const char*)filepath_.c_str(), "rb");
    #else
    FILE* file = fopen((const char*)filepath_.c_str(), "rb");
    #endif
    if(nullptr == file){
        return 0;
    }
    size_t r = fread(dst, size_, 1, file);
    fclose(file);
    return r;
}

void PhyFile::initialize(PhyFS* fs, const std::filesystem::directory_entry& entry)
{
    assert(nullptr != fs);
    assert(!is_hidden(entry));
    using namespace std::filesystem;
    fs_ = fs;
    is_file_ = entry.is_regular_file();
    size_ = static_cast<u32>(entry.file_size());
    filepath_ = entry.path().u8string();
    filename_ = entry.path().filename().u8string();
    if(!is_file_) {
        for(const directory_entry& x: directory_iterator(entry)) {
            if(is_hidden(x)) {
                continue;
            }
            if(x.is_regular_file() || x.is_directory()) {
                children_.push_back(x);
            }
        }
    }
}

//--- PhyFS
//-------------------------------------------------------------------
PhyFS::PhyFS()
    : opend_(0)
    , pages_(nullptr)
    , entries_(nullptr)
{
}

PhyFS::~PhyFS()
{
    while(nullptr != pages_) {
        Page* next = pages_->next_;
        SFS_FREE(pages_);
        pages_ = next;
    }
    entries_ = nullptr;
}

bool PhyFS::open(const char* filepath)
{
    assert(nullptr != filepath);
    std::filesystem::path path(filepath);
    if(!std::filesystem::exists(path)) {
        return false;
    }
    if(!std::filesystem::is_directory(path)) {
        return false;
    }
    std::error_code error;
    root_ = std::filesystem::directory_entry(path, error);
    return error ? false : true;
}

void PhyFS::close()
{
}

IFile* PhyFS::open_file(const char* filepath)
{
    assert(nullptr != filepath);
    const char* begin = '/' == filepath[0] ? filepath + 1 : filepath;
    size_t len = strnlen(begin, MaxPath);
    if(len<=0){
        PhyFile* file = pop();
        file->initialize(this, root_);
        return file;
    }
    return open_file(root_, begin, begin + len);
}

bool PhyFS::close_file(IFile* file)
{
    assert(nullptr != file);
    std::uintptr_t p = (std::uintptr_t)file;
    Page* page = pages_;
    while(nullptr != page) {
        if((std::uintptr_t)page < p && p < ((std::uintptr_t)page + PageSize)) {
            Entry* f = (Entry*)file;
            f->file_.~PhyFile();
            push(f);
            return true;
        }
        page = page->next_;
    }
    return false;
}

IFile* PhyFS::open_file(const std::filesystem::directory_entry& root, const char* begin, const char* end)
{
    using namespace std::filesystem;
    size_t len = name_length(begin, end);
    const char* next = '/' == begin[len] ? begin + len + 1 : begin + len;
    for(const directory_entry& x: directory_iterator(root)) {
        if(is_hidden(x)) {
            continue;
        }
        if(equals(x, len, begin)) {
            if('\0' == next[0]) {
                PhyFile* file = pop();
                file->initialize(this, x);
                return file;
            }
            return open_file(x, next, end);
        }
    }
    return nullptr;
}

PhyFile* PhyFS::pop()
{
    if(nullptr == entries_) {
        alloc_page();
        assert(nullptr != entries_);
    }
    Entry* entry = entries_;
    entries_ = entries_->next_;
    ++opend_;
    return new(&entry->file_) PhyFile();
}

void PhyFS::push(Entry* f)
{
    --opend_;
    f->next_ = entries_;
    entries_ = f;
}

void PhyFS::alloc_page()
{
    Page* page = (Page*)SFS_MALLOC(PageSize);
    Entry* entries = reinterpret_cast<Entry*>(&page[1]);
    u32 num_entries = (PageSize - sizeof(Page)) / sizeof(Entry);
    assert(num_entries * sizeof(Entry) <= (PageSize - sizeof(Page)));
    for(u32 i = 1; i < num_entries; ++i) {
        entries[i - 1].next_ = &entries[i];
    }
    entries[num_entries - 1].next_ = entries_;
    entries_ = entries;
    page->next_ = pages_;
    pages_ = page;
}

//--- PacFile
//-------------------------------------------------------------------
PacFile::PacFile()
    : fs_(nullptr)
    , file_(nullptr)
{
}

PacFile::~PacFile()
{
    fs_ = nullptr;
    file_ = nullptr;
}

void PacFile::close()
{
    if(nullptr != fs_) {
        fs_->close_file(this);
        fs_ = nullptr;
    }
    file_ = nullptr;
}

u32 PacFile::original_size() const
{
    return file_->type_==(u8)Type::File? file_->size_offset_.original_size_ : 0;
}

u32 PacFile::compressed_size() const
{
    return file_->type_==(u8)Type::File? file_->size_offset_.compressed_size_ : 0;
}

bool PacFile::is_file() const
{
    return file_->type_==(u8)Type::File;
}

u32 PacFile::num_children() const
{
    return file_->type_==(u8)Type::Directory? file_->children_.num_children_ : 0;
}

DirectoryIterator PacFile::begin()
{
    if(file_->type_ != (u8)Type::Directory || file_->children_.num_children_<=0){
        return DirectoryIterator(this, nullptr, 0);
    }
    PacFile* file = fs_->pop();
    const File& child = fs_->files_[file_->children_.child_start_];
    file->initialize(fs_, &child);
    return DirectoryIterator(this, file, 0);
}

void PacFile::next(DirectoryIterator& itr)
{
    assert(nullptr != itr.parent_);
    if(nullptr == itr.file_) {
        return;
    }
    itr.file_->close();
    itr.file_ = nullptr;
    u32 next = itr.index_ + 1;
    if(file_->children_.num_children_<=next){
        return;
    }
    PacFile* file = fs_->pop();
    const File& child = fs_->files_[file_->children_.child_start_+next];
    file->initialize(fs_, &child);
    itr.file_ = file;
    itr.index_ = next;
}

std::u8string_view PacFile::filename() const
{
    return std::u8string_view((const char8_t*)&fs_->names_[file_->name_offset_], file_->name_length_);
}

u32 PacFile::read(void* dst)
{
    assert(nullptr != fs_);
    assert(nullptr != file_);
    assert(is_file());
    u32 original_size = file_->size_offset_.original_size_;
    u32 compressed_size = file_->size_offset_.compressed_size_;
    u64 offset = file_->size_offset_.offset_ + fs_->header_.data_;

    SFS_FSEEK(fs_->file_, static_cast<int64_t>(offset), SEEK_SET);
    if((u8)Compression::Raw == file_->compression_){
        return ::fread(dst, original_size, 1, fs_->file_);
    }
    void* buffer = fs_->get_buffer(compressed_size);
    if(::fread(buffer, compressed_size, 1, fs_->file_)<=0){
        return 0;
    }
    int32_t r = LZ4_decompress_safe((const char*)buffer, (char*)dst, static_cast<int32_t>(compressed_size), static_cast<int32_t>(original_size));
    return original_size == static_cast<u32>(r) ? 1 : 0;
}

void PacFile::initialize(PacFS* fs, const File* file)
{
    assert(nullptr != fs);
    fs_ = fs;
    file_ = file;
}

//--- PacFS
//-------------------------------------------------------------------
PacFS::PacFS()
    : file_(0)
    , header_{}
    , files_(nullptr)
    , opend_(0)
    , pages_(nullptr)
    , entries_(nullptr)
    , names_(nullptr)
{
}

PacFS::~PacFS()
{
    close();
}

bool PacFS::open(const char* filepath)
{
    assert(nullptr != filepath);
    std::filesystem::path path(filepath);
    if(!std::filesystem::exists(path)) {
        return false;
    }
    if(!std::filesystem::is_regular_file(path)) {
        return false;
    }
    close();
#ifdef _MSC_VER
    fopen_s(&file_, (const char*)path.u8string().c_str(), "rb");
#else
    file_ = fopen((const char*)path.u8string().c_str(), "rb");
#endif
    if(nullptr == file_){
        return false;
    }
    if(fread(&header_, sizeof(Header), 1, file_)<=0 || Magic != header_.magic_){
        fclose(file_);
        file_ = nullptr;
        return false;
    }
    size_t size_entries = sizeof(File)*header_.num_entries_;
    size_t size_names = (header_.data_-header_.name_+0x03UL) & ~0x03UL;
    size_t size = size_entries + size_names;
    files_ = (File*)SFS_MALLOC(size);
    names_ = (const char*)files_ + size_entries;
    if(nullptr == files_){
        fclose(file_);
        file_ = nullptr;
        return false;
    }
    if(fread(files_, size, 1, file_)<=0){
        SFS_FREE(files_);
        files_ = nullptr;
        fclose(file_);
        file_ = nullptr;
        return false;
    }
    return true;
}

void PacFS::close()
{
    if(nullptr != file_) {
        fclose(file_);
        file_ = nullptr;
    }
    SFS_FREE(files_);
    files_ = nullptr;
    names_ = nullptr;
    while(nullptr != pages_) {
        Page* next = pages_->next_;
        SFS_FREE(pages_);
        pages_ = next;
    }
    entries_ = nullptr;
}

IFile* PacFS::open_file(const char* filepath)
{
    assert(nullptr != filepath);
    const char* begin = '/' == filepath[0] ? filepath + 1 : filepath;
    size_t len = strnlen(begin, MaxPath);
    if(len<=0){
        PacFile* file = pop();
        file->initialize(this, &files_[0]);
        return file;
    }
    return open_file(0, begin, begin + len);
}

bool PacFS::close_file(IFile* file)
{
    assert(nullptr != file);
    std::uintptr_t p = (std::uintptr_t)file;
    Page* page = pages_;
    while(nullptr != page) {
        if((std::uintptr_t)page < p && p < ((std::uintptr_t)page + PageSize)) {
            Entry* f = (Entry*)file;
            f->file_.~PacFile();
            push(f);
            return true;
        }
        page = page->next_;
    }
    return false;
}

IFile* PacFS::open_file(u32 root, const char* begin, const char* end)
{
    size_t len = name_length(begin, end);
    const char* next = '/' == begin[len] ? begin + len + 1 : begin + len;
    File& root_file = files_[root];
    assert(root_file.type_ == (u8)Type::Directory);
    for(u32 i=0; i<root_file.children_.num_children_; ++i){
        u32 index = root_file.children_.child_start_+i;
        const File& entry = files_[index];
        const char* filename = &names_[entry.name_offset_];
        if(equals(entry.name_length_, filename, len, begin)){
            if('\0' == next[0]) {
                PacFile* file = pop();
                file->initialize(this, &entry);
                return file;
            }
            return open_file(index, next, end);
        }
    }
    return nullptr;
}

PacFile* PacFS::pop()
{
    if(nullptr == entries_) {
        alloc_page();
        assert(nullptr != entries_);
    }
    Entry* entry = entries_;
    entries_ = entries_->next_;
    ++opend_;
    return new(&entry->file_) PacFile();
}

void PacFS::push(Entry* f)
{
    --opend_;
    f->next_ = entries_;
    entries_ = f;
}

void PacFS::alloc_page()
{
    Page* page = (Page*)SFS_MALLOC(PageSize);
    Entry* entries = reinterpret_cast<Entry*>(&page[1]);
    u32 num_entries = (PageSize - sizeof(Page)) / sizeof(Entry);
    assert(num_entries * sizeof(Entry) <= (PageSize - sizeof(Page)));
    for(u32 i = 1; i < num_entries; ++i) {
        entries[i - 1].next_ = &entries[i];
    }
    entries[num_entries - 1].next_ = entries_;
    entries_ = entries;
    page->next_ = pages_;
    pages_ = page;
}

void* PacFS::get_buffer(u32 size)
{
    return buffer_pool_.get(size);
}

//--- VFS
//-------------------------------------------------------------------
VFS::VFS()
{
}

VFS::~VFS()
{
    for(u32 i = 0; i < fs_.size(); ++i) {
        fs_[i]->close();
        delete fs_[i];
    }
    fs_.clear();
}

bool VFS::add_phyfs(const char* root)
{
    assert(nullptr != root);
    PhyFS* fs = new PhyFS();
    if(!fs->open(root)) {
        delete fs;
        return false;
    }
    fs_.insert(0, fs);
    return true;
}

bool VFS::add_pacfs(const char* file)
{
    assert(nullptr != file);
    PacFS* fs = new PacFS();
    if(!fs->open(file)) {
        delete fs;
        return false;
    }
    fs_.insert(0, fs);
    return true;
}

IFile* VFS::open_file(const char* filepath)
{
    for(u32 i = 0; i < fs_.size(); ++i) {
        IFile* file = fs_[i]->open_file(filepath);
        if(nullptr != file) {
            return file;
        }
    }
    return nullptr;
}

bool VFS::close_file(IFile* file)
{
    for(u32 i = 0; i < fs_.size(); ++i) {
        if(fs_[i]->close_file(file)) {
            return true;
        }
    }
    return false;
}

} // namespace sfs
