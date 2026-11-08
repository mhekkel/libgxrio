// Copyright Maarten L. Hekkelman, Radboud University 2008-2013.
// SPDX-FileCopyrightText: 2026 Maarten L. Hekkelman
//
// SPDX-License-Identifier: BSL-1.0

#include "test-main.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

#include <gxrio.hpp>

#if HAVE_LibLZMA
unsigned char kXZData[] = {
	0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00, 0x00, 0x04, 0xe6, 0xd6, 0xb4, 0x46, 0x02, 0x00, 0x21, 0x01,
	0x16, 0x00, 0x00, 0x00, 0x74, 0x2f, 0xe5, 0xa3, 0x01, 0x00, 0x0d, 0x48, 0x65, 0x6c, 0x6c, 0x6f,
	0x2c, 0x20, 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x21, 0x0a, 0x00, 0x00, 0x00, 0xc3, 0xad, 0x94, 0xb3,
	0x17, 0xf6, 0x0c, 0xca, 0x00, 0x01, 0x26, 0x0e, 0x08, 0x1b, 0xe0, 0x04, 0x1f, 0xb6, 0xf3, 0x7d,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x04, 0x59, 0x5a
};
#endif

unsigned char kGZippedData[] = {
	0x1f, 0x8b, 0x08, 0x08, 0x61, 0xb2, 0xf0, 0x62, 0x00, 0x03, 0x74, 0x65, 0x73, 0x74, 0x2e, 0x74,
	0x78, 0x74, 0x00, 0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0xd7, 0x51, 0x28, 0xcf, 0x2f, 0xca, 0x49, 0x51,
	0xe4, 0x02, 0x00, 0x18, 0xa7, 0x55, 0x7b, 0x0e, 0x00, 0x00, 0x00
};

int main(int argc, char *argv[])
{
	return gxrio_test_main(argc, argv);
}

// --------------------------------------------------------------------

TEST_CASE("reading compressed and uncompressed files", "[t_1]")
{
	for (fs::path f : {
			 gTestDir / "hello.txt.gz",
#if HAVE_LibLZMA
			 gTestDir / "hello.txt.xz",
#endif
			 gTestDir / "hello.txt" })
	{
		std::filebuf fb;
		fb.open(f, std::ios::in | std::ios::binary);
		CHECK(fb.is_open());

		gxrio::istream is(&fb);

		std::string line;

		getline(is, line);

		CHECK(line == "Hello, world!");
	}
}

// --------------------------------------------------------------------

TEST_CASE("reading compressed and uncompressed files via ifstream", "[t_2]")
{
	for (fs::path f : {
			 gTestDir / "hello.txt.gz",
#if HAVE_LibLZMA
			 gTestDir / "hello.txt.xz",
#endif
			 gTestDir / "hello.txt" })
	{
		gxrio::ifstream is(f);
		CHECK(is.is_open());

		std::string line;

		getline(is, line);

		CHECK(line == "Hello, world!");
	}
}

// --------------------------------------------------------------------

TEST_CASE("move constructing an ifstream", "[t_3]")
{
	for (fs::path f : {
			 gTestDir / "hello.txt.gz",
#if HAVE_LibLZMA
			 gTestDir / "hello.txt.xz",
#endif
			 gTestDir / "hello.txt" })
	{
		gxrio::ifstream is1(f);
		CHECK(is1.is_open());

		gxrio::ifstream is2(std::move(is1));
		CHECK(is2.is_open());

		std::string line;

		getline(is2, line);

		CHECK(line == "Hello, world!");
	}
}

// --------------------------------------------------------------------

TEST_CASE("move assigning an ifstream", "[t_4]")
{
	for (fs::path f : {
			 gTestDir / "hello.txt.gz",
#if HAVE_LibLZMA
			 gTestDir / "hello.txt.xz",
#endif
			 gTestDir / "hello.txt" })
	{
		gxrio::ifstream is1(f);
		CHECK(is1.is_open());

		gxrio::ifstream is2;

		is2 = std::move(is1);
		CHECK(is2.is_open());

		std::string line;

		getline(is2, line);

		CHECK(line == "Hello, world!");
	}
}

// --------------------------------------------------------------------

TEST_CASE("writing and reading back compressed files", "[t_5]")
{
	std::filesystem::create_directories(std::filesystem::temp_directory_path() / "gxrio-unit-test");

	for (fs::path f : {
			 std::filesystem::temp_directory_path() / "gxrio-unit-test" / "hello.txt.gz",
#if HAVE_LibLZMA
			 std::filesystem::temp_directory_path() / "gxrio-unit-test" / "hello.txt.xz",
#endif
			 std::filesystem::temp_directory_path() / "gxrio-unit-test" / "hello.txt" })
	{
		gxrio::ofstream out(f);
		CHECK(out.is_open());

		out << "Hello, world!" << std::endl;
		out.close();

		gxrio::ifstream in(f);
		CHECK(in.is_open());

		std::string line;

		getline(in, line);

		CHECK(line == "Hello, world!");
	}
}

// --------------------------------------------------------------------

TEST_CASE("move constructing an ofstream", "[t_6]")
{
	std::filesystem::create_directories(std::filesystem::temp_directory_path() / "gxrio-unit-test");

	for (fs::path f : {
			 std::filesystem::temp_directory_path() / "gxrio-unit-test" / "hello.txt.gz",
#if HAVE_LibLZMA
			 std::filesystem::temp_directory_path() / "gxrio-unit-test" / "hello.txt.xz",
#endif
			 std::filesystem::temp_directory_path() / "gxrio-unit-test" / "hello.txt" })
	{
		gxrio::ofstream out_1(f);
		CHECK(out_1.is_open());

		gxrio::ofstream out_2(std::move(out_1));
		CHECK(out_2.is_open());

		out_2 << "Hello, world!" << std::endl;
		out_2.close();

		gxrio::ifstream in(f);

		std::string line;

		getline(in, line);

		CHECK(line == "Hello, world!");
	}
}

// --------------------------------------------------------------------

TEST_CASE("move assigning an ofstream", "[t_7]")
{
	std::filesystem::create_directories(std::filesystem::temp_directory_path() / "gxrio-unit-test");

	for (fs::path f : {
			 std::filesystem::temp_directory_path() / "gxrio-unit-test" / "hello.txt.gz",
#if HAVE_LibLZMA
			 std::filesystem::temp_directory_path() / "gxrio-unit-test" / "hello.txt.xz",
#endif
			 std::filesystem::temp_directory_path() / "gxrio-unit-test" / "hello.txt" })
	{
		gxrio::ofstream out_1(f);
		CHECK(out_1.is_open());

		gxrio::ofstream out_2;
		out_2 = std::move(out_1);
		CHECK(out_2.is_open());

		out_2 << "Hello, world!" << std::endl;
		out_2.close();

		gxrio::ifstream in(f);

		std::string line;

		getline(in, line);

		CHECK(line == "Hello, world!");
	}
}

// --------------------------------------------------------------------

TEST_CASE("sniffing the format of a stream", "[t_8]")
{
	for (const auto &[text, length] : std::vector<std::tuple<const char *, size_t>>{
			 { "Hello, world!", 13 },
			 { (const char *)kGZippedData, sizeof(kGZippedData) },
#if HAVE_LibLZMA
			 { (const char *)kXZData, sizeof(kXZData) },
#endif
			 { "\xfd\x37Hello, world!", 15 },
			 { "\x1fHello, world!", 14 } })
	{
		struct membuf : public std::streambuf
		{
			membuf(char *text, size_t length)
			{
				this->setg(text, text, text + length);
			}
		} buffer(const_cast<char *>(text), length);

		gxrio::istream in(&buffer);

		std::string line;
		std::getline(in, line);

		CHECK(line.length() >= 13);
		if (line.length() < 13)
			continue;

		CHECK(line.compare(line.length() - 13, 13, "Hello, world!") == 0);
	}
}

// --------------------------------------------------------------------

TEST_CASE("swapping ifstream objects", "[t_9]")
{
	// swapping ifstream objects transfers the (de)compressor with the filebuf
	gxrio::ifstream in_1, in_2;
#if HAVE_LibLZMA
	in_1.open(gTestDir / "hello.txt.xz");
#else
	in_1.open(gTestDir / "hello.txt.gz");
#endif

	in_1.swap(in_2);

	std::string line;
	getline(in_2, line);

	CHECK(line == "Hello, world!");

	std::filesystem::create_directories(std::filesystem::temp_directory_path() / "gxrio-unit-test");

	fs::path out_file = std::filesystem::temp_directory_path() / "gxrio-unit-test" / "swap-out.txt.gz";

	gxrio::ofstream out_1, out_2(out_file);
	out_1.swap(out_2);

	out_1 << "Hello, world!" << std::endl;
	out_1.close();

	gxrio::ifstream in(out_file);
	getline(in, line);

	CHECK(line == "Hello, world!");
}

// --------------------------------------------------------------------

TEST_CASE("reopening with a different extension", "[t_10]")
{
	// reopening with a different extension must drop any stale decompressor
	gxrio::ifstream in;
	in.open(gTestDir / "hello.txt.gz");

	std::string line;
	getline(in, line);
	CHECK(line == "Hello, world!");

	in.open(gTestDir / "hello.txt");
	getline(in, line);
	CHECK(line == "Hello, world!");

	std::filesystem::create_directories(std::filesystem::temp_directory_path() / "gxrio-unit-test");

	fs::path out_dir = std::filesystem::temp_directory_path() / "gxrio-unit-test";

	gxrio::ofstream out;
	out.open(out_dir / "reopen.txt.gz");
	out << "Hello, world!" << std::endl;
	out.close();

	out.open(out_dir / "reopen.txt");
	out << "Hello, world!" << std::endl;
	out.close();

	gxrio::ifstream in_2(out_dir / "reopen.txt");
	getline(in_2, line);
	CHECK(line == "Hello, world!");
}

// --------------------------------------------------------------------

TEST_CASE("moving an istream wrapping an uncompressed streambuf", "[t_11]")
{
	// moving a gxrio::istream that wraps an uncompressed streambuf
	std::filebuf fb;
	fb.open(gTestDir / "hello.txt", std::ios::in | std::ios::binary);

	gxrio::istream is(&fb);

	gxrio::istream is_2(std::move(is));

	std::string line;
	getline(is_2, line);

	CHECK(line == "Hello, world!");
}

// --------------------------------------------------------------------

TEST_CASE("truncated compressed input reaches eof", "[t_12]")
{
	// truncated compressed input reads what is available and hits eof without hanging
	for (fs::path f : {
#if HAVE_LibLZMA
			 gTestDir / "hello.txt.xz",
#endif
			 gTestDir / "hello.txt.gz" })
	{
		std::ifstream data(f, std::ios::binary);
		std::vector<char> contents(
			(std::istreambuf_iterator<char>(data)),
			std::istreambuf_iterator<char>());

		contents.resize(contents.size() - 4);

		struct membuf : public std::streambuf
		{
			membuf(std::vector<char> &data)
			{
				this->setg(data.data(), data.data(), data.data() + data.size());
			}
		} buffer(contents);

		gxrio::istream in(&buffer);

		std::string line;
		while (getline(in, line))
			CHECK(line.length() >= 13);

		CHECK(in.eof());
	}
}

// --------------------------------------------------------------------

TEST_CASE("corrupt input sets failbit", "[t_13]")
{
	// corrupt input must not crash and must set failbit
	struct membuf : public std::streambuf
	{
		membuf(std::array<char, 64> &data)
		{
			this->setg(data.data(), data.data(), data.data() + data.size());
		}
	};

	std::array<char, 64> data;
	data.fill('x');
	membuf buffer(data);

	gxrio::istream in(&buffer);

	std::string line;
	while (getline(in, line))
		;

	CHECK((in.fail() or in.eof()));
}

// --------------------------------------------------------------------

TEST_CASE("setting the compression level", "[t_14]")
{
	std::filesystem::create_directories(std::filesystem::temp_directory_path() / "gxrio-unit-test");

	for (fs::path f : {
			 std::filesystem::temp_directory_path() / "gxrio-unit-test" / "level.txt.gz",
			 std::filesystem::temp_directory_path() / "gxrio-unit-test" / "level.txt.xz" })
	{
		gxrio::ofstream out(f);
		out.set_compression_level(1);

		out << "Hello, world!" << std::endl;
		out.close();

		gxrio::ifstream in(f);

		std::string line;
		getline(in, line);

		CHECK(line == "Hello, world!");
	}
}

// --------------------------------------------------------------------

TEST_CASE("reading concatenated gzip members", "[t_21]")
{
	gxrio::ifstream file(gTestDir / "aap.gz");

	REQUIRE(file.is_open());

	std::string line;

	CHECK(getline(file, line));
	CHECK(line == "aap");

	CHECK(getline(file, line));
	CHECK(line == "noot");

	CHECK(getline(file, line));
	CHECK(line == "mies");

	CHECK(not getline(file, line));
	CHECK(file.eof());
}
