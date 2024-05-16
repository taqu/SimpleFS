#ifndef INC_SIMPLEFS_H_
#define INC_SIMPLEFS_H_
/**
@author t-sakai
@file SimpleFS.h
*/
#include <cstdint>
#ifdef _WIN32
#    include <Windows.h>
#else
#    include <cstdio>
#    include <dirent.h>
#    include <sys/types.h>
#endif

namespace sfs
{
using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using f32 = float;
using f64 = double;

enum class FileSytemType
{
    OS,
    Pac,
};

enum class FileTypeFlag
{
    None = 0,
    File = (0x01U << 0),
    Compressed = (0x01U << 1),
    Directory = (0x01U << 2),
};

struct FileStatus
{
    u64 access_;
    u64 modified_;
    u64 create_;
    u64 size_;
    u64 compressedSize_;
    u32 type_;
};

class IFileSystem;
class FileSystemOS;
class FileSystemPac;
class FileSystem;

//--- pac structures
//-------------------------------------
struct PacHeader
{
    u64 size_;
    u32 magic_;
    u32 entries_; //!< number of entries
    u64 nameOffset_;
    u64 dataOffset_;
};

struct PacEntry
{
    u32 uncompressedSize_;
    u32 compressedSize_;
    u32 typeNumChildren_;
    u32 childStart_;
    u64 nameStart_;
    u64 dataStart_;

    inline void setTypeNumChildren(u32 type, u32 numChildren)
    {
        typeNumChildren_ = (type<<24U) | (numChildren|0xFFFFFFUL);
    }

    inline u32 getType() const
    {
        return typeNumChildren_>>24U;
    }

    inline u32 getNumChildren() const
    {
        return typeNumChildren_&0xFFFFFFUL;
    }
};

//--- FileIterator
//-------------------------------------
/**
 * @brief File iterator
 */
class FileIterator
{
public:
    FileIterator(const FileIterator& other);
    FileIterator(FileIterator&& other);
    ~FileIterator();
    FileIterator& operator=(const FileIterator&);
    FileIterator& operator=(FileIterator&& other);

    void close();
    bool next();
    FileTypeFlag type() const;
    const char* path() const;

private:
    friend class DirectoryHandleOS;
    friend class DirectoryHandlePac;

#if _WIN32
    FileIterator(const char* path);
    const char* path_;
    HANDLE handle_;
    WIN32_FIND_DATAA data_;
#else
    FileIterator(DIR* handle, struct dirent* current);
    DIR* handle_;
    struct dirent* current_;
#endif
    FileIterator(FileSystemPac* fileSystem, PacEntry* entry);
    const FileSystemPac* fileSystem_;
    const PacEntry* entry_;
    u32 count_;
};

/**
 * @brief File interface
 */
class IFileHandle
{
public:
    virtual FileStatus status() const = 0;
    virtual bool seek(s64 offset, s32 dir) = 0;
    virtual s64 tell() const = 0;
    virtual s64 read(s64 size, void* ptr) = 0;
    virtual s64 write(s64 size, const void* ptr) = 0;
protected:
    IFileHandle();
    virtual ~IFileHandle();

private:
    IFileHandle(const IFileHandle&) = delete;
    IFileHandle& operator=(const IFileHandle&) = delete;
};

/**
 * @brief Directory interface
 */
class IDirectoryHandle
{
public:
    virtual FileIterator begin() = 0;

protected:
    IDirectoryHandle();
    virtual ~IDirectoryHandle();

private:
    IDirectoryHandle(const IDirectoryHandle&) = delete;
    IDirectoryHandle& operator=(const IDirectoryHandle&) = delete;
};

/**
 * @brief FileSystem interface
 */
class IFileSystem
{
public:
    virtual ~IFileSystem();
    virtual FileSytemType getType() = 0;
    virtual IFileHandle* openFile(const char* path) = 0;
    virtual void closeFile(IFileHandle* file) = 0;
    virtual IDirectoryHandle* openDirectory(const char* path) = 0;
    virtual void closeDirectory(IDirectoryHandle* directory) = 0;

protected:
    IFileSystem();

private:
    IFileSystem(const IFileSystem&) = delete;
    IFileSystem& operator=(const IFileSystem&) = delete;
};

//--- FileHandleOS
//-------------------------------------
class FileHandleOS: public IFileHandle
{
public:
    virtual FileStatus status() const override;
    virtual bool seek(s64 offset, s32 dir) override;
    virtual s64 tell() const override;
    virtual s64 read(s64 size, void* ptr) override;
    virtual s64 write(s64 size, const void* ptr) override;
private:
    FileHandleOS(const FileHandleOS&) = delete;
    FileHandleOS& operator=(const FileHandleOS&) = delete;

    friend class FileSystemOS;

    virtual ~FileHandleOS();

#ifdef _WIN32
    FileHandleOS(HANDLE file);
    HANDLE file_;
#else
    FileHandleOS(FILE* file);
    FILE* file_;
#endif
};

//--- DirectoryHandleOS
//-------------------------------------
class DirectoryHandleOS: public IDirectoryHandle
{
public:
    virtual FileIterator begin() override;
private:
    DirectoryHandleOS(const DirectoryHandleOS&) = delete;
    DirectoryHandleOS& operator=(const DirectoryHandleOS&) = delete;

    friend class FileSystemOS;

    DirectoryHandleOS(char* path);
    virtual ~DirectoryHandleOS();

    char* path_;
};

//--- FileSystemOS
//-------------------------------------
class FileSystemOS: public IFileSystem
{
public:
    virtual ~FileSystemOS();
    virtual FileSytemType getType() override;
    virtual IFileHandle* openFile(const char* path) override;
    virtual void closeFile(IFileHandle* handle) override;
    virtual IDirectoryHandle* openDirectory(const char* path) override;
    virtual void closeDirectory(IDirectoryHandle* handle) override;

private:
    FileSystemOS(const FileSystemOS&) = delete;
    FileSystemOS& operator=(const FileSystemOS&) = delete;

    friend class FileSystem;

    inline static constexpr u32 PageSize = 4 * 1024;

    class StringBuffer
    {
    public:
        StringBuffer();
        ~StringBuffer();
        void reserve(u32 capacity);
        const char& operator[](u32 index) const;
        char& operator[](u32 index);

    private:
        StringBuffer(const StringBuffer&) = delete;
        StringBuffer& operator=(const StringBuffer&) = delete;
        u32 capacity_;
        char* buffer_;
    };
    struct Page
    {
        Page* next_;
    };

    template<class T>
    constexpr const T& maximum(const T& a, const T& b)
    {
        return a < b ? b : a;
    }

    constexpr size_t getSize()
    {
        return maximum(maximum(sizeof(FileHandleOS), sizeof(DirectoryHandleOS)), sizeof(Page));
    }

    FileSystemOS(const char* root);

    void* pop();
    void push(void* handle);

    void expand();

    StringBuffer buffer_;
    size_t length_;
    char* root_;
    Page* page_;
    Page* top_;
};

//--- FileHandlePac
//-------------------------------------
class FileHandlePac: public IFileHandle
{
public:
    virtual FileStatus status() const override;
    virtual bool seek(s64 offset, s32 dir) override;
    virtual s64 tell() const override;
    virtual s64 read(s64 size, void* ptr) override;
    virtual s64 write(s64 size, const void* ptr) override;
private:
    FileHandlePac(const FileHandlePac&) = delete;
    FileHandlePac& operator=(const FileHandlePac&) = delete;

    friend class FileSystemPac;

    FileHandlePac(const PacEntry* entry, const void* data);
    virtual ~FileHandlePac();

    const PacEntry* entry_;
    s64 offset_;
    const void* data_;
};

//--- DirectoryHandlePac
//-------------------------------------
class DirectoryHandlePac: public IDirectoryHandle
{
public:
    virtual FileIterator begin() override;

private:
    DirectoryHandlePac(const DirectoryHandlePac&) = delete;
    DirectoryHandlePac& operator=(const DirectoryHandlePac&) = delete;

    friend class FileSystemPac;

    DirectoryHandlePac(FileSystemPac* fileSystem, const PacEntry* entry);
    virtual ~DirectoryHandlePac();
    FileSystemPac* fileSystem_;
    const PacEntry* entry_;
};

//--- FileSystemPac
//-------------------------------------
class FileSystemPac: public IFileSystem
{
public:
    virtual ~FileSystemPac();
    virtual FileSytemType getType() override;
    virtual IFileHandle* openFile(const char* path) override;
    virtual void closeFile(IFileHandle* handle) override;
    virtual IDirectoryHandle* openDirectory(const char* path) override;
    virtual void closeDirectory(IDirectoryHandle* handle) override;

private:
    FileSystemPac(const FileSystemPac&) = delete;
    FileSystemPac& operator=(const FileSystemPac&) = delete;

    friend class FileSystem;
    friend class FileHandlePac;
    friend class DirectoryHandlePac;
    friend class FileIterator;

    FileSystemPac();
    bool open(const char* path);

#ifdef _WIN32
    HANDLE file_;
#else
    FILE* file_;
#endif
    PacHeader header_;
    PacEntry* entries_;
};

//--- FileSystem
//------------------------------------------
class FileSystem
{
public:
    FileSystem();
    ~FileSystem();
    bool mountOS(const char* root);

    u32 size() const;
    FileSytemType getType(u32 index) const;
    IFileHandle* openFile(const char* path);
    void closeFile(IFileHandle* handle);
    IDirectoryHandle* openDirectory(const char* path);
    void closeDirectory(IDirectoryHandle* handle);

private:
    inline static constexpr u32 MaxPath = 2048;
    FileSystem(const FileSystem&) = delete;
    FileSystem& operator=(const FileSystem&) = delete;
    void expand();
    u32 capacity_;
    u32 size_;
    IFileSystem** systems_;
};
} // namespace sfs
#endif // INC_SIMPLEFS_H_
