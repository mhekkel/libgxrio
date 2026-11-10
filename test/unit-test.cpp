// Copyright Maarten L. Hekkelman, Radboud University 2008-2013.
// SPDX-FileCopyrightText: 2026 Maarten L. Hekkelman
//
// SPDX-License-Identifier: BSL-1.0

#include "test-main.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
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
	for (const fs::path& f : {
			 gTestDir / "hello.txt.gz",
#if HAVE_LibLZMA
			 gTestDir / "hello.txt.xz",
#endif
			 gTestDir / "hello.txt" })
	{
		std::filebuf fb;
		fb.open(f, std::ios::in | std::ios::binary);
		REQUIRE(fb.is_open());

		gxrio::istream is(&fb);

		std::string line;

		getline(is, line);

		CHECK(line == "Hello, world!");
	}
}

// --------------------------------------------------------------------

TEST_CASE("reading compressed and uncompressed files via ifstream", "[t_2]")
{
	for (const fs::path& f : {
			 gTestDir / "hello.txt.gz",
#if HAVE_LibLZMA
			 gTestDir / "hello.txt.xz",
#endif
			 gTestDir / "hello.txt" })
	{
		gxrio::ifstream is(f);
		REQUIRE(is.is_open());

		std::string line;

		getline(is, line);

		CHECK(line == "Hello, world!");
	}
}

// --------------------------------------------------------------------

TEST_CASE("move constructing an ifstream", "[t_3]")
{
	for (const fs::path& f : {
			 gTestDir / "hello.txt.gz",
#if HAVE_LibLZMA
			 gTestDir / "hello.txt.xz",
#endif
			 gTestDir / "hello.txt" })
	{
		gxrio::ifstream is1(f);
		REQUIRE(is1.is_open());

		gxrio::ifstream is2(std::move(is1));
		REQUIRE(is2.is_open());

		std::string line;

		getline(is2, line);

		CHECK(line == "Hello, world!");
	}
}

// --------------------------------------------------------------------

TEST_CASE("move assigning an ifstream", "[t_4]")
{
	for (const fs::path& f : {
			 gTestDir / "hello.txt.gz",
#if HAVE_LibLZMA
			 gTestDir / "hello.txt.xz",
#endif
			 gTestDir / "hello.txt" })
	{
		gxrio::ifstream is1(f);
		REQUIRE(is1.is_open());

		gxrio::ifstream is2;

		is2 = std::move(is1);
		REQUIRE(is2.is_open());

		std::string line;

		getline(is2, line);

		CHECK(line == "Hello, world!");
	}
}

// --------------------------------------------------------------------

TEST_CASE("writing and reading back compressed files", "[t_5]")
{
	for (const fs::path& f : {
			 gTestTempDir / "hello.txt.gz",
#if HAVE_LibLZMA
			 gTestTempDir / "hello.txt.xz",
#endif
			 gTestTempDir / "hello.txt" })
	{
		gxrio::ofstream out(f);
		REQUIRE(out.is_open());

		out << "Hello, world!" << '\n';
		out.close();

		gxrio::ifstream in(f);
		REQUIRE(in.is_open());

		std::string line;

		getline(in, line);

		CHECK(line == "Hello, world!");
	}
}

// --------------------------------------------------------------------

TEST_CASE("move constructing an ofstream", "[t_6]")
{

	for (const fs::path& f : {
			 gTestTempDir / "hello.txt.gz",
#if HAVE_LibLZMA
			 gTestTempDir / "hello.txt.xz",
#endif
			 gTestTempDir / "hello.txt" })
	{
		gxrio::ofstream out_1(f);
		REQUIRE(out_1.is_open());

		gxrio::ofstream out_2(std::move(out_1));
		REQUIRE(out_2.is_open());

		out_2 << "Hello, world!" << '\n';
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
	for (const fs::path& f : {
			 gTestTempDir / "hello.txt.gz",
#if HAVE_LibLZMA
			 gTestTempDir / "hello.txt.xz",
#endif
			 gTestTempDir / "hello.txt" })
	{
		gxrio::ofstream out_1(f);
		REQUIRE(out_1.is_open());

		gxrio::ofstream out_2;
		out_2 = std::move(out_1);
		REQUIRE(out_2.is_open());

		out_2 << "Hello, world!" << '\n';
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
			 { reinterpret_cast<const char *>(kGZippedData), sizeof(kGZippedData) },
#if HAVE_LibLZMA
			 { reinterpret_cast<const char *>(kXZData), sizeof(kXZData) },
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

		REQUIRE(line.length() >= 13);
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

	fs::path out_file = gTestTempDir / "swap-out.txt.gz";

	gxrio::ofstream out_1, out_2(out_file);
	out_1.swap(out_2);

	out_1 << "Hello, world!" << '\n';
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
	REQUIRE(in.is_open());

	std::string line;
	REQUIRE(std::getline(in, line));
	CHECK(line == "Hello, world!");
	CHECK_FALSE(in.bad());

	in.close();
	in.open(gTestDir / "hello.txt");
	REQUIRE(in.is_open());
	line.clear();
	REQUIRE(std::getline(in, line));
	CHECK(line == "Hello, world!");
	CHECK_FALSE(in.bad());

	// and the other direction: plain first, compressed second
	in.close();
	in.open(gTestDir / "hello.txt");
	REQUIRE(in.is_open());
	line.clear();
	REQUIRE(std::getline(in, line));
	CHECK(line == "Hello, world!");

	in.close();
	in.open(gTestDir / "hello.txt.gz");
	REQUIRE(in.is_open());
	line.clear();
	REQUIRE(std::getline(in, line));
	CHECK(line == "Hello, world!");
	CHECK_FALSE(in.bad());

	fs::path out_dir = gTestTempDir;

	gxrio::ofstream out;
	out.open(out_dir / "reopen.txt.gz");
	REQUIRE(out.is_open());
	out << "Hello, world!" << '\n';
	out.close();

	out.open(out_dir / "reopen.txt");
	REQUIRE(out.is_open());
	out << "Hello, world!" << '\n';
	out.close();

	// Verify with a plain ifstream that the .txt file really is uncompressed;
	// gxrio detects compression by content and would otherwise hide a stale
	// compressor here.
	std::ifstream raw_txt(out_dir / "reopen.txt", std::ios::binary);
	std::string contents(
		(std::istreambuf_iterator<char>(raw_txt)),
		std::istreambuf_iterator<char>());
	CHECK(contents == "Hello, world!\n");

	// The reverse direction must work as well.
	out.open(out_dir / "reopen.txt");
	REQUIRE(out.is_open());
	out << "Hello, world!" << '\n';
	out.close();

	out.open(out_dir / "reopen.txt.gz");
	REQUIRE(out.is_open());
	out << "Hello, world!" << '\n';
	out.close();

	std::ifstream raw_gz(out_dir / "reopen.txt.gz", std::ios::binary);
	unsigned char magic[2] = {};
	raw_gz.read(reinterpret_cast<char *>(magic), sizeof(magic));
	REQUIRE(raw_gz.gcount() == static_cast<std::streamsize>(sizeof(magic)));
	CHECK(magic[0] == 0x1f);
	CHECK(magic[1] == 0x8b);

	gxrio::ifstream in_2(out_dir / "reopen.txt");
	REQUIRE(in_2.is_open());
	line.clear();
	REQUIRE(std::getline(in_2, line));
	CHECK(line == "Hello, world!");
}

// --------------------------------------------------------------------

TEST_CASE("default constructed streams are good", "[t_default]")
{
	// like the standard streams, default construction must not put the stream
	// in a failed state
	gxrio::ifstream in;
	gxrio::ofstream out;

	CHECK(in.good());
	CHECK(out.good());
	CHECK(in.rdbuf() != nullptr);
	CHECK(out.rdbuf() != nullptr);
}

// --------------------------------------------------------------------

TEST_CASE("a failed open sets failbit but not badbit", "[t_default]")
{
	auto missing_dir = gTestTempDir / "missing-dir";
	fs::remove_all(missing_dir);

	gxrio::ifstream in;
	in.open(missing_dir / "does-not-exist.txt.gz");
	CHECK(in.fail());
	CHECK_FALSE(in.bad());

	gxrio::ofstream out;
	out.open(missing_dir / "does-not-exist.txt.gz");
	CHECK(out.fail());
	CHECK_FALSE(out.bad());

	gxrio::ifstream in_2(missing_dir / "does-not-exist.txt.gz");
	CHECK(in_2.fail());
	CHECK_FALSE(in_2.bad());

	gxrio::ofstream out_2(missing_dir / "does-not-exist.txt.gz");
	CHECK(out_2.fail());
	CHECK_FALSE(out_2.bad());
}

// --------------------------------------------------------------------

#if !HAVE_LibLZMA
TEST_CASE("xz streams are rejected without LibLZMA", "[t_no_lzma]")
{
	auto out_dir = gTestTempDir;

	gxrio::ifstream in(gTestDir / "hello.txt.xz");
	CHECK(in.fail());
	CHECK(in.bad());
	CHECK_FALSE(in.is_open());

	fs::path rejected = out_dir / "no-lzma.txt.xz";
	fs::remove(rejected);

	gxrio::ofstream out(rejected);
	CHECK(out.fail());
	CHECK(out.bad());
	CHECK_FALSE(out.is_open());
	CHECK_FALSE(fs::exists(rejected));

	fs::path preserved = out_dir / "no-lzma-preserved.txt.xz";
	{
		std::ofstream keep(preserved, std::ios::binary | std::ios::trunc);
		keep << "keep";
	}

	gxrio::ofstream out_2(preserved);
	CHECK(out_2.fail());
	CHECK(out_2.bad());
	CHECK(fs::file_size(preserved) == 4);

	// content sniffing through a plain streambuf must fail as well instead of
	// silently returning the compressed bytes
	std::ifstream raw(gTestDir / "hello.txt.xz", std::ios::binary);
	gxrio::istream in_2(raw.rdbuf());
	CHECK(in_2.fail());
	CHECK(in_2.bad());
	CHECK(in_2.rdbuf() == nullptr);

	// clear() must not turn the rejected stream into a readable one
	in_2.clear();
	CHECK(in_2.bad());

	std::string line;
	CHECK_FALSE(static_cast<bool>(std::getline(in_2, line)));
}
#endif

// --------------------------------------------------------------------

TEST_CASE("moving default and closed streambufs is safe", "[t_move_streambuf]")
{
	using traits = std::char_traits<char>;

	using gzip_in = gxrio::basic_igzip_streambuf<char, traits>;
	using gzip_out = gxrio::basic_ogzip_streambuf<char, traits>;

	gzip_in in;
	CHECK(in.init(nullptr) == nullptr);

	gzip_in in_2(std::move(in));
	in = std::move(in_2);

	std::stringbuf sink;
	gzip_out out;
	CHECK(out.init(nullptr) == nullptr);
	REQUIRE(out.init(&sink) != nullptr);
	out.sputn("Hello, world!", 13);
	out.close();

	gzip_out out_2(std::move(out));
	gzip_out out_3;
	out_3 = std::move(out_2);

#if HAVE_LibLZMA
	using xz_in = gxrio::basic_ixz_streambuf<char, traits>;
	using xz_out = gxrio::basic_oxz_streambuf<char, traits>;

	xz_in xin;
	CHECK(xin.init(nullptr) == nullptr);

	xz_in xin_2(std::move(xin));
	xin = std::move(xin_2);

	std::stringbuf xsink;
	xz_out xout;
	CHECK(xout.init(nullptr) == nullptr);
	REQUIRE(xout.init(&xsink) != nullptr);
	xout.sputn("Hello, world!", 13);
	xout.close();

	xz_out xout_2(std::move(xout));
	xz_out xout_3;
	xout_3 = std::move(xout_2);
#endif
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

TEST_CASE("truncated compressed input sets badbit", "[t_12]")
{
	// truncated compressed input reads what is available and hits eof without hanging
	for (const fs::path& f : {
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
			REQUIRE(line.length() >= 13);

		// Truncated input is an error and must be distinguishable from a clean
		// end of the compressed stream.
		CHECK(in.bad());
	}
}

// --------------------------------------------------------------------

TEST_CASE("corrupt compressed input sets badbit", "[t_13]")
{
	// corrupt a valid gzip stream; decoding must stop with badbit
	std::vector<unsigned char> data(std::begin(kGZippedData), std::end(kGZippedData));
	data[data.size() / 2] ^= 0x5a;

	struct membuf : public std::streambuf
	{
		membuf(std::vector<unsigned char> &data)
		{
			auto *p = reinterpret_cast<char *>(data.data());
			this->setg(p, p, p + data.size());
		}
	} buffer(data);

	gxrio::istream in(&buffer);

	std::string line;
	while (getline(in, line))
		;

	CHECK(in.bad());
}

// --------------------------------------------------------------------

TEST_CASE("setting the compression level", "[t_14]")
{
	auto out_dir = gTestTempDir;

	auto line_for = [](int i)
	{
		return "line " + std::to_string(i) + ": alpha bravo charlie " + std::to_string((static_cast<unsigned>(i) * 2654435761u) % 1000000);
	};

	std::string data;
	for (int i = 0; i < 2000; ++i)
		data += line_for(i) + "\n";

	auto write_level = [&](int level, const fs::path &f)
	{
		gxrio::ofstream out;
		out.set_compression_level(level);
		out.open(f);
		REQUIRE(out.is_open());
		out.write(data.data(), static_cast<std::streamsize>(data.size()));
		REQUIRE(out.good());
		out.close();
		REQUIRE_FALSE(out.fail());
	};

	for (const std::string &ext : { std::string(".gz"), std::string(".xz") })
	{
#if !HAVE_LibLZMA
		if (ext == ".xz")
			continue;
#endif
		fs::path low = out_dir / ("level0" + ext);
		fs::path high = out_dir / ("level9" + ext);

		write_level(0, low);
		write_level(9, high);

		// A higher compression level must result in a smaller file.
		CHECK(std::filesystem::file_size(low) > std::filesystem::file_size(high));

		gxrio::ifstream in(high);
		REQUIRE(in.is_open());
		std::string line;
		int n = 0;
		while (std::getline(in, line))
		{
			CHECK(line == line_for(n));
			++n;
		}
		CHECK(n == 2000);
		CHECK_FALSE(in.bad());
	}

	// Out-of-range levels are clamped to the documented range instead of
	// making open() fail (and, historically, leaving rdbuf dangling).
	{
		fs::path f = out_dir / "level-clamped.gz";
		gxrio::ofstream out;
		out.set_compression_level(42);
		out.open(f);
		REQUIRE(out.is_open());
		out << "Hello, world!" << '\n';
		out.close();
		REQUIRE_FALSE(out.fail());

		gxrio::ifstream in(f);
		std::string line;
		REQUIRE(std::getline(in, line));
		CHECK(line == "Hello, world!");
	}
}

// --------------------------------------------------------------------

TEST_CASE("moving an ofstream preserves the compression level", "[t_move_level]")
{
	auto out_dir = gTestTempDir;

	std::string data;
	for (int i = 0; i < 1000; ++i)
	{
		data += "line " + std::to_string(i) + ": alpha bravo charlie " + std::to_string((static_cast<unsigned>(i) * 2654435761u) % 1000000) + "\n";
	}

	auto write = [&](gxrio::ofstream &out, const fs::path &f)
	{
		out.open(f);
		REQUIRE(out.is_open());
		out.write(data.data(), static_cast<std::streamsize>(data.size()));
		REQUIRE(out.good());
		out.close();
		REQUIRE_FALSE(out.fail());
	};

	for (const std::string &ext : { std::string(".gz"), std::string(".xz") })
	{
#if !HAVE_LibLZMA
		if (ext == ".xz")
			continue;
#endif
		fs::path reference = out_dir / ("moved-level0" + ext);
		fs::path moved_ctor = out_dir / ("moved-ctor" + ext);
		fs::path moved_assign = out_dir / ("moved-assign" + ext);
		fs::path default_level = out_dir / ("moved-default" + ext);

		{
			gxrio::ofstream out;
			out.set_compression_level(0);
			write(out, reference);
		}
		{
			gxrio::ofstream src;
			src.set_compression_level(0);
			gxrio::ofstream out(std::move(src));
			write(out, moved_ctor);
		}
		{
			gxrio::ofstream src;
			src.set_compression_level(0);
			gxrio::ofstream out;
			out = std::move(src);
			write(out, moved_assign);
		}
		{
			gxrio::ofstream out;
			write(out, default_level);
		}

		CHECK(std::filesystem::file_size(moved_ctor) == std::filesystem::file_size(reference));
		CHECK(std::filesystem::file_size(moved_assign) == std::filesystem::file_size(reference));
		CHECK(std::filesystem::file_size(moved_ctor) > std::filesystem::file_size(default_level));
	}
}

// --------------------------------------------------------------------

TEST_CASE("reading concatenated gzip members", "[t_21]")
{
	gxrio::ifstream file(gTestDir / "aap.gz");

	REQUIRE(file.is_open());

	std::string line;

	REQUIRE(getline(file, line));
	CHECK(line == "aap");

	REQUIRE(getline(file, line));
	CHECK(line == "noot");

	REQUIRE(getline(file, line));
	CHECK(line == "mies");

	REQUIRE_FALSE(getline(file, line));
	CHECK(file.eof());
}

// --------------------------------------------------------------------

TEST_CASE("gxrio::ostream writes uncompressed data", "[t_ostream]")
{
	std::stringbuf buffer;

	{
		gxrio::ostream out(&buffer);
		REQUIRE(out.good());

		out << "Hello, world!";
	}

	CHECK(buffer.str() == "Hello, world!");
}
