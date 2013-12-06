#ifndef page0size_t
#define page0size_t

#include "univ.i"
#include "fsp0types.h"

#define PAGE_SIZE_T_SIZE_BITS	15

class page_size_t {
public:
	page_size_t(ulint physical, ulint logical, bool is_compressed)
		:
		m_physical(physical),
		m_logical(logical),
		m_is_compressed(is_compressed)
	{
		ut_ad(physical <= (1 << PAGE_SIZE_T_SIZE_BITS));
		ut_ad(logical <= (1 << PAGE_SIZE_T_SIZE_BITS));

		ut_ad(ut_is_2pow(physical));
		ut_ad(ut_is_2pow(logical));

		ut_ad(logical <= UNIV_PAGE_SIZE_MAX);
		ut_ad(!is_compressed || physical <= UNIV_ZIP_SIZE_MAX);
	}

	explicit page_size_t(ulint fsp_flags)
	{
		ulint	ssize = FSP_FLAGS_GET_PAGE_SSIZE(fsp_flags);

		if (ssize != 0) {
			/* Convert from a 'log2 minus 9' to a page size
			in bytes. */
			const ulint	size
				= ((UNIV_ZIP_SIZE_MIN >> 1) << ssize);

			ut_ad(size <= UNIV_PAGE_SIZE_MAX);
			ut_ad(size <= (1 << PAGE_SIZE_T_SIZE_BITS));

			m_logical = size;
		} else {
			/* If the page size was not stored, then it
			is the legacy 16k. */
			m_logical = UNIV_PAGE_SIZE_ORIG;
		}

		ssize = FSP_FLAGS_GET_ZIP_SSIZE(fsp_flags);

		if (ssize == 0) {
			m_is_compressed = false;
			m_physical = m_logical;
		} else {
			m_is_compressed = true;

			/* Convert from a 'log2 minus 9' to a page size
			in bytes. */
			const ulint	phy
				= ((UNIV_ZIP_SIZE_MIN >> 1) << ssize);

			ut_ad(phy <= UNIV_ZIP_SIZE_MAX);
			ut_ad(phy <= (1 << PAGE_SIZE_T_SIZE_BITS));

			m_physical = phy;
		}
	}

	inline ulint physical() const
	{
		/* Remove this assert once we add support for different
		page size per tablespace. Currently all tablespaces must
		have a page size that is equal to --innodb-page-size= */
		ut_ad(m_logical == srv_page_size);

		ut_ad(m_physical > 0);
		return(m_physical);
	}

	inline ulint logical() const
	{
		ut_ad(m_logical > 0);
		return(m_logical);
	}

	inline bool is_compressed() const
	{
		//buf_block_get_page_zip() == NULL;
		return(m_is_compressed);
	}

	inline bool equals_to(const page_size_t& a) const
	{
		return(a.physical() == m_physical
		       && a.logical() == m_logical
		       && a.is_compressed() == m_is_compressed);
	}

	inline void copy_from(const page_size_t& src)
	{
		m_physical = src.physical();
		m_logical = src.logical();
		m_is_compressed = src.is_compressed();
	}

private:
	//page_size_t(const page_size_t&);
	void operator=(const page_size_t&);

	/* For non compressed tablespaces, physical page size is equal to
	the logical page size and the data is stored in buf_page_t::frame
	(and is also always equal to univ_page_size (--innodb-page-size=)).

	For compressed tablespaces, physical page size is the compressed
	page size as stored on disk and in buf_page_t::zip::data. The logical
	page size is the uncompressed page size in memory - the size of
	buf_page_t::frame (currently also always equal to univ_page_size
	(--innodb-page-size=)). */

	/* XXX perf test if this makes any difference and use the faster
	one if it does. */
#if 1
	/** Physical page size. */
	unsigned	m_physical:PAGE_SIZE_T_SIZE_BITS;

	/** Logical page size. */
	unsigned	m_logical:PAGE_SIZE_T_SIZE_BITS;

	/** Flag designating whether the page/tablespace is compressed. */
	unsigned	m_is_compressed:1;
#else
	/** Physical page size. */
	ulint	m_physical;

	/** Logical page size. */
	ulint	m_logical;

	/** Flag designating whether the page/tablespace is compressed. */
	bool	m_is_compressed;
#endif
};

extern page_size_t	univ_page_size;

#endif /* page0size_t */
