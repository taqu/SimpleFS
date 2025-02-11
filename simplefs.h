#include <cstdint>
#include <type_traits>
#include <filesystem>

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

inline static constexpr u32 HashSeed = 0xAF08'73F2U;

enum class Type
{
    File = 0,
    Directory,
};

enum class Compression
{
    None = 0,
    LZ4,
};

struct Header
{
    u32 magic_;
    u32 entry_;
    u32 name_;
    u32 data_;
    u64 hash_;
};

struct File
{
    u64 size_;
    u64 offset_;
    u32 num_children_;
    u32 child_start_;
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
    void push_back(const T& x);
    void pop_back();

private:
    Array(const Array&) = delete;
    Array& operator=(const Array&) = delete;
    void expand();
    u32 capacity_;
    u32 size_;
    T* items_;
};

//--- Builder
//--------------------------------------------------------
class Builder
{
public:
    enum class Compression
    {
        Raw,
        LZ4,
    };

    struct Param
    {
        Compression compression_;
    };

    Builder();
    ~Builder();
    bool build(const char* root, const char* outfile);

private:
    void build_file(const std::filesystem::directory_entry& entry);
    void build_directory(const std::filesystem::directory_entry& entry);
    bool compress(const char* file);
    Array<File> files_;
    Array<char> names_;
    Array<std::filesystem::path> filepath_;
};

//--- IFileSystem
//-------------------------------------------------------------------
class IFileSystem
{
public:
    virtual bool open(const char* filepath) = 0;
    virtual void close() = 0;

protected:
    IFileSystem() {}
    virtual ~IFileSystem() {}
};
 
//--- IFile
//-------------------------------------------------------------------
class IFile
{
public:
    virtual bool open(const char* filepath) = 0;
    virtual void close() = 0;

protected:
    IFile(const IFile&) = delete;
    IFile& operator=(const IFile&) = delete;
    IFile() {}
    virtual ~IFile() {}
};

//--- PhyFile
//-------------------------------------------------------------------
class PhyFile : public IFile
{
public:
    virtual bool open(const char* filepath) override;
    virtual void close() override;

protected:
    PhyFile(const PhyFile&) = delete;
    PhyFile& operator=(const PhyFile&) = delete;
    PhyFile() {}
    virtual ~PhyFile() {}
};

//--- PhySF
//-------------------------------------------------------------------
class PhySF : public IFileSystem
{
public:
    PhySF();
    virtual ~PhySF();

    virtual bool open(const char* filepath) override;
    virtual void close() override;

private:
    std::filesystem::path root_;
};

} // namespace sfs
