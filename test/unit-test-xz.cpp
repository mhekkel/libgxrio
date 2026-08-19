// Copyright Maarten L. Hekkelman, Radboud University 2008-2013.
// SPDX-FileCopyrightText: 2026 Maarten L. Hekkelman
//
// SPDX-License-Identifier: BSL-1.0

#include "test-main.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <gxrio.hpp>

unsigned char kXZData[] = {
	0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00, 0x00, 0x04, 0xe6, 0xd6, 0xb4, 0x46, 0x02, 0x00, 0x21, 0x01,
	0x16, 0x00, 0x00, 0x00, 0x74, 0x2f, 0xe5, 0xa3, 0x01, 0x00, 0x0d, 0x48, 0x65, 0x6c, 0x6c, 0x6f,
	0x2c, 0x20, 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x21, 0x0a, 0x00, 0x00, 0x00, 0xc3, 0xad, 0x94, 0xb3,
	0x17, 0xf6, 0x0c, 0xca, 0x00, 0x01, 0x26, 0x0e, 0x08, 0x1b, 0xe0, 0x04, 0x1f, 0xb6, 0xf3, 0x7d,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x04, 0x59, 0x5a
};

static_assert(sizeof(kXZData) == 72, "That buffer should be 72 bytes in length");

int main(int argc, char *argv[])
{
	return gxrio_test_main(argc, argv);
}

// --------------------------------------------------------------------

TEST_CASE("decompress an in-memory xz buffer", "[t_c]")
{
	struct membuf : public std::streambuf
	{
		membuf(char *text, size_t length)
		{
			this->setg(text, text, text + length);
		}
	} buffer(reinterpret_cast<char *>(kXZData), sizeof(kXZData));

	gxrio::istream in(&buffer);

	std::string line;
	std::getline(in, line);

	CHECK(line == "Hello, world!");
}

// --------------------------------------------------------------------

TEST_CASE("decompress an xz file", "[t_1]")
{
	fs::path f = gTestDir / "hello.txt.xz";

	gxrio::ifstream in(f, std::ios::in | std::ios::binary);

	std::string line;
	std::getline(in, line);

	CHECK(line == "Hello, world!");
}

// --------------------------------------------------------------------

TEST_CASE("decompress 100 lines", "[t_100]")
{
	fs::path f = gTestDir / "hello-100.txt.xz";

	gxrio::ifstream in(f, std::ios::in | std::ios::binary);

	std::string line;

	int n = 0;
	while (std::getline(in, line))
	{
		CHECK(line == "Hello, world!");
		++n;
	}

	CHECK(n == 100);
}

// --------------------------------------------------------------------

TEST_CASE("decompress 1000 lines", "[t_1000]")
{
	fs::path f = gTestDir / "hello-1000.txt.xz";

	gxrio::ifstream in(f, std::ios::in | std::ios::binary);

	std::string line;

	int n = 0;
	while (std::getline(in, line))
	{
		std::string test = "Hello, world! - this is line " + std::to_string(n);

		CHECK(line == test);
		++n;
	}

	CHECK(n == 1000);
}

// --------------------------------------------------------------------

TEST_CASE("move an istream", "[t_copy_1]")
{
	struct membuf : public std::streambuf
	{
		membuf(char *text, size_t length)
		{
			this->setg(text, text, text + length);
		}
	} buffer(reinterpret_cast<char *>(kXZData), sizeof(kXZData));

	gxrio::istream in(&buffer);

	gxrio::istream in2(std::move(in));

	std::string line;
	std::getline(in2, line);

	CHECK(line == "Hello, world!");
}

// --------------------------------------------------------------------

TEST_CASE("move an ifstream while reading", "[t_copy_2]")
{
	fs::path f = gTestDir / "hello-1000.txt.xz";

	gxrio::ifstream in_1(f, std::ios::in | std::ios::binary);

	std::string line;

	std::getline(in_1, line);
	CHECK(line == "Hello, world! - this is line 0");

	gxrio::ifstream in_2(std::move(in_1));
	std::getline(in_2, line);
	CHECK(line == "Hello, world! - this is line 1");

	gxrio::ifstream in_3;
	in_3 = std::move(in_2);
	std::getline(in_3, line);
	CHECK(line == "Hello, world! - this is line 2");
}

// --------------------------------------------------------------------

template <std::size_t SIZE, class CharT = char>
class ArrayedStreamBuffer : public std::basic_streambuf<CharT>
{
  public:
	using Base = std::basic_streambuf<CharT>;
	using char_type = typename Base::char_type;
	using int_type = typename Base::int_type;

	ArrayedStreamBuffer()
		: buffer_{} // value-initialize buffer_ to all zeroes
	{
		Base::setp(buffer_.data(), buffer_.data() + buffer_.size()); // set std::basic_streambuf
		                                                            // put area pointers to work with 'buffer_'
	}

	void reading()
	{
		Base::setg(Base::pbase(), Base::pbase(), Base::pptr());
	}

	int_type overflow(int_type ch)
	{
		return Base::overflow(ch);
	}

  private:
	std::array<char_type, SIZE> buffer_;
};

TEST_CASE("compress directly to a streambuf", "[d_1]")
{
	ArrayedStreamBuffer<100> buffer;

	gxrio::basic_oxz_streambuf<char, std::char_traits<char>> zb;
	zb.init(&buffer);

	zb.sputn("Hello, world!", 13);

	zb.close();

	buffer.reading();

	gxrio::istream in(&buffer);

	std::string line;
	std::getline(in, line);

	CHECK(line == "Hello, world!");
}

// --------------------------------------------------------------------

TEST_CASE("round trip a 1000 line xz file", "[d_2]")
{
	auto filename = "hello-1000.txt.xz";

	fs::path in_file = gTestDir / filename;
	fs::path out_file = fs::temp_directory_path() / filename;

	gxrio::ifstream in;
	gxrio::ofstream out;

	in.open(in_file);
	out.open(out_file);

	REQUIRE(in.is_open());
	REQUIRE(out.is_open());

	std::string line;

	while (getline(in, line))
		out << line << std::endl;

	in.close();
	out.close();

	in.open(in_file);
	gxrio::ifstream in_test(out_file);

	REQUIRE(in.is_open());
	REQUIRE(in_test.is_open());

	int n = 0;
	std::string test_line;

	for (;;)
	{
		bool b1 = (bool)getline(in, line);
		bool b2 = (bool)getline(in_test, test_line);

		CHECK(b1 == b2);

		if (not (b1 and b2))
			break;

		CHECK(line == test_line);
		++n;
	}

	CHECK(n == 1000);
}

// --------------------------------------------------------------------

TEST_CASE("move an oxz streambuf while compressing", "[d_3]")
{
	ArrayedStreamBuffer<100> buffer;

	gxrio::basic_oxz_streambuf<char, std::char_traits<char>> zb;
	zb.init(&buffer);

	zb.sputn("aap ", 4);

	gxrio::basic_oxz_streambuf<char, std::char_traits<char>> zb2(std::move(zb));

	zb2.sputn("noot ", 5);

	gxrio::basic_oxz_streambuf<char, std::char_traits<char>> zb3;

	zb3 = std::move(zb2);

	zb3.sputn("mies\n", 5);

	zb3.close();

	buffer.reading();

	gxrio::istream in(&buffer);

	std::string line;
	std::getline(in, line);

	CHECK(line == "aap noot mies");
}

// --------------------------------------------------------------------

TEST_CASE("move an ixz streambuf while decompressing", "[d_4]")
{
	// moving a basic_ixz_streambuf while decompressing
	struct membuf : public std::streambuf
	{
		membuf(char *text, size_t length)
		{
			this->setg(text, text, text + length);
		}
	} buffer(reinterpret_cast<char *>(kXZData), sizeof(kXZData));

	gxrio::basic_ixz_streambuf<char, std::char_traits<char>> zb;
	zb.init(&buffer);

	zb.sbumpc();

	gxrio::basic_ixz_streambuf<char, std::char_traits<char>> zb2(std::move(zb));
	zb2.sbumpc();

	gxrio::basic_ixz_streambuf<char, std::char_traits<char>> zb3;
	zb3 = std::move(zb2);

	std::string line;
	int ch;
	while ((ch = zb3.sbumpc()) != std::char_traits<char>::eof())
		line += static_cast<char>(ch);

	CHECK(line == "llo, world!\n");
}

// --------------------------------------------------------------------

TEST_CASE("set the compression level on a streambuf", "[d_5]")
{
	ArrayedStreamBuffer<100> buffer;

	gxrio::basic_oxz_streambuf<char, std::char_traits<char>> zb;
	zb.set_compression_level(1);
	zb.init(&buffer);

	zb.sputn("Hello, world!", 13);

	zb.close();

	buffer.reading();

	gxrio::istream in(&buffer);

	std::string line;
	std::getline(in, line);

	CHECK(line == "Hello, world!");
}
