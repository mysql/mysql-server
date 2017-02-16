/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       file_io.h
/// \brief      I/O types and functions
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

// Some systems have suboptimal BUFSIZ. Use a bit bigger value on them.
#if BUFSIZ <= 1024
#	define IO_BUFFER_SIZE 8192
#else
#	define IO_BUFFER_SIZE BUFSIZ
#endif


typedef struct {
	/// Name of the source filename (as given on the command line) or
	/// pointer to static "(stdin)" when reading from standard input.
	const char *src_name;

	/// Destination filename converted from src_name or pointer to static
	/// "(stdout)" when writing to standard output.
	char *dest_name;

	/// File descriptor of the source file
	int src_fd;

	/// File descriptor of the target file
	int dest_fd;

	/// Stat of the source file.
	struct stat src_st;

	/// Stat of the destination file.
	struct stat dest_st;

	/// True once end of the source file has been detected.
	bool src_eof;

} file_pair;


/// \brief      Initialize the I/O module
extern void io_init(void);


/// \brief      Opens a file pair
extern file_pair *io_open(const char *src_name);


/// \brief      Closes the file descriptors and frees possible allocated memory
///
/// The success argument determines if source or destination file gets
/// unlinked:
///  - false: The destination file is unlinked.
///  - true: The source file is unlinked unless writing to stdout or --keep
///    was used.
extern void io_close(file_pair *pair, bool success);


/// \brief      Reads from the source file to a buffer
///
/// \param      pair    File pair having the source file open for reading
/// \param      buf     Destination buffer to hold the read data
/// \param      size    Size of the buffer; assumed be smaller than SSIZE_MAX
///
/// \return     On success, number of bytes read is returned. On end of
///             file zero is returned and pair->src_eof set to true.
///             On error, SIZE_MAX is returned and error message printed.
extern size_t io_read(file_pair *pair, uint8_t *buf, size_t size);


/// \brief      Writes a buffer to the destination file
///
/// \param      pair    File pair having the destination file open for writing
/// \param      buf     Buffer containing the data to be written
/// \param      size    Size of the buffer; assumed be smaller than SSIZE_MAX
///
/// \return     On success, zero is returned. On error, -1 is returned
///             and error message printed.
extern bool io_write(const file_pair *pair, const uint8_t *buf, size_t size);
