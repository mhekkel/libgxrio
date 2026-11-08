// Copyright Maarten L. Hekkelman, 2022
// SPDX-FileCopyrightText: 2026 Maarten L. Hekkelman
//
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <istream>
#include <memory>
#include <ostream>
#include <streambuf>
#include <utility>

#include <zlib.h>
#if HAVE_LibLZMA
#include <lzma.h>
#endif

/// \file gxrio.hpp
///
/// Single header file for the implementation of stream classes
/// that can transparently read and write compressed files.
///
/// The gxrio::istream_buf class sniffs the input and decides whether to use
/// a decompressor if a signature was recognized.
///
/// There's also an ifstream and ofstream class here that can
/// read and write compressed files. In this case the decision
/// whether to use a compressions/decompression algorithm is
/// based on the extension of the \a filename argument.

// NOLINTBEGIN(bugprone-use-after-move)

namespace gxrio
{

constexpr size_t kDefaultBufferSize = 8192;
constexpr int kDefaultCompressionLevel = 9;

/// \brief The longest signature of the supported compression formats (the xz magic)
constexpr std::streamsize kMaxSignatureLength = 6;

// --------------------------------------------------------------------

/// \brief A base class for the streambuf classes in gxrio
///
/// \tparam CharT Type of the character stream.
/// \tparam Traits Traits for character type, defaults to char_traits<_CharT>.
///
/// The base class for all streambuf classes in this library.
/// It maintains the pointer to the upstream streambuf.

template <typename CharT, typename Traits>
class basic_streambuf : public std::basic_streambuf<CharT, Traits>
{
  public:
	using char_type = CharT;
	using traits_type = Traits;

	using int_type = typename traits_type::int_type;
	using pos_type = typename traits_type::pos_type;
	using off_type = typename traits_type::off_type;

	using streambuf_type = std::basic_streambuf<CharT, Traits>;

	basic_streambuf() = default;

	basic_streambuf(const basic_streambuf &) = delete;

	basic_streambuf(basic_streambuf &&rhs) noexcept
		: streambuf_type(std::move(rhs))
	{
		m_upstream = std::exchange(rhs.m_upstream, nullptr);
	}

	basic_streambuf &operator=(const basic_streambuf &) = delete;

	basic_streambuf &operator=(basic_streambuf &&rhs) noexcept
	{
		m_upstream = std::exchange(rhs.m_upstream, nullptr);
		return *this;
	}

	void set_upstream(streambuf_type *upstream) noexcept
	{
		m_upstream = upstream;
	}

	virtual basic_streambuf *init(streambuf_type *sb) = 0;
	virtual basic_streambuf *close() = 0;

	/// \brief Set the compression level, only meaningful for the compressing streambufs
	virtual void set_compression_level(int /*level*/) noexcept {}

  protected:
	/// \brief The upstream streambuf object, usually this is a basic_filebuf
	streambuf_type *m_upstream = nullptr;
};

// --------------------------------------------------------------------

/// \brief A streambuf class that can be used to decompress gzipped data
///
/// \tparam CharT		Type of the character stream.
/// \tparam Traits		Traits for character type, defaults to char_traits<_CharT>.
/// \tparam BufferSize	The size of the internal buffers.
///
/// This implementation of streambuf can decompress (inflate) data compressed
/// using zlib.

template <typename CharT, typename Traits, size_t BufferSize = kDefaultBufferSize>
class basic_igzip_streambuf : public basic_streambuf<CharT, Traits>
{
  public:
	static_assert(sizeof(CharT) == 1, "Unfortunately, support for wide characters is not implemented yet.");

	using char_type = CharT;
	using traits_type = Traits;

	using streambuf_type = std::basic_streambuf<char_type, traits_type>;
	using base_type = basic_streambuf<CharT, Traits>;

	using int_type = typename traits_type::int_type;
	using pos_type = typename traits_type::pos_type;
	using off_type = typename traits_type::off_type;

	basic_igzip_streambuf() = default;

	basic_igzip_streambuf(const basic_igzip_streambuf &) = delete;

	/// \brief Move constructor
	basic_igzip_streambuf(basic_igzip_streambuf &&rhs) noexcept
		: base_type(std::move(rhs))
	{
		std::swap(m_zstream, rhs.m_zstream);
		std::swap(m_gzheader, rhs.m_gzheader);

		auto p = std::copy(rhs.gptr(), rhs.egptr(), m_out_buffer.data());
		this->setg(m_out_buffer.data(), m_out_buffer.data(), p);

		if (m_zstream and m_zstream->avail_in > 0)
		{
			auto next_in_offset = m_zstream->next_in - reinterpret_cast<unsigned char *>(rhs.m_in_buffer.data());
			std::copy(rhs.m_in_buffer.data() + next_in_offset,
				rhs.m_in_buffer.data() + next_in_offset + m_zstream->avail_in,
				m_in_buffer.data());
			m_zstream->next_in = reinterpret_cast<unsigned char *>(m_in_buffer.data());
		}
	}

	basic_igzip_streambuf &operator=(const basic_igzip_streambuf &) = delete;

	/// \brief Move operator= implementation
	basic_igzip_streambuf &operator=(basic_igzip_streambuf &&rhs) noexcept
	{
		base_type::operator=(std::move(rhs));

		std::swap(m_zstream, rhs.m_zstream);
		std::swap(m_gzheader, rhs.m_gzheader);

		auto p = std::copy(rhs.gptr(), rhs.egptr(), m_out_buffer.data());
		this->setg(m_out_buffer.data(), m_out_buffer.data(), p);

		if (m_zstream and m_zstream->avail_in > 0)
		{
			auto next_in_offset = m_zstream->next_in - reinterpret_cast<unsigned char *>(rhs.m_in_buffer.data());
			std::copy(rhs.m_in_buffer.data() + next_in_offset,
				rhs.m_in_buffer.data() + next_in_offset + m_zstream->avail_in,
				m_in_buffer.data());
			m_zstream->next_in = reinterpret_cast<unsigned char *>(m_in_buffer.data());
		}

		return *this;
	}

	~basic_igzip_streambuf()
	{
		close();
	}

	/// \brief This closes the zlib stream and sets the get pointers to null.
	base_type *close() noexcept override
	{
		if (m_zstream)
		{
			::inflateEnd(m_zstream.get());

			m_zstream.reset(nullptr);
			m_gzheader.reset(nullptr);
		}

		this->setg(nullptr, nullptr, nullptr);

		return this;
	}

	/// \brief Initialize a zlib stream and set the upstream.
	///
	/// \param upstream The upstream streambuf
	///
	/// The zstream is constructed and an optional header is
	/// read from upstream. The contents of the header are ignored
	/// but we must maintain that structure.
	base_type *init(streambuf_type *upstream) override
	{
		this->set_upstream(upstream);

		close();

		m_zstream.reset(new z_stream_s);
		m_gzheader.reset(new gz_header_s);

		auto &zstream = *m_zstream.get();
		zstream = z_stream_s{};
		auto &header = *m_gzheader.get();
		header = gz_header_s{};

		int err = ::inflateInit2(&zstream, 47);
		if (err == Z_OK)
		{
			zstream.next_in = reinterpret_cast<unsigned char *>(m_in_buffer.data());
			zstream.avail_in = static_cast<uInt>(this->m_upstream->sgetn(m_in_buffer.data(), m_in_buffer.size()));

			err = ::inflateGetHeader(&zstream, &header);

			if (err != Z_OK)
				::inflateEnd(&zstream);
		}

		if (err != Z_OK)
			zstream = z_stream_s{};

		return err == Z_OK ? this : nullptr;
	}

  private:
	/// \brief The actual work is done here.
	int_type underflow() override
	{
		if (m_zstream and this->m_upstream)
		{
			auto &zstream = *m_zstream.get();
			const std::streamsize kBufferByteSize = m_out_buffer.size();

			while (this->gptr() == this->egptr())
			{
				zstream.next_out = reinterpret_cast<unsigned char *>(m_out_buffer.data());
				zstream.avail_out = static_cast<uInt>(kBufferByteSize);

				if (zstream.avail_in == 0)
				{
					zstream.next_in = reinterpret_cast<unsigned char *>(m_in_buffer.data());
					zstream.avail_in = static_cast<uInt>(this->m_upstream->sgetn(m_in_buffer.data(), m_in_buffer.size()));
				}

				if (zstream.avail_in == 0)
					break;

				int err = ::inflate(&zstream, Z_SYNC_FLUSH);
				std::streamsize n = kBufferByteSize - zstream.avail_out;

				if (n > 0)
				{
					this->setg(
						m_out_buffer.data(),
						m_out_buffer.data(),
						m_out_buffer.data() + n);
					break;
				}

				if (err == Z_STREAM_END and zstream.avail_in > 0)
					err = ::inflateReset2(&zstream, 47);

				if (err < Z_OK)
					break;
			}
		}

		return this->gptr() != this->egptr() ? traits_type::to_int_type(*this->gptr()) : traits_type::eof();
	}

  private:
	/// \brief The zlib internal structures are maintained as pointers to avoid having
	/// to copy their content in move constructors.
	std::unique_ptr<z_stream_s> m_zstream;

	/// \brief The zlib internal structures are maintained as pointers to avoid having
	/// to copy their content in move constructors.
	std::unique_ptr<gz_header> m_gzheader;

	/// \brief Input buffer, this is the input for zlib
	std::array<char_type, BufferSize> m_in_buffer;

	/// \brief Output buffer, where the ostream finds the data
	std::array<char_type, BufferSize> m_out_buffer;
};

// --------------------------------------------------------------------

/// \brief A streambuf class that can be used to compress data using zlib
///
/// \tparam CharT		Type of the character stream.
/// \tparam Traits		Traits for character type, defaults to char_traits<_CharT>.
/// \tparam BufferSize	The size of the internal buffers.
///
/// This implementation of streambuf can compress (deflate) data using zlib.

template <typename CharT, typename Traits, size_t BufferSize = kDefaultBufferSize>
class basic_ogzip_streambuf : public basic_streambuf<CharT, Traits>
{
  public:
	static_assert(sizeof(CharT) == 1, "Unfortunately, support for wide characters is not implemented yet.");

	using char_type = CharT;
	using traits_type = Traits;

	using streambuf_type = std::basic_streambuf<char_type, traits_type>;
	using base_type = basic_streambuf<CharT, Traits>;

	using int_type = typename traits_type::int_type;
	using pos_type = typename traits_type::pos_type;
	using off_type = typename traits_type::off_type;

	basic_ogzip_streambuf() = default;

	basic_ogzip_streambuf(const basic_ogzip_streambuf &) = delete;

	/// \brief Move constructor
	basic_ogzip_streambuf(basic_ogzip_streambuf &&rhs) noexcept
		: base_type(std::move(rhs))
	{
		std::swap(m_zstream, rhs.m_zstream);
		std::swap(m_gzheader, rhs.m_gzheader);

		m_compression_level = rhs.m_compression_level;
		m_error = rhs.m_error;

		this->setp(m_in_buffer.data(), m_in_buffer.data() + m_in_buffer.size());
		this->sputn(rhs.pbase(), rhs.pptr() - rhs.pbase());
		rhs.setp(nullptr, nullptr);
	}

	basic_ogzip_streambuf &operator=(const basic_ogzip_streambuf &) = delete;

	/// \brief Move operator=
	basic_ogzip_streambuf &operator=(basic_ogzip_streambuf &&rhs) noexcept
	{
		base_type::operator=(std::move(rhs));

		std::swap(m_zstream, rhs.m_zstream);
		std::swap(m_gzheader, rhs.m_gzheader);

		m_compression_level = rhs.m_compression_level;
		m_error = rhs.m_error;

		this->setp(m_in_buffer.data(), m_in_buffer.data() + m_in_buffer.size());
		this->sputn(rhs.pbase(), rhs.pptr() - rhs.pbase());
		rhs.setp(nullptr, nullptr);

		return *this;
	}

	~basic_ogzip_streambuf()
	{
		close();
	}

	/// \brief This closes the zlib stream and sets the put pointers to null.
	///
	/// Returns nullptr when flushing the remaining data failed, so that
	/// callers can propagate the error.
	base_type *close() override
	{
		if (m_zstream)
		{
			overflow(traits_type::eof());

			::deflateEnd(m_zstream.get());

			m_zstream.reset(nullptr);
			m_gzheader.reset(nullptr);
		}

		this->setp(nullptr, nullptr);

		return m_error ? nullptr : this;
	}

	/// \brief Set the compression level, 0 (no compression) to 9 (maximum compression)
	///
	/// When the stream is already initialized the new level is applied
	/// immediately using deflateParams().
	void set_compression_level(int level) noexcept override
	{
		m_compression_level = level;

		if (m_zstream)
			::deflateParams(m_zstream.get(), level, Z_DEFAULT_STRATEGY);
	}

	/// \brief Initialize the internal zlib structures
	///
	/// \param upstream The upstream streambuf
	///
	/// The zlib stream is initialized as one that can accept
	/// a gzip header.
	base_type *init(streambuf_type *upstream) override
	{
		this->set_upstream(upstream);

		m_error = false;

		close();

		m_zstream.reset(new z_stream_s);
		m_gzheader.reset(new gz_header_s);

		auto &zstream = *m_zstream.get();
		zstream = z_stream_s{};
		auto &header = *m_gzheader.get();
		header = gz_header_s{};

		const int WINDOW_BITS = 15, GZIP_ENCODING = 16;

		int err = deflateInit2(&zstream, m_compression_level, Z_DEFLATED,
			WINDOW_BITS | GZIP_ENCODING, Z_DEFLATED, Z_DEFAULT_STRATEGY);

		if (err == Z_OK)
			err = ::deflateSetHeader(&zstream, &header);

		if (err == Z_OK)
			this->setp(this->m_in_buffer.data(), this->m_in_buffer.data() + this->m_in_buffer.size());
		else
			zstream = z_stream_s{};

		return err == Z_OK ? this : nullptr;
	}

  private:
	/// \brief The actual work is done here
	///
	/// \param ch The character that did not fit, in case it is eof we need to flush
	///
	int_type overflow(int_type ch) override
	{
		if (not m_zstream)
			return traits_type::eof();

		auto &zstream = *m_zstream;

		zstream.next_in = reinterpret_cast<unsigned char *>(this->pbase());
		zstream.avail_in = static_cast<uInt>(this->pptr() - this->pbase());

		char_type buffer[BufferSize];

		for (;;)
		{
			zstream.next_out = reinterpret_cast<unsigned char *>(buffer);
			zstream.avail_out = sizeof(buffer);

			int err = ::deflate(&zstream, ch == traits_type::eof() ? Z_FINISH : Z_NO_FLUSH);

			std::streamsize n = sizeof(buffer) - zstream.avail_out;
			if (n > 0)
			{
				auto r = this->m_upstream->sputn(reinterpret_cast<char_type *>(buffer), n);

				if (r != n)
				{
					m_error = true;
					return traits_type::eof();
				}
			}

			if (err == Z_OK)
			{
				if (zstream.avail_out == 0)
					continue;

				if (ch == traits_type::eof())
					continue;
			}
			else if (err != Z_STREAM_END)
				m_error = true;

			break;
		}

		this->setp(this->m_in_buffer.data(), this->m_in_buffer.data() + this->m_in_buffer.size());

		if (not traits_type::eq_int_type(ch, traits_type::eof()))
		{
			*this->pptr() = traits_type::to_char_type(ch);
			this->pbump(1);
		}

		return ch;
	}

  private:
	/// \brief The zlib internal structures are maintained as pointers to avoid having
	/// to copy their content in move constructors.
	std::unique_ptr<z_stream_s> m_zstream;

	/// \brief The zlib internal structures are maintained as pointers to avoid having
	/// to copy their content in move constructors.
	std::unique_ptr<gz_header> m_gzheader;

	/// \brief The compression level used, 0 (none) to 9 (maximum)
	int m_compression_level = kDefaultCompressionLevel;

	/// \brief Set when a write to the upstream failed, so close() can report the error
	bool m_error = false;

	/// \brief Input buffer, this is the input for zlib
	std::array<char_type, BufferSize> m_in_buffer;
};

// --------------------------------------------------------------------
#if HAVE_LibLZMA

/// \brief A streambuf class that can be used to decompress xz data
///
/// \tparam CharT		Type of the character stream.
/// \tparam Traits		Traits for character type, defaults to char_traits<_CharT>.
/// \tparam BufferSize	The size of the internal buffers.
///
/// This implementation of streambuf can decompress data compressed
/// using xz or lzma.

template <typename CharT, typename Traits, size_t BufferSize = kDefaultBufferSize, std::enable_if_t<sizeof(CharT) == 1, int> = 0>
class basic_ixz_streambuf : public basic_streambuf<CharT, Traits>
{
  public:
	using char_type = CharT;
	using traits_type = Traits;

	using streambuf_type = std::basic_streambuf<char_type, traits_type>;
	using base_type = basic_streambuf<CharT, Traits>;

	using int_type = typename traits_type::int_type;
	using pos_type = typename traits_type::pos_type;
	using off_type = typename traits_type::off_type;

	basic_ixz_streambuf() = default;

	basic_ixz_streambuf(const basic_ixz_streambuf &) = delete;

	/// \brief Move constructor
	basic_ixz_streambuf(basic_ixz_streambuf &&rhs) noexcept
		: base_type(std::move(rhs))
	{
		std::swap(m_xzstream, rhs.m_xzstream);

		auto p = std::copy(rhs.gptr(), rhs.egptr(), m_out_buffer.data());
		this->setg(m_out_buffer.data(), m_out_buffer.data(), p);

		if (m_xzstream and m_xzstream->avail_in > 0)
		{
			auto next_in_offset = m_xzstream->next_in - reinterpret_cast<const unsigned char *>(rhs.m_in_buffer.data());
			std::copy(rhs.m_in_buffer.data() + next_in_offset,
				rhs.m_in_buffer.data() + next_in_offset + m_xzstream->avail_in,
				m_in_buffer.data());
			m_xzstream->next_in = reinterpret_cast<const unsigned char *>(m_in_buffer.data());
		}
	}

	basic_ixz_streambuf &operator=(const basic_ixz_streambuf &) = delete;

	/// \brief Move operator= implementation
	basic_ixz_streambuf &operator=(basic_ixz_streambuf &&rhs) noexcept
	{
		base_type::operator=(std::move(rhs));
		std::swap(m_xzstream, rhs.m_xzstream);

		auto p = std::copy(rhs.gptr(), rhs.egptr(), m_out_buffer.data());
		this->setg(m_out_buffer.data(), m_out_buffer.data(), p);

		if (m_xzstream and m_xzstream->avail_in > 0)
		{
			auto next_in_offset = m_xzstream->next_in - reinterpret_cast<unsigned char *>(rhs.m_in_buffer.data());
			std::copy(rhs.m_in_buffer.data() + next_in_offset,
				rhs.m_in_buffer.data() + next_in_offset + m_xzstream->avail_in,
				m_in_buffer.data());
			m_xzstream->next_in = reinterpret_cast<unsigned char *>(m_in_buffer.data());
		}

		return *this;
	}

	~basic_ixz_streambuf()
	{
		close();
	}

	/// \brief This closes the xz stream and sets the get pointers to null.
	base_type *close() noexcept override
	{
		if (m_xzstream)
		{
			::lzma_end(m_xzstream.get());

			m_xzstream.reset(nullptr);
		}

		this->setg(nullptr, nullptr, nullptr);

		return this;
	}

	/// \brief Initialize a xz stream and set the upstream.
	///
	/// \param upstream The upstream streambuf
	///
	/// The zstream is constructed and an optional header is
	/// read from upstream. The contents of the header are ignored
	/// but we must maintain that structure.
	base_type *init(streambuf_type *upstream) override
	{
		this->set_upstream(upstream);

		close();

		m_xzstream.reset(new lzma_stream);

		auto &xzstream = *m_xzstream.get();
		xzstream = LZMA_STREAM_INIT;

		int err = lzma_stream_decoder(&xzstream, UINT64_MAX, LZMA_TELL_NO_CHECK);

		return err == LZMA_OK ? this : nullptr;
	}

  private:
	/// \brief The actual work is done here.
	int_type underflow() override
	{
		if (m_xzstream and this->m_upstream)
		{
			auto &zstream = *m_xzstream.get();
			const std::streamsize kBufferByteSize = m_out_buffer.size();

			while (this->gptr() == this->egptr())
			{
				zstream.next_out = reinterpret_cast<unsigned char *>(m_out_buffer.data());
				zstream.avail_out = kBufferByteSize;

				bool at_eof = false;
				if (zstream.avail_in == 0)
				{
					zstream.next_in = reinterpret_cast<unsigned char *>(m_in_buffer.data());
					zstream.avail_in = this->m_upstream->sgetn(m_in_buffer.data(), m_in_buffer.size());
					at_eof = zstream.avail_in == 0;
				}

				int err = ::lzma_code(&zstream, LZMA_RUN);
				std::streamsize n = kBufferByteSize - zstream.avail_out;

				if (err == LZMA_STREAM_END or (err == LZMA_OK and n > 0))
				{
					this->setg(
						m_out_buffer.data(),
						m_out_buffer.data(),
						m_out_buffer.data() + n);
					break;
				}

				if (err != LZMA_OK or at_eof)
					break;
			}
		}

		return this->gptr() != this->egptr() ? traits_type::to_int_type(*this->gptr()) : traits_type::eof();
	}

  private:
	/// \brief The lzma internal structures are maintained as pointers to avoid having
	/// to copy their content in move constructors.
	std::unique_ptr<lzma_stream> m_xzstream;

	/// \brief Input buffer, this is the input for xz
	std::array<char_type, BufferSize> m_in_buffer;

	/// \brief Output buffer, where the ostream finds the data
	std::array<char_type, BufferSize> m_out_buffer;
};

// --------------------------------------------------------------------

/// \brief A streambuf class that can be used to compress data using xz
///
/// \tparam CharT		Type of the character stream.
/// \tparam Traits		Traits for character type, defaults to char_traits<_CharT>.
/// \tparam BufferSize	The size of the internal buffers.
///
/// This implementation of streambuf can compress (deflate) data using xz.

template <typename CharT, typename Traits, size_t BufferSize = kDefaultBufferSize, std::enable_if_t<sizeof(CharT) == 1, int> = 0>
class basic_oxz_streambuf : public basic_streambuf<CharT, Traits>
{
  public:
	using char_type = CharT;
	using traits_type = Traits;

	using streambuf_type = std::basic_streambuf<char_type, traits_type>;
	using base_type = basic_streambuf<CharT, Traits>;

	using int_type = typename traits_type::int_type;
	using pos_type = typename traits_type::pos_type;
	using off_type = typename traits_type::off_type;

	basic_oxz_streambuf() = default;

	basic_oxz_streambuf(const basic_oxz_streambuf &) = delete;

	/// \brief Move constructor
	basic_oxz_streambuf(basic_oxz_streambuf &&rhs) noexcept
		: base_type(std::move(rhs))
	{
		std::swap(m_xzstream, rhs.m_xzstream);

		m_compression_level = rhs.m_compression_level;
		m_error = rhs.m_error;

		this->setp(m_in_buffer.data(), m_in_buffer.data() + m_in_buffer.size());
		this->sputn(rhs.pbase(), rhs.pptr() - rhs.pbase());
		rhs.setp(nullptr, nullptr);
	}

	basic_oxz_streambuf &operator=(const basic_oxz_streambuf &) = delete;

	/// \brief Move operator=
	basic_oxz_streambuf &operator=(basic_oxz_streambuf &&rhs) noexcept
	{
		base_type::operator=(std::move(rhs));

		std::swap(m_xzstream, rhs.m_xzstream);

		m_compression_level = rhs.m_compression_level;
		m_error = rhs.m_error;

		this->setp(m_in_buffer.data(), m_in_buffer.data() + m_in_buffer.size());
		this->sputn(rhs.pbase(), rhs.pptr() - rhs.pbase());
		rhs.setp(nullptr, nullptr);

		return *this;
	}

	~basic_oxz_streambuf()
	{
		close();
	}

	/// \brief This closes the xz stream and sets the put pointers to null.
	///
	/// Returns nullptr when flushing the remaining data failed, so that
	/// callers can propagate the error.
	base_type *close() override
	{
		if (m_xzstream)
		{
			overflow(traits_type::eof());

			::lzma_end(m_xzstream.get());

			m_xzstream.reset(nullptr);
		}

		this->setp(nullptr, nullptr);

		return m_error ? nullptr : this;
	}

	/// \brief Set the compression level, 0 (no compression) to 9 (maximum compression)
	void set_compression_level(int level) noexcept override
	{
		m_compression_level = level;
	}

	/// \brief Initialize the internal lzma structures
	///
	/// \param upstream The upstream streambuf
	///
	/// The zlib stream is initialized as one that can accept
	/// a gzip header.
	base_type *init(streambuf_type *upstream) override
	{
		this->set_upstream(upstream);

		m_error = false;

		close();

		m_xzstream.reset(new lzma_stream);

		auto &zstream = *m_xzstream.get();
		zstream = LZMA_STREAM_INIT;

		int err = lzma_easy_encoder(&zstream, m_compression_level, LZMA_CHECK_CRC64);

		if (err == LZMA_OK)
			this->setp(this->m_in_buffer.data(), this->m_in_buffer.data() + this->m_in_buffer.size());

		return err == LZMA_OK ? this : nullptr;
	}

  private:
	/// \brief The actual work is done here
	///
	/// \param ch The character that did not fit, in case it is eof we need to flush
	///
	int_type overflow(int_type ch) override
	{
		if (not m_xzstream)
			return traits_type::eof();

		auto &zstream = *m_xzstream;

		zstream.next_in = reinterpret_cast<unsigned char *>(this->pbase());
		zstream.avail_in = this->pptr() - this->pbase();

		char_type buffer[BufferSize];

		for (;;)
		{
			zstream.next_out = reinterpret_cast<unsigned char *>(buffer);
			zstream.avail_out = sizeof(buffer);

			int err = ::lzma_code(&zstream, ch == traits_type::eof() ? LZMA_FINISH : LZMA_RUN);

			std::streamsize n = sizeof(buffer) - zstream.avail_out;
			if (n > 0)
			{
				auto r = this->m_upstream->sputn(reinterpret_cast<char_type *>(buffer), n);

				if (r != n)
				{
					m_error = true;
					return traits_type::eof();
				}
			}

			if (err == LZMA_OK)
			{
				if (zstream.avail_out == 0)
					continue;

				if (ch == traits_type::eof())
					continue;
			}
			else if (err != LZMA_STREAM_END)
				m_error = true;

			break;
		}

		this->setp(this->m_in_buffer.data(), this->m_in_buffer.data() + this->m_in_buffer.size());

		if (not traits_type::eq_int_type(ch, traits_type::eof()))
		{
			*this->pptr() = traits_type::to_char_type(ch);
			this->pbump(1);
		}

		return ch;
	}

  private:
	/// \brief The lzma internal structures are maintained as pointers to avoid having
	/// to copy their content in move constructors.
	std::unique_ptr<lzma_stream> m_xzstream;

	/// \brief The compression level used, 0 (none) to 9 (maximum)
	int m_compression_level = kDefaultCompressionLevel;

	/// \brief Set when a write to the upstream failed, so close() can report the error
	bool m_error = false;

	/// \brief Input buffer, this is the input for lzma
	std::array<char_type, BufferSize> m_in_buffer;
};

#endif

// --------------------------------------------------------------------

/// \brief A streambuf that replays a number of bytes before delegating to another streambuf
///
/// \tparam CharT		Type of the character stream.
/// \tparam Traits		Traits for character type, defaults to char_traits<_CharT>.
///
/// This streambuf is used while sniffing the format of an input stream. The signature
/// bytes that were already read from the stream are played back from an internal buffer
/// before the actual upstream streambuf is consulted. This avoids the need to put back
/// bytes into the upstream streambuf, which not every streambuf implementation supports.

template <typename CharT, typename Traits>
class basic_prefix_streambuf : public std::basic_streambuf<CharT, Traits>
{
  public:
	using char_type = CharT;
	using traits_type = Traits;

	using int_type = typename traits_type::int_type;
	using streambuf_type = std::basic_streambuf<CharT, Traits>;

	basic_prefix_streambuf(streambuf_type *upstream, const char_type *prefix, std::streamsize length) noexcept
		: m_upstream(upstream)
	{
		std::copy(prefix, prefix + length, m_prefix.begin());
		this->setg(m_prefix.data(), m_prefix.data(), m_prefix.data() + length);
	}

  protected:
	int_type underflow() override
	{
		if (this->gptr() < this->egptr())
			return traits_type::to_int_type(*this->gptr());

		if (m_upstream == nullptr)
			return traits_type::eof();

		std::streamsize n = m_upstream->sgetn(m_buffer.data(), m_buffer.size());

		if (n == 0)
			return traits_type::eof();

		this->setg(m_buffer.data(), m_buffer.data(), m_buffer.data() + n);

		return traits_type::to_int_type(*this->gptr());
	}

  private:
	/// \brief The streambuf we read from once the prefix is exhausted
	streambuf_type *m_upstream;

	/// \brief The signature bytes that were already consumed from the upstream
	std::array<char_type, kMaxSignatureLength> m_prefix;

	/// \brief A buffer to read the remaining data into
	std::array<char_type, kDefaultBufferSize> m_buffer;
};

// --------------------------------------------------------------------

/// \brief An istream implementation that wraps a streambuf with a decompressing streambuf
///
/// \tparam CharT		Type of the character stream.
/// \tparam Traits		Traits for character type, defaults to char_traits<_CharT>.
///
/// This is an istream implementation that can take a source streambuf and then wraps
/// this streambuf with a decompressing streambuf class defined above.
/// The class inherits from std::basic_istream and offers all the associated functionality.

template <typename CharT, typename Traits>
class basic_istream : public std::basic_istream<CharT, Traits>
{
  public:
	using base_type = std::basic_istream<CharT, Traits>;

	using traits_type = Traits;
	using char_type = typename traits_type::char_type;
	using int_type = typename traits_type::int_type;

	using z_streambuf_type = basic_streambuf<char_type, traits_type>;
	using upstreambuf_type = std::basic_streambuf<char_type, traits_type>;

	using gzip_streambuf_type = basic_igzip_streambuf<char_type, traits_type>;
#if HAVE_LibLZMA
	using xz_streambuf_type = basic_ixz_streambuf<char_type, traits_type>;
#endif
	using prefix_streambuf_type = basic_prefix_streambuf<char_type, traits_type>;

	/// \brief Regular move constructor
	basic_istream(basic_istream &&rhs) noexcept
		: base_type(std::move(rhs))
	{
		m_gxriobuf = std::move(rhs.m_gxriobuf);
		m_upstream_buf = std::move(rhs.m_upstream_buf);
		m_upstream = rhs.m_upstream;

		this->rdbuf(m_gxriobuf ? m_gxriobuf.get() : m_upstream);
	}

	/// \brief Regular move operator=
	basic_istream &operator=(basic_istream &&rhs) noexcept
	{
		base_type::operator=(std::move(rhs));
		m_gxriobuf = std::move(rhs.m_gxriobuf);
		m_upstream_buf = std::move(rhs.m_upstream_buf);
		m_upstream = rhs.m_upstream;

		this->rdbuf(m_gxriobuf ? m_gxriobuf.get() : m_upstream);

		return *this;
	}

	/// \brief Construct an istream with the passed in streambuf \a buf
	///
	/// \param buf The streambuf that provides the compressed data
	///
	/// This constructor will initialize the zlib code with the \a buf streambuf.

	explicit basic_istream(upstreambuf_type *buf)
		: base_type(nullptr)
	{
		init_z(buf);
	}

  protected:
	basic_istream()
		: base_type(nullptr) {}

	/// \brief Initialise internals with streambuf \a sb
	/// \param sb The upstream streambuf class
	///
	/// This will sniff the content in \a sb and decide upon what is found
	/// what implementation is used. If it doesn't look like compressed data
	/// the \a sb streambuf is used without any decompression being done.
	///
	/// The signature bytes that are consumed while sniffing are replayed to
	/// the reader through a basic_prefix_streambuf, so the upstream does not
	/// need to support putting characters back.

	void init_z(upstreambuf_type *sb)
	{
		if (sb == nullptr)
		{
			this->setstate(std::ios_base::failbit);
			return;
		}

		m_gxriobuf.reset(nullptr);
		m_upstream_buf.reset(nullptr);
		m_upstream = nullptr;

		char_type signature[kMaxSignatureLength];
		std::streamsize signature_length = 0;

		int_type ch = sb->sgetc();
		if (ch == 0x1f)
		{
			signature[signature_length++] = traits_type::to_char_type(ch);
			sb->sbumpc();
			ch = sb->sgetc();

			if (ch == 0x8b) // gzip magic
			{
				signature[signature_length++] = traits_type::to_char_type(ch);
				sb->sbumpc();
				ch = sb->sgetc();

				if (ch == 0x08) // compression method must be deflate
				{
					signature[signature_length++] = traits_type::to_char_type(ch);
					sb->sbumpc();
					m_gxriobuf.reset(new gzip_streambuf_type);
				}
			}
		}
#if HAVE_LibLZMA
		else if (ch == 0xfd)
		{
			// xz magic: 0xfd '7' 'z' 'X' 'Z' 0x00
			constexpr std::streamsize kXZMagicLength = 6;
			constexpr int kXZMagic[kXZMagicLength] = { 0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00 };

			int_type next = ch;
			for (std::streamsize i = 0; i < kXZMagicLength and next == kXZMagic[i]; ++i)
			{
				signature[signature_length++] = traits_type::to_char_type(next);
				sb->sbumpc();
				next = sb->sgetc();
			}

			if (signature_length == kXZMagicLength)
				m_gxriobuf.reset(new xz_streambuf_type);
		}
#endif

		if (m_gxriobuf)
		{
			m_upstream_buf.reset(new prefix_streambuf_type(sb, signature, signature_length));
			m_upstream = m_upstream_buf.get();

			if (not m_gxriobuf->init(m_upstream))
				this->setstate(std::ios_base::failbit);
			else
				this->init(m_gxriobuf.get());
		}
		else if (signature_length > 0)
		{
			m_upstream_buf.reset(new prefix_streambuf_type(sb, signature, signature_length));
			m_upstream = m_upstream_buf.get();
			this->init(m_upstream);
		}
		else
		{
			m_upstream = sb;
			this->init(m_upstream);
		}
	}

  protected:
	/// \brief Our streambuf class
	std::unique_ptr<z_streambuf_type> m_gxriobuf;

	/// \brief Replays the sniffed signature bytes, only used when a basic_istream
	/// is constructed directly with an upstream streambuf.
	std::unique_ptr<upstreambuf_type> m_upstream_buf;

	/// \brief The streambuf that provides the data to this stream, either the
	/// upstream directly or the prefix streambuf wrapping it.
	upstreambuf_type *m_upstream = nullptr;
};

// --------------------------------------------------------------------

/// \brief Control input from files compressed with gzip.
///
/// \tparam CharT		Type of the character stream.
/// \tparam Traits		Traits for character type, defaults to char_traits<_CharT>.
///
/// This is an ifstream implementation that can read from named files compressed with
/// gzip directly. The class inherits from std::basic_istream and offers all the
/// associated functionality.

template <typename CharT, typename Traits>
class basic_ifstream : public basic_istream<CharT, Traits>
{
  public:
	using base_type = basic_istream<CharT, Traits>;

	using char_type = CharT;
	using traits_type = Traits;

	using filebuf_type = std::basic_filebuf<char_type, traits_type>;

	using gzip_streambuf_type = typename base_type::gzip_streambuf_type;
#if HAVE_LibLZMA
	using xz_streambuf_type = typename base_type::xz_streambuf_type;
#endif

	/// \brief Default constructor, does not open a file since none is specified
	basic_ifstream() = default;

	~basic_ifstream()
	{
		if (is_open())
			close();
	}

	/// \brief Construct an ifstream
	/// \param filename Null terminated string specifying the file to open
	/// \param mode The mode in which to open the file

	explicit basic_ifstream(const char *filename, std::ios_base::openmode mode = std::ios_base::in)
	{
		open(filename, mode);
	}

	/// \brief Construct an ifstream
	/// \param filename std::string specifying the file to open
	/// \param mode The mode in which to open the file

	explicit basic_ifstream(const std::string &filename, std::ios_base::openmode mode = std::ios_base::in)
	{
		open(filename, mode);
	}

	/// \brief Construct an ifstream
	/// \param filename std::filesystem::path specifying the file to open
	/// \param mode The mode in which to open the file

	explicit basic_ifstream(const std::filesystem::path &filename, std::ios_base::openmode mode = std::ios_base::in)
	{
		open(filename, mode);
	}

	/// \brief Move constructor
	basic_ifstream(basic_ifstream &&rhs) noexcept
		: base_type(std::move(rhs))
	{
		m_filebuf = std::move(rhs.m_filebuf);
		this->m_upstream = &m_filebuf;

		if (this->m_gxriobuf)
			this->m_gxriobuf->set_upstream(&m_filebuf);
		else
			this->rdbuf(&m_filebuf);
	}

	basic_ifstream(const basic_ifstream &) = delete;

	basic_ifstream &operator=(const basic_ifstream &) = delete;

	/// \brief Move version of operator=
	basic_ifstream &operator=(basic_ifstream &&rhs) noexcept
	{
		base_type::operator=(std::move(rhs));

		m_filebuf = std::move(rhs.m_filebuf);
		this->m_upstream = &m_filebuf;

		if (this->m_gxriobuf)
			this->m_gxriobuf->set_upstream(&m_filebuf);
		else
			this->rdbuf(&m_filebuf);

		return *this;
	}

	/// \brief Open the file \a filename with mode \a mode
	/// \param filename std::filesystem::path specifying the file to open
	/// \param mode The mode in which to open the file

	void open(const std::filesystem::path &filename, std::ios_base::openmode mode = std::ios_base::in)
	{
		if (not m_filebuf.open(filename, mode | std::ios::binary))
			this->setstate(std::ios_base::failbit);
		else
		{
			if (filename.extension() == ".gz")
				this->m_gxriobuf.reset(new gzip_streambuf_type);
#if HAVE_LibLZMA
			else if (filename.extension() == ".xz")
				this->m_gxriobuf.reset(new xz_streambuf_type);
#endif
			else
				this->m_gxriobuf.reset(nullptr);

			this->m_upstream = &m_filebuf;

			if (not this->m_gxriobuf)
			{
				this->rdbuf(&m_filebuf);
				this->clear();
			}
			else if (not this->m_gxriobuf->init(&m_filebuf))
				this->setstate(std::ios_base::failbit);
			else
			{
				this->rdbuf(this->m_gxriobuf.get());
				this->clear();
			}
		}
	}

	/// \brief Open the file \a filename with mode \a mode
	/// \param filename std::string specifying the file to open
	/// \param mode The mode in which to open the file

	void open(const std::string &filename, std::ios_base::openmode mode = std::ios_base::in)
	{
		open(std::filesystem::path{filename}, mode);
	}

	/// \brief Open the file \a filename with mode \a mode
	/// \param filename Null terminated string specifying the file to open
	/// \param mode The mode in which to open the file

	void open(const char *filename, std::ios_base::openmode mode = std::ios_base::in)
	{
		open(std::filesystem::path{filename}, mode);
	}

	/// \brief Return true if the file is open
	/// \return m_filebuf.is_open()

	bool is_open() const noexcept
	{
		return m_filebuf.is_open();
	}

	/// \brief Close the file
	///
	/// Calls m_filebuf.close(). If that fails, the failbit is set.

	void close()
	{
		if (this->m_gxriobuf and not this->m_gxriobuf->close())
			this->setstate(std::ios_base::failbit);

		if (not m_filebuf.close())
			this->setstate(std::ios_base::failbit);
	}

	/// \brief Swap the contents with those of \a rhs
	/// \param rhs The ifstream to swap with

	void swap(basic_ifstream &rhs) noexcept
	{
		base_type::swap(rhs);
		m_filebuf.swap(rhs.m_filebuf);
		std::swap(this->m_gxriobuf, rhs.m_gxriobuf);

		if (this->m_gxriobuf)
		{
			this->m_gxriobuf->set_upstream(&m_filebuf);
			this->rdbuf(this->m_gxriobuf.get());
		}
		else
			this->rdbuf(&m_filebuf);

		this->m_upstream = &m_filebuf;

		if (rhs.m_gxriobuf)
		{
			rhs.m_gxriobuf->set_upstream(&rhs.m_filebuf);
			rhs.rdbuf(rhs.m_gxriobuf.get());
		}
		else
			rhs.rdbuf(&rhs.m_filebuf);

		rhs.m_upstream = &rhs.m_filebuf;
	}

  private:
	/// \brief The filebuf
	filebuf_type m_filebuf;
};

// --------------------------------------------------------------------

/// \brief An ostream implementation that wraps a streambuf with a compressing streambuf
///
/// \tparam CharT		Type of the character stream.
/// \tparam Traits		Traits for character type, defaults to char_traits<_CharT>.
///
/// This is an ostream implementation that can take an upstream streambuf and then wraps
/// this streambuf with a compressing streambuf class defined above.
/// The class inherits from std::basic_ostream and offers all the associated functionality.

template <typename CharT, typename Traits>
class basic_ostream : public std::basic_ostream<CharT, Traits>
{
  public:
	using base_type = std::basic_ostream<CharT, Traits>;

	using char_type = CharT;
	using traits_type = Traits;

	using z_streambuf_type = basic_streambuf<char_type, traits_type>;
	using upstreambuf_type = std::basic_streambuf<char_type, traits_type>;

	/// \brief Regular move constructor
	basic_ostream(basic_ostream &&rhs) noexcept
		: base_type(std::move(rhs))
	{
		m_gxriobuf = std::move(rhs.m_gxriobuf);
		m_upstream = rhs.m_upstream;

		this->rdbuf(m_gxriobuf ? m_gxriobuf.get() : m_upstream);
	}

	/// \brief Regular move operator=
	basic_ostream &operator=(basic_ostream &&rhs) noexcept
	{
		base_type::operator=(std::move(rhs));
		m_gxriobuf = std::move(rhs.m_gxriobuf);
		m_upstream = rhs.m_upstream;

		this->rdbuf(m_gxriobuf ? m_gxriobuf.get() : m_upstream);

		return *this;
	}

  protected:
	basic_ostream()
		: base_type(nullptr) {}

	/// \brief Initialise internals with streambuf \a sb
	void init_z(upstreambuf_type *sb)
	{
		if (not m_gxriobuf->init(sb))
			this->setstate(std::ios_base::failbit);
	}

  protected:
	/// \brief Our streambuf class
	std::unique_ptr<z_streambuf_type> m_gxriobuf;

	/// \brief The streambuf that receives the output, the upstream of the compressing streambuf.
	upstreambuf_type *m_upstream = nullptr;
};

// --------------------------------------------------------------------

/// \brief Control output to files compressing the contents with gzip.
///
/// \tparam CharT		Type of the character stream.
/// \tparam Traits		Traits for character type, defaults to char_traits<_CharT>.
///
/// This is an ofstream implementation that can writeto named files compressing the content
/// with gzip directly. The class inherits from std::basic_ostream and offers all the
/// associated functionality.

template <typename CharT, typename Traits>
class basic_ofstream : public basic_ostream<CharT, Traits>
{
  public:
	using base_type = basic_ostream<CharT, Traits>;

	using char_type = CharT;
	using traits_type = Traits;

	using filebuf_type = std::basic_filebuf<char_type, traits_type>;
	using gzip_streambuf_type = basic_ogzip_streambuf<char_type, traits_type>;
#if HAVE_LibLZMA
	using xz_streambuf_type = basic_oxz_streambuf<char_type, traits_type>;
#endif

	basic_ofstream() = default;

	~basic_ofstream()
	{
		if (is_open())
			close();
	}

	/// \brief Construct an ofstream
	/// \param filename Null terminated string specifying the file to open
	/// \param mode The mode in which to open the file

	explicit basic_ofstream(const char *filename, std::ios_base::openmode mode = std::ios_base::out)
	{
		open(filename, mode);
	}

	/// \brief Construct an ofstream
	/// \param filename std::string specifying the file to open
	/// \param mode The mode in which to open the file

	explicit basic_ofstream(const std::string &filename, std::ios_base::openmode mode = std::ios_base::out)
	{
		open(filename, mode);
	}

	/// \brief Construct an ofstream
	/// \param filename std::filesystem::path specifying the file to open
	/// \param mode The mode in which to open the file

	explicit basic_ofstream(const std::filesystem::path &filename, std::ios_base::openmode mode = std::ios_base::out)
	{
		open(filename, mode);
	}

	/// \brief Move constructor
	basic_ofstream(basic_ofstream &&rhs) noexcept
		: base_type(std::move(rhs))
	{
		m_filebuf = std::move(rhs.m_filebuf);
		this->m_upstream = &m_filebuf;

		if (this->m_gxriobuf)
			this->m_gxriobuf->set_upstream(&m_filebuf);
		else
			this->rdbuf(&m_filebuf);
	}

	basic_ofstream(const basic_ofstream &) = delete;

	basic_ofstream &operator=(const basic_ofstream &) = delete;

	/// \brief Move operator=
	basic_ofstream &operator=(basic_ofstream &&rhs) noexcept
	{
		base_type::operator=(std::move(rhs));
		m_filebuf = std::move(rhs.m_filebuf);
		this->m_upstream = &m_filebuf;

		if (this->m_gxriobuf)
			this->m_gxriobuf->set_upstream(&m_filebuf);
		else
			this->rdbuf(&m_filebuf);

		return *this;
	}

	/// \brief Set the compression level to use, 0 (no compression) to 9 (maximum).
	///
	/// This applies to files that are opened with a .gz or .xz extension.
	/// The default is 9. When called on an already open compressed file the
	/// level is forwarded to the active compressor; for gzip it takes effect
	/// immediately, for xz it applies the next time a file is opened.

	void set_compression_level(int level)
	{
		m_compression_level = level;

		if (this->m_gxriobuf)
			this->m_gxriobuf->set_compression_level(level);
	}

	/// \brief Open the file \a filename with mode \a mode
	/// \param filename std::filesystem::path specifying the file to open
	/// \param mode The mode in which to open the file
	///
	/// A compression algorithm is chosen upon the contents of the
	/// extension() of \a filename with .gz mapping to gzip compression
	/// and .xz to xz compression.

	void open(const std::filesystem::path &filename, std::ios_base::openmode mode = std::ios_base::out)
	{
		if (not m_filebuf.open(filename, mode | std::ios::binary))
			this->setstate(std::ios_base::failbit);
		else
		{
			if (filename.extension() == ".gz")
				this->m_gxriobuf.reset(new gzip_streambuf_type);
#if HAVE_LibLZMA
			else if (filename.extension() == ".xz")
				this->m_gxriobuf.reset(new xz_streambuf_type);
#endif
			else
				this->m_gxriobuf.reset(nullptr);

			this->m_upstream = &m_filebuf;

			if (this->m_gxriobuf)
			{
				this->m_gxriobuf->set_compression_level(m_compression_level);

				if (not this->m_gxriobuf->init(&m_filebuf))
					this->setstate(std::ios_base::failbit);
				else
				{
					this->rdbuf(this->m_gxriobuf.get());
					this->clear();
				}
			}
			else
			{
				this->rdbuf(&m_filebuf);
				this->clear();
			}
		}
	}

	/// \brief Open the file \a filename with mode \a mode
	/// \param filename std::string specifying the file to open
	/// \param mode The mode in which to open the file
	///
	/// A compression algorithm is chosen upon the contents of the
	/// extension of \a filename with .gz mapping to gzip compression
	/// and .xz to xz compression.

	void open(const std::string &filename, std::ios_base::openmode mode = std::ios_base::out)
	{
		open(std::filesystem::path{filename}, mode);
	}

	/// \brief Open the file \a filename with mode \a mode
	/// \param filename Null terminated string specifying the file to open
	/// \param mode The mode in which to open the file
	///
	/// A compression algorithm is chosen upon the contents of the
	/// extension of \a filename with .gz mapping to gzip compression
	/// and .xz to xz compression.

	void open(const char *filename, std::ios_base::openmode mode = std::ios_base::out)
	{
		open(std::filesystem::path{filename}, mode);
	}

	/// \brief Return true if the file is open
	/// \return m_filebuf.is_open()

	bool is_open() const noexcept
	{
		return m_filebuf.is_open();
	}

	/// \brief Close the file
	///
	/// Calls m_filebuf.close(). If that fails, the failbit is set.

	void close()
	{
		if (this->m_gxriobuf and not this->m_gxriobuf->close())
			this->setstate(std::ios_base::failbit);

		if (not m_filebuf.close())
			this->setstate(std::ios_base::failbit);
	}

	/// \brief Swap the contents with those of \a rhs
	/// \param rhs The ifstream to swap with

	void swap(basic_ofstream &rhs) noexcept
	{
		base_type::swap(rhs);
		m_filebuf.swap(rhs.m_filebuf);
		std::swap(this->m_gxriobuf, rhs.m_gxriobuf);
		std::swap(m_compression_level, rhs.m_compression_level);

		if (this->m_gxriobuf)
		{
			this->m_gxriobuf->set_upstream(&m_filebuf);
			this->rdbuf(this->m_gxriobuf.get());
		}
		else
			this->rdbuf(&m_filebuf);

		this->m_upstream = &m_filebuf;

		if (rhs.m_gxriobuf)
		{
			rhs.m_gxriobuf->set_upstream(&rhs.m_filebuf);
			rhs.rdbuf(rhs.m_gxriobuf.get());
		}
		else
			rhs.rdbuf(&rhs.m_filebuf);

		rhs.m_upstream = &rhs.m_filebuf;
	}

  private:
	/// \brief The filebuf
	filebuf_type m_filebuf;

	/// \brief The compression level used when opening compressed files
	int m_compression_level = kDefaultCompressionLevel;
};

// --------------------------------------------------------------------

/// \brief Convenience typedefs
using istream = basic_istream<char, std::char_traits<char>>;
using ifstream = basic_ifstream<char, std::char_traits<char>>;
using ofstream = basic_ofstream<char, std::char_traits<char>>;

// NOLINTEND(bugprone-use-after-move)

} // namespace gxrio
