// Copyright Maarten L. Hekkelman, Radboud University 2008-2013.
// SPDX-FileCopyrightText: 2026 Maarten L. Hekkelman
//
// SPDX-License-Identifier: BSL-1.0

#include "test-main.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <random>
#include <string>
#include <vector>

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

TEST_CASE("decompress an xz file without an integrity check", "[t_nocheck]")
{
	// A stream with LZMA_CHECK_NONE makes lzma_code() return LZMA_NO_CHECK
	// once; that must be treated as success rather than as a fatal error.
	fs::path f = gTestDir / "hello-nocheck.txt.xz";

	gxrio::ifstream in(f, std::ios::in | std::ios::binary);
	REQUIRE(in.is_open());

	std::string line;
	REQUIRE(std::getline(in, line));

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

TEST_CASE("compress directly to a streambuf", "[d_1]")
{
	array_streambuf<100> buffer;

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
	fs::path out_file = gTestTempDir / filename;

	gxrio::ifstream in;
	gxrio::ofstream out;

	in.open(in_file);
	out.open(out_file);

	REQUIRE(in.is_open());
	REQUIRE(out.is_open());

	std::string line;

	while (getline(in, line))
		out << line << '\n';

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
		bool b1 = getline(in, line) ? true : false;
		bool b2 = getline(in_test, test_line) ? true : false;

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
	array_streambuf<100> buffer;

	gxrio::basic_oxz_streambuf<char, std::char_traits<char>> zb;
	zb.init(&buffer);

	zb.sputn("aap ", 4);

	gxrio::basic_oxz_streambuf<char, std::char_traits<char>> zb2(std::move(zb));

	zb2.sputn("noot ", 5);

	array_streambuf<100> discarded;

	gxrio::basic_oxz_streambuf<char, std::char_traits<char>> zb3;
	REQUIRE(zb3.init(&discarded) != nullptr);

	zb3.sputn("old ", 4);

	zb3 = std::move(zb2);

	zb3.sputn("mies\n", 5);

	zb3.close();

	buffer.reading();

	gxrio::istream in(&buffer);

	std::string line;
	std::getline(in, line);

	CHECK(line == "aap noot mies");

	discarded.reading();

	gxrio::istream discarded_in(&discarded);

	std::string old_line;
	std::getline(discarded_in, old_line);

	CHECK(old_line == "old ");
}

// --------------------------------------------------------------------

TEST_CASE("re-initializing an output streambuf keeps data on the old upstream", "[d_reinit]")
{
	array_streambuf<4096> first, second;

	gxrio::basic_oxz_streambuf<char, std::char_traits<char>> zb;
	REQUIRE(zb.init(&first) != nullptr);
	REQUIRE(zb.sputn("aap", 3) == 3);

	// Re-initializing must finish the old stream on the old upstream, not
	// flush its buffered bytes to the new one.
	REQUIRE(zb.init(&second) != nullptr);
	REQUIRE(zb.sputn("noot", 4) == 4);
	REQUIRE(zb.close() != nullptr);

	first.reading();
	gxrio::istream in_first(&first);
	std::string line;
	REQUIRE(std::getline(in_first, line));
	CHECK(line == "aap");

	second.reading();
	gxrio::istream in_second(&second);
	line.clear();
	REQUIRE(std::getline(in_second, line));
	CHECK(line == "noot");
}

// --------------------------------------------------------------------

TEST_CASE("xz output write errors set badbit", "[d_error]")
{
	struct failing_streambuf : public std::streambuf
	{
		std::streamsize xsputn(const char *, std::streamsize) override
		{
			return 0;
		}

		int_type overflow(int_type) override
		{
			return traits_type::eof();
		}
	} sink;

	gxrio::basic_oxz_streambuf<char, std::char_traits<char>> zb;
	std::ostream out(&zb);
	zb.set_ios(&out);

	REQUIRE(zb.init(&sink) != nullptr);
	CHECK(zb.close() == nullptr);
	CHECK(out.bad());
}

// --------------------------------------------------------------------

TEST_CASE("xz output errors do not throw from destruction", "[d_error_exceptions]")
{
	struct failing_streambuf : public std::streambuf
	{
		std::streamsize xsputn(const char *, std::streamsize) override
		{
			return 0;
		}

		int_type overflow(int_type) override
		{
			return traits_type::eof();
		}
	} sink;

	// The streambuf must outlive the ostream here so its destructor can still
	// report the error to it.
	auto zb = std::make_unique<gxrio::basic_oxz_streambuf<char, std::char_traits<char>>>();
	std::ostream out(zb.get());
	zb->set_ios(&out);
	out.exceptions(std::ios::badbit);

	REQUIRE(zb->init(&sink) != nullptr);

	// Flushing in the destructor hits the failing sink. With badbit exceptions
	// enabled this used to call std::terminate.
	zb.reset();

	CHECK(out.bad());
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
	std::string data;
	for (int i = 0; i < 1000; ++i)
	{
		data += "line " + std::to_string(i) + ": alpha bravo charlie " + std::to_string((static_cast<unsigned>(i) * 2654435761u) % 1000000) + "\n";
	}

	array_streambuf<65536> level0, level9;

	gxrio::basic_oxz_streambuf<char, std::char_traits<char>> z0, z9;

	z0.set_compression_level(0);
	REQUIRE(z0.init(&level0) != nullptr);
	REQUIRE(z0.sputn(data.data(), static_cast<std::streamsize>(data.size())) ==
		static_cast<std::streamsize>(data.size()));
	REQUIRE(z0.close() != nullptr);

	z9.set_compression_level(9);
	REQUIRE(z9.init(&level9) != nullptr);
	REQUIRE(z9.sputn(data.data(), static_cast<std::streamsize>(data.size())) ==
		static_cast<std::streamsize>(data.size()));
	REQUIRE(z9.close() != nullptr);

	CHECK(level0.size() > level9.size());

	level9.reading();

	gxrio::istream in(&level9);

	std::string line;
	REQUIRE(std::getline(in, line));

	CHECK(line == "line 0: alpha bravo charlie 0");
}

// --------------------------------------------------------------------

TEST_CASE("xz stream padding is accepted", "[d_padding]")
{
	std::ifstream data(gTestDir / "hello.txt.xz", std::ios::binary);
	std::vector<char> contents(
		(std::istreambuf_iterator<char>(data)),
		std::istreambuf_iterator<char>());

	contents.insert(contents.end(), {0, 0, 0, 0});

	struct membuf : public std::streambuf
	{
		membuf(std::vector<char> &data)
		{
			this->setg(data.data(), data.data(), data.data() + data.size());
		}
	} buffer(contents);

	gxrio::istream in(&buffer);

	std::string line;
	while (std::getline(in, line))
		;

	CHECK(in.eof());
	CHECK_FALSE(in.bad());
}

// --------------------------------------------------------------------

TEST_CASE("concatenated xz streams are decoded", "[d_concat]")
{
	std::ifstream data(gTestDir / "hello.txt.xz", std::ios::binary);
	std::vector<char> contents(
		(std::istreambuf_iterator<char>(data)),
		std::istreambuf_iterator<char>());

	contents.insert(contents.end(), contents.begin(), contents.end());

	struct membuf : public std::streambuf
	{
		membuf(std::vector<char> &data)
		{
			this->setg(data.data(), data.data(), data.data() + data.size());
		}
	} buffer(contents);

	gxrio::istream in(&buffer);

	std::vector<std::string> lines;
	std::string line;
	while (std::getline(in, line))
		lines.push_back(line);

	REQUIRE(lines.size() == 2);
	CHECK(lines[0] == "Hello, world!");
	CHECK(lines[1] == "Hello, world!");
	CHECK_FALSE(in.bad());
}

// --------------------------------------------------------------------

TEST_CASE("garbage after an xz stream sets badbit", "[d_trailing]")
{
	std::ifstream data(gTestDir / "hello.txt.xz", std::ios::binary);
	std::vector<char> contents(
		(std::istreambuf_iterator<char>(data)),
		std::istreambuf_iterator<char>());

	const char garbage[] = {'g', 'a', 'r', 'b', 'a', 'g', 'e'};
	contents.insert(contents.end(), std::begin(garbage), std::end(garbage));

	struct membuf : public std::streambuf
	{
		membuf(std::vector<char> &data)
		{
			this->setg(data.data(), data.data(), data.data() + data.size());
		}
	} buffer(contents);

	gxrio::istream in(&buffer);

	std::string line;
	while (std::getline(in, line))
		;

	CHECK(in.bad());
}

// --------------------------------------------------------------------

TEST_CASE("flush makes compressed data available before close", "[d_flush]")
{
	array_streambuf<65536> buffer;

	gxrio::basic_oxz_streambuf<char, std::char_traits<char>> zb;
	REQUIRE(zb.init(&buffer) != nullptr);

	std::ostream out(&zb);
	out << "Hello, world!";
	out.flush();

	CHECK(buffer.size() > 0);

	zb.close();

	buffer.reading();

	gxrio::istream in(&buffer);
	std::string line;
	REQUIRE(std::getline(in, line));
	CHECK(line == "Hello, world!");
	CHECK_FALSE(in.bad());
}

// --------------------------------------------------------------------

TEST_CASE("valid xz data does not set badbit", "[d_valid]")
{
	gxrio::ifstream in(gTestDir / "hello.txt.xz");
	REQUIRE(in.good());

	std::string line;
	while (std::getline(in, line))
		;

	CHECK(in.eof());
	CHECK_FALSE(in.bad());
}

// --------------------------------------------------------------------

TEST_CASE("corrupt xz data sets badbit", "[d_corrupt]")
{
	std::ifstream data(gTestDir / "hello.txt.xz", std::ios::binary);
	std::vector<char> contents(
		(std::istreambuf_iterator<char>(data)),
		std::istreambuf_iterator<char>());

	REQUIRE(contents.size() > 12);

	const size_t offset = contents.size() / 2;
	contents[offset] = static_cast<char>(contents[offset] ^ 0x5a);

	struct membuf : public std::streambuf
	{
		membuf(std::vector<char> &data)
		{
			this->setg(data.data(), data.data(), data.data() + data.size());
		}
	} buffer(contents);

	gxrio::istream in(&buffer);

	std::string line;
	while (std::getline(in, line))
		;

	CHECK(in.bad());
}

// --------------------------------------------------------------------

TEST_CASE("truncated xz data sets badbit", "[d_truncated]")
{
	std::ifstream data(gTestDir / "hello.txt.xz", std::ios::binary);
	std::vector<char> contents(
		(std::istreambuf_iterator<char>(data)),
		std::istreambuf_iterator<char>());

	REQUIRE(contents.size() > 4);
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
	while (std::getline(in, line))
		;

	CHECK(in.bad());
}

// --------------------------------------------------------------------

TEST_CASE("an empty xz stream decodes to nothing", "[d_empty]")
{
	gxrio::ofstream out(gTestTempDir / "empty.txt.xz");
	REQUIRE(out.is_open());
	out.close();
	REQUIRE_FALSE(out.fail());

	gxrio::ifstream in(gTestTempDir / "empty.txt.xz");
	REQUIRE(in.is_open());

	std::string line;
	CHECK_FALSE(std::getline(in, line));
	CHECK(in.eof());
	CHECK_FALSE(in.bad());
}

// --------------------------------------------------------------------

TEST_CASE("a zero byte .xz file is truncated", "[d_empty]")
{
	fs::path f = gTestTempDir / "zero-bytes.txt.xz";
	{
		std::ofstream raw(f, std::ios::binary | std::ios::trunc);
		REQUIRE(raw.good());
	}

	gxrio::ifstream in(f);
	REQUIRE(in.is_open());

	std::string line;
	while (std::getline(in, line))
		;

	CHECK(in.bad());
}

// --------------------------------------------------------------------

TEST_CASE("short reads split xz signatures and members", "[d_chunked]")
{
	using traits = std::char_traits<char>;

	std::vector<char> single(std::begin(kXZData), std::end(kXZData));
	std::vector<char> twice = single;
	twice.insert(twice.end(), single.begin(), single.end());

	const std::string expected_single = "Hello, world!\n";
	const std::string expected_twice = expected_single + expected_single;

	for (unsigned chunk : { 1u, 2u, 3u, 5u, 7u })
	{
		for (const std::vector<char> *data : { &single, &twice })
		{
			// A short-read upstream directly below the decoder forces refills
			// at arbitrary offsets, including inside the stream header.
			chunked_streambuf buffer(data->data(), data->size(), chunk_size{ chunk });

			gxrio::basic_ixz_streambuf<char, traits> zb;
			REQUIRE(zb.init(&buffer) != nullptr);

			std::string got;
			int ch;
			while ((ch = zb.sbumpc()) != traits::eof())
				got += static_cast<char>(ch);

			CHECK_FALSE(zb.error());
			CHECK(got == (data == &single ? expected_single : expected_twice));
		}

		// And the sniffing layer must cope with a signature split across reads.
		chunked_streambuf buffer(single.data(), single.size(), chunk_size{ chunk });
		gxrio::istream in(&buffer);
		std::string line;
		REQUIRE(std::getline(in, line));
		CHECK(line == "Hello, world!");
		CHECK_FALSE(in.bad());
	}
}

// --------------------------------------------------------------------

TEST_CASE("xz flush errors set badbit", "[d_flush_error]")
{
	struct failing_streambuf : public std::streambuf
	{
		std::streamsize xsputn(const char *, std::streamsize) override
		{
			return 0;
		}

		int_type overflow(int_type) override
		{
			return traits_type::eof();
		}
	} sink;

	gxrio::basic_oxz_streambuf<char, std::char_traits<char>> zb;
	REQUIRE(zb.init(&sink) != nullptr);

	std::ostream out(&zb);
	zb.set_ios(&out);

	out << "Hello, world!";
	out.flush();

	CHECK(zb.error());
	CHECK(out.bad());
}

// --------------------------------------------------------------------

TEST_CASE("randomized xz round trips", "[d_random]")
{
	std::seed_seq seed{ 0xBA, 0xDF, 0x00, 0xD };
	std::mt19937 rng(seed);

	for (int iteration = 0; iteration < 20; ++iteration)
	{
		std::string data(rng() % 50000, '\0');

		if (iteration % 3 == 0)
			std::fill(data.begin(), data.end(), 'x');
		else
			for (char &ch : data)
				ch = static_cast<char>(rng());

		array_streambuf<262144> buffer;

		gxrio::basic_oxz_streambuf<char, std::char_traits<char>> zb;
		REQUIRE(zb.init(&buffer) != nullptr);

		std::size_t pos = 0;
		while (pos < data.size())
		{
			std::size_t n = std::min<std::size_t>(rng() % 1000 + 1, data.size() - pos);
			REQUIRE(zb.sputn(data.data() + pos, static_cast<std::streamsize>(n)) == static_cast<std::streamsize>(n));
			pos += n;
		}
		REQUIRE(zb.close() != nullptr);

		buffer.reading();

		gxrio::istream in(&buffer);
		std::string got((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

		CHECK(got == data);
		CHECK_FALSE(in.bad());
	}
}
