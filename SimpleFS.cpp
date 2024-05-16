/**
@author t-sakai
@file SimpleFS.cpp
*/
#include "SimpleFS.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <new>

#ifdef _WIN32
#    include <fileapi.h>
#else
#    include <dirent.h>
#    include <sys/types.h>
#endif

namespace sfs
{
namespace
{
#ifdef _WIN32
    u64 toU64(const FILETIME& time)
    {
        u64 result = time.dwHighDateTime;
        result <<= 32;
        result |= time.dwLowDateTime;
        return result;
    }
#endif

    /**
     * @brief Replaces all occurrences of a source character with a destination character in a given string.
     * @param str ... Pointer to the input string where replacements will be made.
     * @param dst ... The character to replace all occurrences of the source character with.
     * @param src ... The character to be replaced in the input string.
     */
    void replace(char* str, char dst, char src)
    {
        assert(nullptr != str);
        while('\0' != *str) {
            if(src == *str) {
                *str = dst;
            }
            ++str;
        }
    }

    /**
     * @brief Replaces the end of a string with a specified character.
     * @param length ... The length of the string to be modified.
     * @param str ... The input string to be modified.
     * @param end ... The character to replace the last character of the string with.
     * @return The new length of the modified string.
     */
    size_t replace_endwith(size_t length, char* str, char end)
    {
        if(length <= 0) {
            return 0;
        }
        if(str[length - 1] != end) {
            str[length] = end;
            str[length + 1] = '\0';
            return length + 1;
        }
        return length;
    }
} // namespace

IFileHandle::IFileHandle()
{
}

IFileHandle::~IFileHandle()
{
}

IDirectoryHandle::IDirectoryHandle()
{
}

IDirectoryHandle::~IDirectoryHandle()
{
}

IFileSystem::IFileSystem()
{
}

IFileSystem::~IFileSystem()
{
}

//--------------------------------------
#if _WIN32
FileIterator::FileIterator(const char* path)
    : path_(path)
    , handle_(handle)
    , data_(data)
    , fileSystem_(nullptr)
    , entry_(nullptr)
    , count_(0)
{
}

FileIterator(FileSystemPac* fileSystem, PacEntry* entry)
    : path_(nullptr)
    , handle_(nullptr)
    , data_(data)
    , fileSystem_(fileSystem)
    , entry_(entry)
    , count_(0)
{
}

FileIterator::FileIterator(const FileIterator& other)
    : path_(other.path_)
    , handle_(other.handle_)
    , data_(other.data_)
    , fileSystem_(other.fileSystem_)
    , entry_(other.entry_)
    , count_(other.count_)
{
}

FileIterator::FileIterator(FileIterator&& other)
    : path_(other.path_)
    , handle_(other.handle_)
    , data_(other.data_)
    , fileSystem_(other.fileSystem_)
    , entry_(other.entry_)
    , count_(other.count_)
{
    other.path_ = nullptr;
    other.handle_ = nullptr;
    other.data_ = {};
    other.fileSystem_ = nullptr;
    other.entry_ = nullptr;
    other.count_ = 0;
}

FileIterator::~FileIterator()
{
}

FileIterator& FileIterator::operator=(const FileIterator& other)
{
    if(this != &other) {
        path_ = other.path_;
        handle_ = other.handle_;
        data_ = other.data_;
        fileSystem_ = other.fileSystem_;
        entry_ = other.entry_;
        count_ = other.count_;
    }
    return *this;
}

FileIterator& FileIterator::operator=(FileIterator&& other)
{
    if(this != &other) {
        path_ = other.path_;
        handle_ = other.handle_;
        data_ = other.data_;
        fileSystem_ = other.fileSystem_;
        entry_ = other.entry_;
        count_ = other.count_;
        other.path_ = nullptr;
        other.handle_ = nullptr;
        other.data_ = {};
        other.fileSystem_ = nullptr;
        other.entry_ = nullptr;
        other.count_ = 0;
    }
    return *this;
}

void FileIterator::close()
{
    if(nullptr != handle_) {
        FindClose(handle_);
        handle_ = nullptr;
    }
}

bool FileIterator::next()
{
    if(nullptr != fileSystem_) {
        ++count_;
        if(count_ <= entry_->getNumChildren()) {
            return false;
        }
        return true;
    }

    if(nullptr == handle_) {
        handle_ = FindFirstFileA(path_, &data_);
        if(nullptr == handle_) {
            return false;
        }
        return true;
    }
    return TRUE == FindNextFileA(handle_, &data_);
}

FileTypeFlag FileIterator::type() const
{
    if(nullptr != fileSystem_) {
        assert(nullptr != entry_);
        assert(count_ < entry_->getNumChildren());
        u32 index = entry_->childStart_ + count_;
        fileSystem_->
    }

    assert(nullptr != handle_);
    if(FILE_ATTRIBUTE_NORMAL == (data_.dwFileAttributes & FILE_ATTRIBUTE_NORMAL)) {
        return FileTypeFlag::File;
    }
    if(FILE_ATTRIBUTE_DIRECTORY == (data_.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return FileTypeFlag::Directory;
    }
    return FileTypeFlag::None;
}

const char* FileIterator::path() const
{
    return data_.cFileName;
}

#else
FileIterator::FileIterator(DIR* handle, struct dirent* current)
    : handle_(handle)
    , current_(current)
    , fileSystem_(nullptr)
    , entry_(nullptr)
    , count_(0)
{
}

FileIterator::FileIterator(FileIterator& other)
    : handle_(other.handle_)
    , current_(other.current_)
    , fileSystem_(other.fileSystem_)
    , entry_(other.entry_)
    , count_(other.count_)
{
    other.handle_ = nullptr;
    other.current_ = nullptr;
    other.fileSystem_ = nullptr;
    other.entry_ = nullptr;
    other.count_ = nullptr;
}

FileIterator::FileIterator(FileIterator&& other)
    : handle_(other.handle_)
    , current_(other.current_)
    , fileSystem_(other.fileSystem_)
    , entry_(other.entry_)
    , count_(other.count_)
{
    other.handle_ = nullptr;
    other.current_ = nullptr;
    other.fileSystem_ = nullptr;
    other.entry_ = nullptr;
    other.count_ = 0;
}

FileIterator::~FileIterator()
{
    if(nullptr != handle_) {
        closedir(handle_);
        handle_ = nullptr;
    }
}

FileIterator& FileIterator::operator=(FileIterator&& other)
{
    if(this != &other) {
        handle_ = other.handle_;
        current_ = other.current_;
        fileSystem_ = other.fileSystem_;
        entry_ = other.entry_;
        count_ = other.count_;
        other.handle_ = nullptr;
        other.current_ = nullptr;
        other.fileSystem_ = nullptr;
        other.entry_ = nullptr;
        other.count_ = 0;
    }
    return *this;
}

bool FileIterator::next()
{
    if(nullptr != fileSystem_) {
        ++count_;
        if(count_ <= entry_->getNumChildren()) {
            return false;
        }
        return true;
    }

    if(nullptr == handle_) {
        return false;
    }
    current_ = readdir(handle_);
    return nullptr != current_;
}

FileTypeFlag FileIterator::type() const
{
    assert(nullptr != handle_);
    assert(nullptr != current_);
    if(DT_REG == (current_->d_type & DT_REG)) {
        return FileTypeFlag::Normal;
    }
    if(DT_DIR == (current_->d_type & DT_DIR)) {
        return FileTypeFlag::Directory;
    }
    return FileTypeFlag::None;
}
#endif

//--- FileHandleOS
//-------------------------------------
#ifdef _WIN32
FileHandleOS::FileHandleOS(HANDLE file)
    : file_(file)
{
}
#else
FileHandleOS::FileHandleOS(FILE* file)
    : file_(file)
{
}
#endif

FileHandleOS::~FileHandleOS()
{
}

FileStatus FileHandleOS::status() const
{
    assert(nullptr != file_);
#ifdef _WIN32
    BY_HANDLE_FILE_INFORMATION info;
    FileStatus status = {};
    if(FALSE == GetFileInformationByHandle(file_, &info)) {
        return status;
    }
    status.access_ = toU64(info.ftLastAccessTime);
    status.create_ = toU64(info.ftCreationTime);
    status.modified_ = toU64(info.ftLastWriteTime);
    status.size_ = (static_cast<u64>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
    status.compressedSize_ = status.size_;
    if(FILE_ATTRIBUTE_NORMAL == (FILE_ATTRIBUTE_NORMAL & info.dwFileAttributes)) {
        status.type_ |= static_cast<u32>(FileTypeFlag::File);
    }
    if(FILE_ATTRIBUTE_DIRECTORY == (FILE_ATTRIBUTE_DIRECTORY & info.dwFileAttributes)) {
        status.type_ |= static_cast<u32>(FileTypeFlag::Directory);
    }
    return status;
#else
    FileStatus status = {};
    s32 fd = fileno(file);
    struct stat64 st;
    if(fstat64(fd, &st) < 0) {
        return status;
    }
    status.access_ = st.st_atime;
    status.modified_ = st.st_mtime;
    status.create_ = st.st_birthtime;
    status.size_ = st.st_size;
    if(S_ISDIR(st.st_mode)) {
        status.type_ |= static_cast<u32>(FileTypeFlag::Normal);
    }
    if(S_ISREG(st.st_mode)) {
        status.type_ |= static_cast<u32>(FileTypeFlag::Directory);
    }
    return status;
#endif
}

bool FileHandleOS::seek(s64 offset, s32 dir)
{
    assert(nullptr != file_);
#ifdef _WIN32
    LARGE_INTEGER loffset;
    loffset.QuadPart = offset;
    return TRUE == SetFilePointerEx(file_, loffset, nullptr, dir);
#else
    return 0 == fseeko(file_, offset, dir);
#endif
}

s64 FileHandleOS::tell() const
{
    assert(nullptr != file_);
#ifdef _WIN32
    LARGE_INTEGER offset;
    offset.QuadPart = 0;
    if(!SetFilePointerEx(file_, offset, &offset, FILE_CURRENT)) {
        return 0;
    }
    return offset.QuadPart;
#else
    return ftello(file_);
#endif
}

s64 FileHandleOS::read(s64 size, void* ptr)
{
    assert(nullptr != file_);
#if _WIN32
    DWORD numBytesRead = 0;
    return TRUE == ReadFile(file_, ptr, static_cast<DWORD>(size), &numBytesRead, nullptr);
#else
    return 0 < fread(ptr, size, 1, file_);
#endif
}

s64 FileHandleOS::write(s64 size, const void* ptr)
{
    assert(nullptr != file_);
#if _WIN32
    DWORD numByhtesWrote = 0;
    return TRUE == WriteFile(file_, ptr, static_cast<DWORD>(size), &numByhtesWrote, nullptr);
#else
    return 0 < fwrite(ptr, size, 1, file_);
#endif
}

//--- DirectoryHandleOS
//-------------------------------------
DirectoryHandleOS::DirectoryHandleOS(char* path)
    : path_(path)
{
    assert(nullptr != path);
}

DirectoryHandleOS::~DirectoryHandleOS()
{
    ::free(path_);
}

FileIterator DirectoryHandleOS::begin()
{
    assert(nullptr != path_);
#if _WIN32
    return FileIterator(path_);
#else
#endif
}

//--- FileSystemOS
//-------------------------------------
FileSytemType FileSystemOS::getType()
{
    return FileSytemType::OS;
}

IFileHandle* FileSystemOS::openFile(const char* path)
{
    assert(nullptr != path);

    size_t length = ::strnlen(path, 255);
    size_t size = length_ + length + 1;
    buffer_.reserve(static_cast<u32>(size));
    buffer_[0] = '\0';
#ifdef _WIN32
    ::strncat_s(&buffer_[0], size, root_, length_);
    ::strncat_s(&buffer_[0], size, path, length);
#else
    ::strncat(tmp, root_, length_);
    ::strncat(tmp, path, length);
#endif

#ifdef _WIN32
    HANDLE handle = CreateFileA(
        &buffer_[0],
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if(nullptr == handle) {
        return nullptr;
    }
#else
    FILE* handle = fopen(tmp, "rb");
    if(nullptr == handle) {
        ::free(tmp);
        return nullptr;
    }
#endif
    void* ptr = pop();
    FileHandleOS* file = ::new(ptr) FileHandleOS(handle);
    return file;
}

void FileSystemOS::closeFile(IFileHandle* handle)
{
    assert(nullptr != handle);
    FileHandleOS* file = reinterpret_cast<FileHandleOS*>(handle);
    if(nullptr != file->file_) {
#ifdef _WIN32
        CloseHandle(file->file_);
#else
        fclose(file->file_);
#endif
        file->file_ = nullptr;
    }
    file->~FileHandleOS();
    push(file);
}

IDirectoryHandle* FileSystemOS::openDirectory(const char* path)
{
    assert(nullptr != path);
    size_t length = ::strnlen(path, 255);
    size_t size = length_ + length + 1;
    char* fullpath = reinterpret_cast<char*>(::malloc(size));
    if(nullptr == fullpath) {
        return nullptr;
    }
    fullpath[0] = '\0';
#ifdef _WIN32
    ::strncat_s(fullpath, size, root_, length_);
    ::strncat_s(fullpath, size, path, length);
#else
    ::strncat(fullpath, root_, length_);
    ::strncat(fullpath, path, length);
#endif
    void* ptr = pop();
    if(nullptr == ptr) {
        ::free(fullpath);
        return nullptr;
    }
    DirectoryHandleOS* directory = ::new(ptr) DirectoryHandleOS(fullpath);
    return directory;
}

void FileSystemOS::closeDirectory(IDirectoryHandle* handle)
{
    assert(nullptr != handle);
    DirectoryHandleOS* directory = reinterpret_cast<DirectoryHandleOS*>(handle);
    directory->~DirectoryHandleOS();
    push(directory);
}

FileSystemOS::FileSystemOS(const char* root)
    : length_(0)
    , root_(nullptr)
    , page_(nullptr)
    , top_(nullptr)
{
    if(nullptr == root || std::strlen(root) <= 0) {
        root = ".";
    }
#ifdef _WIN32
    u32 size = GetFullPathNameA(root, 0, nullptr, nullptr);
    if(0 < size) {
        length_ = size - 1;
        size += 1;
        root_ = reinterpret_cast<char*>(::malloc(size));
        GetFullPathNameA(root, size, root_, nullptr);
        replace(root_, '\\', '/');
        replace_endwith(length_, root_, '\\');
    }
#else
    ::strncpy(root_, root, length_);
    replace(root_, '\\', '/');
#endif
}

FileSystemOS::~FileSystemOS()
{
    ::free(root_);
    Page* page = page_;
    while(nullptr != page) {
        Page* next = page->next_;
        ::free(page);
        page = next;
    }
}

void* FileSystemOS::pop()
{
    if(nullptr == top_) {
        expand();
    }
    assert(nullptr != top_);
    void* top = top_;
    top_ = top_->next_;
    return top;
}

void FileSystemOS::push(void* handle)
{
    assert(nullptr != handle);
    Page* top = reinterpret_cast<Page*>(handle);
    top->next_ = top_;
    top_ = top;
}

void FileSystemOS::expand()
{
    u32 blockSize = static_cast<u32>(getSize());
    char* ptr = reinterpret_cast<char*>(::malloc(PageSize));
    Page* top = reinterpret_cast<Page*>(ptr + blockSize);
    u32 number = (PageSize / blockSize) - 1;
    for(u32 i = 1; i < number; ++i) {
        top[i - 1].next_ = &top[i];
    }
    top[number - 1].next_ = top_;
    top_ = top;

    Page* page = reinterpret_cast<Page*>(ptr);
    page->next_ = page_;
    page_ = page;
}

FileSystemOS::StringBuffer::StringBuffer()
    : capacity_(0)
    , buffer_(nullptr)
{
}

FileSystemOS::StringBuffer::~StringBuffer()
{
    ::free(buffer_);
}

void FileSystemOS::StringBuffer::reserve(u32 capacity)
{
    if(capacity <= capacity_) {
        return;
    }
    while(capacity_ < capacity) {
        capacity_ += 64;
    }
    ::free(buffer_);
    buffer_ = reinterpret_cast<char*>(::malloc(capacity_));
}

const char& FileSystemOS::StringBuffer::operator[](u32 index) const
{
    assert(index < capacity_);
    return buffer_[index];
}

char& FileSystemOS::StringBuffer::operator[](u32 index)
{
    assert(index < capacity_);
    return buffer_[index];
}

//--- FileHandlePac
//-------------------------------------
FileHandlePac::FileHandlePac(const PacEntry* entry, const void* data)
    : entry_(entry)
    , offset_(0)
    , data_(data)
{
    assert(nullptr != entry_);
}

FileHandlePac::~FileHandlePac()
{
}

FileStatus FileHandlePac::status() const
{
    assert(nullptr != entry_);
    FileStatus status = {};
    status.access_ = 0;
    status.create_ = 0;
    status.modified_ = 0;
    status.size_ = entry_->uncompressedSize_;
    status.compressedSize_ = entry_->compressedSize_;
    status.type_ = entry_->getType();
    return status;
}

bool FileHandlePac::seek(s64 offset, s32 dir)
{
    assert(0 <= offset);
    assert(nullptr != entry_);
    switch(dir) {
    case SEEK_CUR: {
        s64 t = offset_ + offset;
        if(static_cast<s64>(entry_->compressedSize_) < t) {
            offset_ = static_cast<s64>(entry_->compressedSize_);
        } else if(t < 0) {
            offset_ = 0;
        } else {
            offset_ = t;
        }
    }
        return true;
    case SEEK_END:
        offset_ = static_cast<s64>(entry_->compressedSize_) < offset ? 0 : (static_cast<s64>(entry_->compressedSize_) - offset);
        return true;
    case SEEK_SET:
        offset_ = static_cast<s64>(entry_->compressedSize_) < (offset_ + offset) ? static_cast<s64>(entry_->compressedSize_) : (offset_ + offset);
        return true;
    default:
        assert(false);
        break;
    }
}

s64 FileHandlePac::tell() const
{
    assert(nullptr != file_);
    return offset_;
}

s64 FileHandlePac::read(s64 size, void* ptr)
{
    assert(nullptr != entry_);
    assert(0 <= size);
    assert(nullptr != ptr);
    s64 offset = offset_ + size;
    if(entry_->compressedSize_ < offset) {
        offset = entry_->compressedSize_;
        size = offset - offset_;
    }
    ::memcpy(ptr, data_ + offset, static_cast<size_t>(size));
    offset_ = offset;
}

s64 FileHandlePac::write(s64 size, const void* ptr)
{
    assert(nullptr != entry_);
    assert(0 <= size);
    assert(nullptr != ptr);
    return 0;
}

//--- DirectoryHandlePac
//-------------------------------------
DirectoryHandlePac::DirectoryHandlePac(char* path)
    : path_(path)
{
}

DirectoryHandlePac::~DirectoryHandlePac()
{
    ::free(path_);
}

FileIterator DirectoryHandlePac::begin()
{
    assert(nullptr != path_);
#if _WIN32
    WIN32_FIND_DATAA data;
    HANDLE handle = FindFirstFileA(path_, &data);
    return FileIterator(handle, data);
#else
#endif
}

//--- FileSystemPac
//-------------------------------------
FileSytemType FileSystemPac::getType()
{
    return FileSytemType::Pac;
}

IFileHandle* FileSystemPac::openFile(const char* path)
{
    assert(nullptr != path);

    size_t length = ::strnlen(path, 255);
    size_t size = length_ + length + 1;
    buffer_.reserve(static_cast<u32>(size));
    buffer_[0] = '\0';
#ifdef _WIN32
    ::strncat_s(&buffer_[0], size, root_, length_);
    ::strncat_s(&buffer_[0], size, path, length);
#else
    ::strncat(tmp, root_, length_);
    ::strncat(tmp, path, length);
#endif

#ifdef _WIN32
    HANDLE handle = CreateFileA(
        &buffer_[0],
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if(nullptr == handle) {
        return nullptr;
    }
#else
    FILE* handle = fopen(tmp, "rb");
    if(nullptr == handle) {
        ::free(tmp);
        return nullptr;
    }
#endif
    void* ptr = pop();
    FileHandleOS* file = ::new(ptr) FileHandleOS(handle);
    return file;
}

void FileSystemPac::closeFile(IFileHandle* handle)
{
    assert(nullptr != handle);
    FileHandleOS* file = reinterpret_cast<FileHandleOS*>(handle);
    if(nullptr != file->file_) {
#ifdef _WIN32
        CloseHandle(file->file_);
#else
        fclose(file->file_);
#endif
        file->file_ = nullptr;
    }
    file->~FileHandleOS();
    push(file);
}

IDirectoryHandle* FileSystemPac::openDirectory(const char* path)
{
    assert(nullptr != path);
    size_t length = ::strnlen(path, 255);
    size_t size = length_ + length + 1;
    char* fullpath = reinterpret_cast<char*>(::malloc(size));
    if(nullptr == fullpath) {
        return nullptr;
    }
    fullpath[0] = '\0';
#ifdef _WIN32
    ::strncat_s(fullpath, size, root_, length_);
    ::strncat_s(fullpath, size, path, length);
#else
    ::strncat(fullpath, root_, length_);
    ::strncat(fullpath, path, length);
#endif
    void* ptr = pop();
    if(nullptr == ptr) {
        ::free(fullpath);
        return nullptr;
    }
    DirectoryHandleOS* directory = ::new(ptr) DirectoryHandleOS(fullpath);
    return directory;
}

void FileSystemPac::closeDirectory(IDirectoryHandle* handle)
{
    assert(nullptr != handle);
    DirectoryHandleOS* directory = reinterpret_cast<DirectoryHandleOS*>(handle);
    directory->~DirectoryHandleOS();
    push(directory);
}

FileSystemPac::FileSystemPac()
    : file_(nullptr)
    , header_{}
    , entries_(nullptr)
{
}

bool FileSystemPac::open(const char* path)
{
    assert(nullptr != path);
#ifdef _WIN32
    HANDLE handle = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if(nullptr == handle) {
        return false;
    }
    LARGE_INTEGER size;
    if(FALSE == GetFileSizeEx(handle, &size)){
        CloseHandle(handle);
        return false;
    }
    if(size.QuadPart<sizeof(PacHeader)){
        CloseHandle(handle);
        return false;
    }
    PacHeader header;
    if(TRUE == ReadFile(handle, &header, static_cast<DWORD>(sizeof(PacHeader)), nullptr, nullptr)){
        CloseHandle(handle);
        return false;
    }
    static const u8 Magic[] = {'l','p', 'a', 'c'};
    if(0 != ::memcmp(Magic, &header.magic, 4)){
        CloseHandle(handle);
        return false;
    }
    file_ = handle;
#else
    FILE* handle = fopen(path, "rb");
    if(nullptr == handle){
        return false;
    }
    file_ = handle;
#endif
    return true;
}

FileSystemPac::~FileSystemPac()
{
#ifdef _WIN32
    CloseHandle(file_);
#else
    fclose(file_);
#endif
    header_ = {};
    ::free(entries_);
    entries_ = nullptr;
}

//--- FileSystem
//------------------------------------------
FileSystem::FileSystem()
    : capacity_(0)
    , size_(0)
    , systems_(nullptr)
{
}

FileSystem::~FileSystem()
{
    for(u32 i = 0; i < size_; ++i) {
        delete systems_[i];
    }
    capacity_ = 0;
    size_ = 0;
    ::free(systems_);
    systems_ = nullptr;
}

bool FileSystem::mountOS(const char* root)
{
    if(capacity_ <= size_) {
        expand();
    }
    assert(size_ < capacity_);
    systems_[size_] = new FileSystemOS(root);
    ++size_;
    return true;
}

void FileSystem::expand()
{
    u32 capacity = capacity_;
    while(capacity <= size_) {
        capacity += 4;
    }
    IFileSystem** systems = reinterpret_cast<IFileSystem**>(::malloc(sizeof(IFileSystem*) * capacity));
    if(nullptr == systems) {
        return;
    }
    ::memcpy(systems, systems_, sizeof(IFileSystem*) * capacity_);
    ::free(systems_);
    capacity_ = capacity;
    systems_ = systems;
}

u32 FileSystem::size() const
{
    return size_;
}

FileSytemType FileSystem::getType(u32 index) const
{
    assert(index < size_);
    assert(nullptr != systems_[index]);
    return systems_[index]->getType();
}

IFileHandle* FileSystem::openFile(const char* path)
{
    assert(nullptr != path);
    for(u32 i = size_; 0 < i; --i) {
        IFileHandle* file = systems_[i - 1]->openFile(path);
        if(nullptr != file) {
            return file;
        }
    }
    return nullptr;
}

void FileSystem::closeFile(IFileHandle* handle)
{
    assert(nullptr != handle);
}

IDirectoryHandle* FileSystem::openDirectory(const char* path)
{
    if(nullptr == path) {
        path = "";
    }
    for(u32 i = size_; 0 < i; --i) {
        IDirectoryHandle* directory = systems_[i - 1]->openDirectory(path);
        if(nullptr != directory) {
            return directory;
        }
    }
    return nullptr;
}

void FileSystem::closeDirectory(IDirectoryHandle* handle)
{
    assert(nullptr != handle);
}
} // namespace sfs
