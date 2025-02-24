#include <cstdint>
#include <type_traits>
#include <string_view>
#include <filesystem>

struct XXH64_state_s;

namespace std
{
namespace filesystem
{
    class directory_entry;
}
} // namespace std

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

inline static constexpr u32 Magic = 0x70616330UL;
inline static constexpr u64 HashSeed = 0x3AE8'2BF0'AF08'73F2ULL;

enum class Type
{
    File = 0,
    Directory,
};

enum class Compression : u8
{
    Raw = 0,
    LZ4,
};

struct Header
{
    u32 magic_;
    u32 num_entries_;
    u32 name_;
    u32 data_;
    u64 hash_;
};

struct SizeOffset
{
    u64 offset_;
    u32 original_size_;
    u32 compressed_size_;
};

struct Children
{
    u64 num_children_;
    u64 child_start_;
};

struct File
{
    union
    {
        SizeOffset size_offset_;
        Children children_;
    };
    u32 name_offset_;
    u16 name_length_;
    u8 type_;
    u8 compression_;
};

//--- Array
//--------------------------------------------------------
template<class T>
class Array
{
public:
    Array();
    ~Array();
    u32 capacity() const;
    u32 size() const;
    void clear();
    const T& operator[](u32 index) const;
    T& operator[](u32 index);
    void resize(u32 newSize);
    void push_back(const T& x);
    void pop_back();

    void insert(u32 index, const T& x);
    void remove_at(u32 index);

private:
    Array(const Array&) = delete;
    Array& operator=(const Array&) = delete;
    void expand(u32 newCapacity);
    u32 capacity_;
    u32 size_;
    T* items_;
};

//--- BufferPool
//--------------------------------------------------------
class BufferPool
{
public:
    BufferPool();
    ~BufferPool();
    u32 size() const;
    void clear();
    void* get(u32 size);
private:
    BufferPool(const BufferPool&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;
    u32 size_;
    void* buffer_;
};

//--- Builder
//--------------------------------------------------------
class Builder
{
public:
    struct Param
    {
        Compression compression_ = Compression::LZ4;
        u32 minimum_size_to_compress_ = 512;
    };

    Builder();
    ~Builder();
    bool build(const char* root, const char* outfile, const Param& param);

private:
    void add_file(u32 index, const std::filesystem::directory_entry& entry);
    void add_directory(u32 index, const std::filesystem::directory_entry& entry, const std::u8string& name);
    bool compress(const char* file, const Param& param);
    Array<File> files_;
    Array<char> names_;
    Array<std::filesystem::path> filepath_;
};

//--- IFileSystem
//-------------------------------------------------------------------
class IFile;

class IFileSystem
{
public:
    virtual bool open(const char* filepath) = 0;
    virtual void close() = 0;
    virtual IFile* open_file(const char* filepath) = 0;
    virtual bool close_file(IFile* file) = 0;

protected:
    IFileSystem(const IFileSystem&) = delete;
    IFileSystem& operator=(const IFileSystem&) = delete;
    friend class VFS;
    IFileSystem() {}
    virtual ~IFileSystem() {}
};
 
//--- DirectoryIterator
//-------------------------------------------------------------------
class IFile;
class DirectoryIterator
{
public:
    DirectoryIterator(IFile* parent, IFile* file, u32 index);
    ~DirectoryIterator();
    DirectoryIterator(DirectoryIterator&& other);
    DirectoryIterator& operator=(DirectoryIterator&& other);
    operator bool() const;
    IFile& operator*();
    IFile* operator->();
    void operator++();
private:
    DirectoryIterator(const DirectoryIterator&) = delete;
    DirectoryIterator& operator=(const DirectoryIterator&) = delete;
    friend class IFile;
    friend class PhyFile;
    friend class PacFile;

    IFile* parent_;
    IFile* file_;
    u32 index_;
};

//--- IFile
//-------------------------------------------------------------------
class IFile
{
public:
    virtual void close() = 0;
    virtual u32 original_size() const = 0;
    virtual u32 compressed_size() const = 0;
    virtual bool is_file() const = 0;
    virtual u32 num_children() const = 0;
    virtual DirectoryIterator begin() = 0;
    virtual void next(DirectoryIterator& itr) = 0;
    virtual std::u8string_view filename() const = 0;
    virtual u32 read(void* dst) = 0;
protected:
    IFile(const IFile&) = delete;
    IFile& operator=(const IFile&) = delete;
    friend class DirectoryIterator;
    friend class VFS;
    IFile() {}
    virtual ~IFile() {}
};

//--- PhyFile
//-------------------------------------------------------------------
class PhyFS;
class PhyFile : public IFile
{
public:
    virtual ~PhyFile();
    virtual void close() override;
    virtual u32 original_size() const override;
    virtual u32 compressed_size() const override;
    virtual bool is_file() const override;
    virtual u32 num_children() const override;
    virtual DirectoryIterator begin() override;
    virtual void next(DirectoryIterator& itr) override;
    virtual std::u8string_view filename() const override;
    virtual u32 read(void* dst) override;
protected:
    PhyFile(const PhyFile&) = delete;
    PhyFile& operator=(const PhyFile&) = delete;
    friend class PhyFS;
    PhyFile();

    void initialize(PhyFS* fs, const std::filesystem::directory_entry& entry);
    PhyFS* fs_;
    bool is_file_;
    u32 size_;
    std::u8string filepath_;
    std::u8string filename_;
    Array<std::filesystem::directory_entry> children_;
};

//--- PhyFS
//-------------------------------------------------------------------
class PhyFS : public IFileSystem
{
public:
    inline static constexpr u32 MaxPath = 512;
    inline static constexpr u32 PageSizeShift = 16;
    inline static constexpr u32 PageSize = 1ULL<<PageSizeShift;
    PhyFS();
    virtual ~PhyFS();

    virtual bool open(const char* filepath) override;
    virtual void close() override;

    virtual IFile* open_file(const char* filepath) override;
    virtual bool close_file(IFile* file) override;
private:
    PhyFS(const PhyFS&) = delete;
    PhyFS& operator=(const PhyFS&) = delete;
    friend class PhyFile;
    struct Page
    {
        Page* next_;
    };

    union Entry
    {
        PhyFile file_;
        Entry* next_;
    };

    IFile* open_file(const std::filesystem::directory_entry& root, const char* begin, const char* end);
    PhyFile* pop();
    void push(Entry* f);
    void alloc_page();

    std::filesystem::directory_entry root_;
    u32 opend_;
    Page* pages_;
    Entry* entries_;
};

//--- PacFile
//-------------------------------------------------------------------
class PacFS;
class PacFile : public IFile
{
public:
    virtual ~PacFile();
    virtual void close() override;
    virtual u32 original_size() const override;
    virtual u32 compressed_size() const override;
    virtual bool is_file() const override;
    virtual u32 num_children() const override;
    virtual DirectoryIterator begin() override;
    virtual void next(DirectoryIterator& itr) override;
    virtual std::u8string_view filename() const override;
    virtual u32 read(void* dst) override;
protected:
    PacFile(const PacFile&) = delete;
    PacFile& operator=(const PacFile&) = delete;
    friend class PacFS;
    PacFile();

    void initialize(PacFS* fs, const File* file);
    PacFS* fs_;
    const File* file_;
};

//--- PacFS
//-------------------------------------------------------------------
class PacFS : public IFileSystem
{
public:
    inline static constexpr u32 MaxPath = 512;
    inline static constexpr u32 PageSizeShift = 16;
    inline static constexpr u32 PageSize = 1ULL<<PageSizeShift;
    PacFS();
    virtual ~PacFS();

    virtual bool open(const char* filepath) override;
    virtual void close() override;

    virtual IFile* open_file(const char* filepath) override;
    virtual bool close_file(IFile* file) override;
private:
    PacFS(const PacFS&) = delete;
    PacFS& operator=(const PacFS&) = delete;
    friend class PacFile;
    struct Page
    {
        Page* next_;
    };

    union Entry
    {
        PacFile file_;
        Entry* next_;
    };

    IFile* open_file(u32 root, const char* begin, const char* end);
    PacFile* pop();
    void push(Entry* f);
    void alloc_page();
    void* get_buffer(u32 size);

    FILE* file_;
    Header header_;
    File* files_;
    const char* names_;
    u32 opend_;
    Page* pages_;
    Entry* entries_;
    BufferPool buffer_pool_;
};

//--- VFS
//-------------------------------------------------------------------
class VFS
{
public:
    VFS();
    ~VFS();

    bool add_phyfs(const char* root);
    bool add_pacfs(const char* file);

    IFile* open_file(const char* filepath);
    bool close_file(IFile* file);
private:
    VFS(const VFS&) = delete;
    VFS& operator=(const VFS&) = delete;
    Array<IFileSystem*> fs_;
};

} // namespace sfs
