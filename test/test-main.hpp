// SPDX-FileCopyrightText: 2026 Maarten L. Hekkelman
//
// SPDX-License-Identifier: BSL-1.0

#ifndef GXRIO_TEST_MAIN_HPP
#define GXRIO_TEST_MAIN_HPP

#define CATCH_CONFIG_RUNNER
#include <catch2/catch_all.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <random>
#include <streambuf>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

/// \brief The directory that contains the test data files
inline fs::path gTestDir;

/// \brief A unique temporary directory for this test process
///
/// Using a per-process directory keeps concurrently running test executables
/// from clobbering each other's files.
inline fs::path gTestTempDir;

/// \brief Create the temporary directory used by the tests
inline fs::path make_test_temp_dir()
{
	std::random_device device;
	const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();

	fs::path dir = fs::temp_directory_path() /
		("gxrio-unit-test-" + std::to_string(stamp) + "-" + std::to_string(device()));

	std::error_code ec;
	fs::create_directories(dir, ec);
	return dir;
}

/// \brief Named chunk size, keeping it distinct from the data length
struct chunk_size
{
	std::size_t value;
};

/// \brief A streambuf that hands out its data in chunks of at most \a chunk bytes
///
/// Used to exercise code paths where format signatures, headers and
/// concatenated members are split across reads. Overriding xsgetn() makes the
/// upstream return short reads as well, which forces the decompressors to
/// refill at arbitrary offsets.
class chunked_streambuf : public std::streambuf
{
  public:
	chunked_streambuf(const char *data, std::size_t size, chunk_size chunk)
		: m_data(data), m_size(size), m_chunk(std::max<std::size_t>(chunk.value, 1))
	{
	}

  protected:
	int_type underflow() override
	{
		if (m_pos >= m_size)
			return traits_type::eof();

		std::size_t n = std::min(m_chunk, m_size - m_pos);
		char *p = const_cast<char *>(m_data) + m_pos;
		m_pos += n;
		this->setg(p, p, p + n);
		return traits_type::to_int_type(*this->gptr());
	}

	std::streamsize xsgetn(char_type *s, std::streamsize n) override
	{
		if (n <= 0)
			return 0;

		// Reuse the get area so that mixing sgetc()/sbumpc() with sgetn()
		// (as the sniffing layer and the decompressors do) never skips bytes.
		if (this->gptr() == nullptr or this->gptr() >= this->egptr())
		{
			if (underflow() == traits_type::eof())
				return 0;
		}

		std::streamsize available = this->egptr() - this->gptr();
		std::streamsize count = std::min(n, available);
		std::copy_n(this->gptr(), count, s);
		this->gbump(static_cast<int>(count));
		return count;
	}

  private:
	const char *m_data;
	std::size_t m_size;
	std::size_t m_pos = 0;
	std::size_t m_chunk;
};

/// \brief A streambuf that uses an array as storage
template <std::size_t SIZE, class CharT = char>
class array_streambuf : public std::basic_streambuf<CharT>
{
  public:
	using base = std::basic_streambuf<CharT>;
	using char_type = typename base::char_type;
	using int_type = typename base::int_type;

	array_streambuf()
	{
		base::setp(buffer_.data(),
			buffer_.data() +
				buffer_.size()); // set std::basic_streambuf
		                         // put area pointers to work with 'buffer_'
	}

	void reading() { base::setg(base::pbase(), base::pbase(), base::pptr()); }

	[[nodiscard]] std::size_t size() const
	{
		return static_cast<std::size_t>(base::pptr() - base::pbase());
	}

	[[nodiscard]] int_type overflow(int_type ch) override
	{
		return base::overflow(ch);
	}

  private:
	std::array<char_type, SIZE> buffer_{};
};

/// \brief Shared main for all test executables
///
/// Adds a --testdir command line option that overrides the automatic
/// detection of the test data directory.
inline int gxrio_test_main(int argc, char *argv[])
{
	Catch::Session session;

	std::string testdir;

	using namespace Catch::Clara;
	auto cli = session.cli()
		| Opt(testdir, "dir")["--testdir"]("Directory containing the test data files");

	session.cli(cli);

	if (int rc = session.applyCommandLine(argc, argv); rc != 0)
		return rc;

	if (not testdir.empty())
		gTestDir = testdir;
	else if (fs::current_path().filename().string() == "Debug" or fs::current_path().filename().string() == "Release")
		gTestDir = fs::current_path().parent_path().parent_path() / "test";
	else
		gTestDir = fs::current_path();

	gTestTempDir = make_test_temp_dir();

	int rc = session.run();

	std::error_code ec;
	fs::remove_all(gTestTempDir, ec);

	return rc;
}

#endif
