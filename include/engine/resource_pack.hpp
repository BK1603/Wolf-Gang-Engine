#ifndef ENGINE_RESOURCE_PACK_HPP
#define ENGINE_RESOURCE_PACK_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include <set>
#include <engine/utility.hpp>

namespace engine {

// A path somewhat similar to boost path but with more features
// related to manipulating the path.
class encoded_path
{
public:
	encoded_path() {}
	encoded_path(const char* pString);
	encoded_path(const std::string& pString);

	bool parse(const std::string& pString, const std::set<char>& pDelimitors = {'\\', '/'});

	// Check if first part of this path is the same
	bool in_directory(const encoded_path& pPath) const;

	// Snip the first part of this path (if it can)
	bool snip_path(const encoded_path& pPath);

	encoded_path subpath(size_t pOffset, size_t pCount = 0) const;

	std::string string() const;
	std::string string(char pSeperator) const;
	std::string stem() const;
	std::string extension() const;

	bool empty() const;

	void clear();

	bool is_same(const encoded_path& pPath) const;

	void append(const encoded_path& pRight);

	encoded_path parent() const;

	std::string filename() const;
	bool pop_filename();

	bool remove_extension();

	std::string get_section(size_t pAt) const;
	size_t get_sub_length() const;

	encoded_path& operator=(const std::string& pString);
	encoded_path operator/(const encoded_path& pRight) const;
	encoded_path& operator/=(const encoded_path& pRight);

	int compare(const encoded_path& pCmp) const;
	bool operator<(const encoded_path& pRight) const;
	bool operator==(const encoded_path& pRight) const;

private:
	void simplify();

	std::vector<std::string> mHierarchy;
};

class pack_header
{
public:

	/*
	Structure of the header

	[uint64_t] File count
	file
	[uint16_t] Path size (Is a filepath that is 65536 characters useful?)
	[char] Character of path
	...
	[uint64_t] Position
	[uint64_t] Size
	...
	*/

	static_assert(sizeof(uint16_t) == 2, "uint16_t is not 2 bytes");
	static_assert(sizeof(uint64_t) == 8, "uint64_t is not 8 bytes");

	pack_header();

	struct file_info
	{
		encoded_path path;
		uint64_t position;
		uint64_t size;
	};

	void add_file(file_info pFile);

	bool generate(std::ostream& pStream) const;

	bool parse(std::istream& pStream);

	util::optional<file_info> get_file(const encoded_path& pPath) const;
	std::vector<encoded_path> recursive_directory(const encoded_path& pPath) const;

	uint64_t get_header_size() const;

private:
	uint64_t mHeader_size;
	std::vector<file_info> mFiles;
};

// Reads the header of a pack file and generates streams for the contained files
class pack_stream_factory
{
public:
	bool open(const encoded_path& pPath);
	std::vector<char> read_all(const encoded_path& pPath) const;
	std::vector<encoded_path> recursive_directory(const encoded_path& pPath) const;
private:
	encoded_path mPath;
	pack_header mHeader;
	friend class pack_stream;
};

class pack_stream
{
public:
	pack_stream();
	pack_stream(const pack_stream_factory& pPack);
	pack_stream(const pack_stream& pCopy);
	~pack_stream();

	void set_pack(const pack_stream_factory& pPack);

	bool open(const encoded_path & pPath);
	void close();

	std::vector<char> read(uint64_t pCount);
	int64_t read(char* pData, uint64_t pCount);
	bool read(std::vector<char>& pData, uint64_t pCount);

	std::vector<char> read_all();

	bool seek(uint64_t pPosition);
	uint64_t tell();

	bool is_valid();

	uint64_t size() const;

	pack_stream& operator=(const pack_stream& pRight);

	operator bool() { return is_valid(); }

	friend class pack_stream_factory;

private:
	const pack_stream_factory * mPack;
	pack_header::file_info mFile_info;
	std::ifstream mStream;
};

bool create_resource_pack(const std::string& pSrc_directory, const std::string& pDest);

}
#endif // ENGINE_RESOURCE_PACK_HPP