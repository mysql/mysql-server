package gov.lbl.fastbit;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.channels.FileLock;
import java.nio.charset.UnsupportedCharsetException;
import java.util.Date;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.atomic.AtomicLong;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Java StringWriter for FastBit.  For a Java program to write
 * string-valued data to a format required by FastBit.  See
 * java/tests/TestFastBitJava.java for an example of use.
 *
 * @author Andrey Kolchanov
 * @ingroup FastBitJava
 */
public class FastBitStringWriter {
    final static private byte zero = 0;

    /**
     * Maximum size of the byte buffers.
     */
    final private int bufferLength;
    private static int DEFAULT_BUFFER_LENGTH = 1048576;
    private static int LONG_LENGTH = 8;

    /**
     * FastBitStringWriter constructor with default buffer size
     */
    public FastBitStringWriter() {
	this.bufferLength=DEFAULT_BUFFER_LENGTH;
    }

    /**
     * FastBitStringWriter constructor
     * @param bufferLength
     */
    public FastBitStringWriter(int bufferLength) {
	this.bufferLength=bufferLength;
    }

    /**
     * Add arbitrary string values
     * @param partition
     * @param colname
     * @param arr
     * @param charsetName
     * @throws FastBitStringWriterException
     */
    public void addText(String partition, String colname, String[] arr,
			String charsetName)
	throws FastBitStringWriterException {
	WriteHandle handler = getHandle(partition, colname, charsetName);
	for (int i=0; i<arr.length; i++) {
	    addRow(handler, arr[i]);
	}
	handler.close();
    }

    /**
     * Add String values with a small number of distinct choices.
     * @param partition
     * @param colname
     * @param arr
     * @param charsetName
     * @throws FastBitStringWriterException
     */
    public void addCategories(String partition, String colname, String[] arr,
			      String charsetName)
	throws FastBitStringWriterException {
	WriteHandle handler = getHandle(partition, colname, charsetName);
	String dicFileName = handler.getDataFileName()+".dic";
	Set<String> keys = new HashSet<String>();
	Set<String> oldkeys = new HashSet<String>();

	File dicFile = new File(dicFileName);
	FileLock dicLock = null;

	try {
	    if (dicFile.exists()) {
		dicLock =
		    new RandomAccessFile(dicFile,"rw").getChannel().lock();
		// Dictionary must fit in the heap memory
		ByteBuffer oldBuffer = ByteBuffer.allocate
		    (new Long(dicLock.channel().size()).intValue());
		dicLock.channel().read(oldBuffer);
		oldBuffer.position(0);
		oldkeys = getOldDicValues(oldBuffer, oldkeys, charsetName);
	    } else {
		dicLock =
		    new RandomAccessFile(dicFile,"rw").getChannel().lock();
	    }
	} catch (FileNotFoundException ex) {
	    throw new FastBitStringWriterException(ex.getMessage());
	} catch (IOException ex) {
	    throw new FastBitStringWriterException(ex.getMessage());
	}


	for (int i=0; i<arr.length; i++) {
	    addRow(handler, arr[i]);
	    if (!keys.contains(arr[i]) && !oldkeys.contains(arr[i])) {
		keys.add(arr[i]);
	    }
	}

	saveDictionary(handler, keys, dicLock);
	try {
	    dicLock.release();
	}
	catch (IOException ex) {
	    throw new FastBitStringWriterException(ex.getMessage());
	}
	handler.close();
    }

    /**
     * An auxiliary translator from long to byte array
     * @param a
     * @return
     */
    private byte[] getBytes(long a) {
	byte [] b = new byte[8];
	for (int i= 0; i < 8; i++) {
	    b[i] = (byte)(a >>> (i * 8) & 0xFF);
	}
	return b;
    }

    private void flushBuffer(ByteBuffer buf, FileChannel channel)
	throws IOException {
	buf.flip();
	channel.write(buf);
	buf.clear();
    }

    /**
     * Add old dictionary values to the Set
     * @param buf
     * @param keys
     * @param charsetName
     * @return
     */
    private synchronized Set<String>
	getOldDicValues(ByteBuffer buf, final Set<String> keys,
			String charsetName)
	throws UnsupportedEncodingException {
	if (buf.position() == buf.capacity()) {
	    return keys;
	}
	ByteArrayOutputStream bos = new ByteArrayOutputStream();
	byte b = buf.get();
	while (b != 0) {
	    bos.write(b);
	    b = buf.get();
	}
	String key = bos.toString(charsetName);
	if (!keys.contains(key)) {
	    keys.add(key);
	}
	return getOldDicValues(buf, keys, charsetName);
    }

    /**
     * Add dictionary strings to a .dic file
     *
     * @param handler
     * @param keys
     * @param fileName
     * @throws FastBitStringWriterException
     */
    public void saveDictionary(final WriteHandle handler,
                               final Set<String> keys,  String fileName)
	throws FastBitStringWriterException {
	try {
	    FileLock dicLock =
		new FileOutputStream(fileName).getChannel().lock();
	    saveDictionary(handler, keys, dicLock);
	    dicLock.release();
	} catch (IOException ex) {
	    throw new FastBitStringWriterException(ex.getMessage());
	}
    }

    /**
     * Add dictionary strings to a .dic file
     * @param handler
     * @param keys
     */
    private void saveDictionary(final WriteHandle handler,
                                final Set<String> keys, FileLock dicLock)
	throws FastBitStringWriterException {
	try {
	    // rewind to the end of the file
	    dicLock.channel().position(dicLock.channel().size());

	    ByteBuffer dicbuf = ByteBuffer.allocate(bufferLength);
	    for (String key: keys) {

		byte[] ar = key.getBytes(handler.getCharsetName());
		if (bufferLength - dicbuf.position() < ar.length +1) {
		    flushBuffer(dicbuf, dicLock.channel());
		}
		dicbuf.put(ar);
		dicbuf.put(zero);
	    }
	    flushBuffer(dicbuf, dicLock.channel());

	} catch (IOException ex) {
	    throw new FastBitStringWriterException(ex.getMessage());
	}
    }

    /**
     * Add String values row by row.
     * @param handler  The object to handle the actual write operation.
     * @param row The content of the row to be written.
     * @throws FastBitStringWriterException
     */
    public void addRow(final WriteHandle handler, String row)
	throws FastBitStringWriterException {
	try {
	    byte[] ar = row.getBytes(handler.getCharsetName());
	    if (bufferLength - handler.getDatabuf().position() < ar.length +1) {
		flushBuffer(handler.getDatabuf(),
			    handler.getDatalock().channel());
	    }
	    handler.getDatabuf().put(ar);
	    handler.getDatabuf().put(zero);

	    if (bufferLength - handler.getSpbuf().position() < LONG_LENGTH) {
		flushBuffer(handler.getSpbuf(), handler.getSplock().channel());
	    }
	    handler.getSpbuf().put
		(getBytes(handler.getOffset().addAndGet(1 + ar.length)));
	} catch (IOException ex) {
	    throw new FastBitStringWriterException(ex.getMessage());
	}
    }

    /**
     * Add column info to existing index metadata
     * @param partitionDirectory
     * @param columnName
     * @param type
     * @throws FastBitStringWriterException
     */
    public synchronized void addStringColumnToMetadata
        (String partitionDirectory, String columnName, String type)
        throws FastBitStringWriterException {
        File header =
            checkMetadata(partitionDirectory, columnName, type, false);

        RandomAccessFile raf=null;
        FileLock lock=null;
        try {
            raf = new RandomAccessFile(header, "rw");
            lock = raf.getChannel().lock();
            Pattern p1 = Pattern.compile("^Number_of_columns = \\d+");

            String str ="";
            StringBuffer outStr = new StringBuffer();
            do {
                str = raf.readLine();
                if (str != null) {
                    Matcher m1 = p1.matcher(str);
                    if (m1.matches()) {
                        Pattern p2 = Pattern.compile("\\d+");
                        Matcher m2 = p2.matcher(str);
                        m2.find();
                        int oldColumnCount = Integer.parseInt(m2.group());
                        outStr.append("Number_of_columns = ");
                        outStr.append(++oldColumnCount);
                        outStr.append("\n");
                    } else {
                        outStr.append(str);
                        outStr.append("\n");
                    }
                }
            } while (str != null);

            outStr.append("\n");
            outStr.append("Begin Column\n");
            outStr.append("name = ").append(columnName);
            outStr.append("\n");
            outStr.append("data_type = ").append(type.toUpperCase());
            outStr.append("\n");
            if (type.toUpperCase().equals("TEXT")) {
                outStr.append("index=none\n");
            }
            outStr.append("End Column\n");

            String resString = new String(outStr);
            raf.seek(0);
            raf.write(resString.getBytes());
        } catch (IOException ex) {
            throw new FastBitStringWriterException(ex.getMessage());
        } finally {
            try {
                lock.release();
                raf.close();
            } catch(IOException ex) {
                throw new FastBitStringWriterException(ex.getMessage());
            }
        }
    }

    /**
     * Check metadata parameters.
     * @param partitionDirectory
     * @param columnName
     * @param type
     * @param newHeader
     * @return
     * @throws FastBitStringWriterException
     */
    private File checkMetadata(String partitionDirectory, String columnName,
			       String type,  boolean newHeader)
	throws FastBitStringWriterException {
	File header = new File(partitionDirectory+"/-part.txt");
	if (newHeader) {
	    try {
		header.createNewFile();
	    } catch (IOException ex) {
		throw new FastBitStringWriterException(ex.getMessage());
	    }
	} else {
	    if (!header.exists()) {
		throw new FastBitStringWriterException
		    ("File "+partitionDirectory+"/-part.txt not found");
	    }
	}

	File column = new File(partitionDirectory+"/"+columnName);
	if (!column.exists()) {
	    throw new FastBitStringWriterException
		("File "+partitionDirectory+"/"+columnName+" not found");
	}
	if (!type.toUpperCase().equals("KEY") &&
	    !type.toUpperCase().equals("TEXT")) {
	    throw new FastBitStringWriterException
		("Type should be KEY or TEXT. "+type+ " found");
	}
	return header;
    }

    /**
     * Create metadata for a data partition with a single column.
     * @param partitionDirectory Directory for the data partition
     * @param columnName Column name.  Also used as the data partition name.
     * @param type Data type.
     * @param rowCount Number of rows.
     * @throws FastBitStringWriterException
     */
    public synchronized void createMetadata
        (String partitionDirectory, String columnName, String type,
         int rowCount)
        throws FastBitStringWriterException {
        File header = checkMetadata(partitionDirectory, columnName, type,
                                    true);

        StringBuffer outStr = new StringBuffer();
        outStr.append("# meta data for data partition ");
        outStr.append(header.getParentFile().getName());
        outStr.append(" by FastBitStringWriter::createMetadata on ");
        outStr.append(new Date()).append("\n");
        outStr.append("\n");
        outStr.append("BEGIN HEADER\n");
        outStr.append("Name = ").append(columnName).append("\n");
        outStr.append("Description = user-supplied data parsed by FastBitStringWriter\n");
        outStr.append("Number_of_rows = ").append(rowCount).append("\n");
        outStr.append("Number_of_columns = 1\n");
        outStr.append("Timestamp  = ").append(new Date().getTime()/1000);
        outStr.append("\n");
        outStr.append("END HEADER\n");
        outStr.append("\n");
        outStr.append("Begin Column\n");
        outStr.append("name = ").append(columnName).append("\n");
        outStr.append("data_type = ").append(type.toUpperCase()).append("\n");
        if (type.toUpperCase().equals("TEXT")) {
            outStr.append("index=none\n");
        }
        outStr.append("End Column\n");

        RandomAccessFile raf=null;
        FileLock lock=null;
        try {
            raf = new RandomAccessFile(header, "rw");
            lock = raf.getChannel().lock();
            raf.setLength(0);

            String resString = new String(outStr);
            raf.seek(0);
            raf.write(resString.getBytes());
        } catch (IOException ex) {
            throw new FastBitStringWriterException(ex.getMessage());
        } finally {
            try {
                lock.release();
                raf.close();
            } catch(IOException ex) {
                throw new FastBitStringWriterException(ex.getMessage());
            }
        }
    }

    /**
     * Create WriteHandle.
     *
     * @param partition
     * @param colname
     * @param charsetName
     * @return
     * @throws FastBitStringWriterException
     */
    public WriteHandle
	getHandle(String partition, String colname, String charsetName)
	throws FastBitStringWriterException {
	String dataFileName =
	    new String(new StringBuffer(partition).append("/").append(colname));
	String spFileName =
	    new String(new StringBuffer(partition).append("/").append(colname).append(".sp"));

	try {
	    return new WriteHandle(dataFileName, spFileName, charsetName);
	} catch (IOException ex) {
	    throw new FastBitStringWriterException(ex.getMessage());
	}
    }

    /**
     * An auxiliary thread-safe class to hold file locks and buffers.
     * @author Andrey Kolchanov
     *
     */
    public class WriteHandle {

	// String data file name
	final String dataFileName;

	//String data file lock
	final private FileLock datalock;

	// .sp file lock
	final private FileLock splock;

	// String data buffer
	final private ByteBuffer databuf;

	// .sp file buffer
	final private ByteBuffer spbuf;

	// CharsetName
	final String charsetName;

	// String data file offset
	AtomicLong offset = new AtomicLong(0);

	private WriteHandle(String dataFileName, String spFileName,
                            String charsetName) throws IOException {
	    this.charsetName = charsetName;
	    this.dataFileName = dataFileName;
	    datalock =
		new RandomAccessFile(dataFileName,"rw").getChannel().lock();
            FileChannel spch =
                new RandomAccessFile(spFileName,"rw").getChannel();
	    splock = spch.lock();

	    // rewind to the end of the file
	    datalock.channel().position(datalock.channel().size());
	    splock.channel().position(splock.channel().size());

	    databuf = ByteBuffer.allocate(bufferLength);
	    spbuf = ByteBuffer.allocate(bufferLength);

            long splength = spch.size();
            if (splength % 8 != 0) {
                // truncate the file to be multiple of 8 bytes
                splength = 8 * (splength / 8);
                spch.truncate(splength);
            }
            if (splength == 0) { // write the first 0 to spbuf
                spbuf.put(getBytes(0));
            }
            else { // read the last 8 bytes from the file
                splength -= 8;
                spch.read(spbuf, splength);
                splength = spbuf.getLong();
                offset.getAndSet(splength);
                spbuf.clear(); // no longer need the value in the buffer
            }
	}

	public void close() throws FastBitStringWriterException {
	    try {
		flushBuffer(databuf, datalock.channel());
		flushBuffer(spbuf, splock.channel());

		datalock.release();
		splock.release();
		datalock.channel().close();
		splock.channel().close();
		databuf.clear();
		spbuf.clear();
	    } catch (IOException ex) {
		throw new FastBitStringWriterException(ex.getMessage());
	    }
	}

	private FileLock getDatalock() {
	    return datalock;
	}

	private FileLock getSplock() {
	    return splock;
	}

	private ByteBuffer getDatabuf() {
	    return databuf;
	}

	private ByteBuffer getSpbuf() {
	    return spbuf;
	}

	private String getCharsetName() {
	    return charsetName;
	}

	private AtomicLong getOffset() {
	    return offset;
	}

	private String getDataFileName() {
	    return dataFileName;
	}
    }
}
