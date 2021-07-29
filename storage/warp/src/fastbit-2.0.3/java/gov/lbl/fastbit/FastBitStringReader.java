package gov.lbl.fastbit;

import gov.lbl.fastbit.FastBitStringWriter.WriteHandle;

import java.io.BufferedOutputStream;
import java.io.ByteArrayOutputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.channels.FileLock;
import java.util.ArrayList;
import java.util.Arrays;

/**
 * Java StringReader for FastBit.  It converts the strings retured from
 * FastBit into strings that can be accessed from a Java program.  See
 * java/tests/TestFastBitJava.java for an example of use.
 *
 * @author Andrey Kolchanov
 * @ingroup FastBitJava
 */
public class FastBitStringReader {
    /**
     * Maximum size of the byte buffer.
     */
    final private long bufferSize;
    private static int DEFAULT_BUFFER_LENGTH = 1024;

    /**
     * FastBitStringReader constructor with default buffer length
     */
    public FastBitStringReader () {
	this.bufferSize = DEFAULT_BUFFER_LENGTH;
    }

    /**
     * FastBitStringReader constructor
     * @param bufferSize
     */
    public FastBitStringReader (long bufferSize) {
	this.bufferSize = bufferSize;
    }

    /**
     * Thread-safe FastBit String[] getter. This function does not open
     * and lock file.
     * @param fc
     * @param handle
     * @param column
     * @return
     * @throws FastBitStringReaderException
     */

    public String[]
	getQualifiedStrings(final FileChannel fc,
			    final FastBitStringReader.ReadHandle handle,
			    String column)
	throws FastBitStringReaderException {
	ArrayList<String> ret = new ArrayList<String>();
	try {
	    getStringArrayList(fc, handle, column, ret);
	    return ret.toArray(new String[0]);
	}
	catch (IOException ex) {
	    throw new FastBitStringReaderException(ex.getMessage());
	}
    }

    /**
     * Thread-safe FastBit String[] getter. This function opens and closes file.
     * @param handle
     * @param column
     * @return
     * @throws FastBitStringReaderException
     */
    public String[]
	getQualifiedStrings(final FastBitStringReader.ReadHandle handle,
			    String column)
	throws FastBitStringReaderException {
	ArrayList<String> ret = new ArrayList<String>();
	StringBuffer sb =
	    new StringBuffer(handle.getPartition()).append("/").append(column);
	FileChannel ch = null;

	try {
	    ch = new RandomAccessFile(new String(sb),"r").getChannel();
	    getStringArrayList(ch, handle, column, ret);
	    return ret.toArray(new String[0]);
	}
	catch (IOException ex) {
	    throw new FastBitStringReaderException(ex.getMessage());
	}
	finally {
	    try {
		ch.close();
	    }
	    catch (IOException ex) {
	    }
	}
    }

    private void getStringArrayList(final FileChannel fc,
				    final FastBitStringReader.ReadHandle handle,
				    String column,
				    ArrayList<String> ret)
	throws FastBitStringReaderException,
	       IOException, UnsupportedEncodingException {
	long[] offsets =
	    handle.getFb().get_qualified_longs(handle.getFbHandle(), column);
	if (offsets == null) {
	    throw new FastBitStringReaderException
		("get_qualified_longs result is null");
	}

	for (int i=0; i<offsets.length; i++) {
	    StringBuffer buf = new StringBuffer();

	    boolean eobuf = false;
	    long offset = offsets[i];
	    do {
		long size = Math.min(bufferSize, fc.size() - offset);
		if (size <= 0) {
		    throw new FastBitStringReaderException
			("Buffer size "+size+ " < 0" );
		}

		ByteBuffer bb = fc.map(FileChannel.MapMode.READ_ONLY, offset,
				       size);
		byte[] buffer = new byte[ new Long(size).intValue()];
		bb.get(buffer);

		int stringLength=0;
		for (int j=0; j<buffer.length; j++) {
		    if (buffer[j] == 0) {
			eobuf = true;
			break;
		    }
		    stringLength=j;
		}
		buf.append (new String(Arrays.copyOf(buffer, stringLength+1),
				       handle.getCharsetName()));
		offset +=size;
	    } while (!eobuf);

	    ret.add(new String(buf));
	}
    }


    /**
     * Create FastBitStringReader.ReadHandle. Currently
     * FastBit.QueryHandle has not methods to get FastBit and
     * partition.
     * @param fb
     * @param fbHandle
     * @param partition
     * @param charsetName
     * @return
     */
    public FastBitStringReader.ReadHandle
	getHandle(FastBit fb, FastBit.QueryHandle fbHandle, String partition,
		  String charsetName ){
	return new FastBitStringReader.ReadHandle
	    (fb, fbHandle, partition, charsetName) ;
    }


    /**
     * An auxiliary thread-safe class to hold file locks and buffers
     * @author Andrey Kolchanov
     *
     */
    public class ReadHandle {
	final private FastBit fb;
	final private FastBit.QueryHandle fbHandle;
	final private String partition;
	final private String charsetName;

	public ReadHandle (FastBit fb, FastBit.QueryHandle fbHandle,
			   String partition, String charsetName) {
	    this.fb = fb;
	    this.fbHandle = fbHandle;
	    this.partition = partition;
	    this.charsetName = charsetName;
	}

	private String getPartition() {
	    return partition;
	}

	private FastBit getFb() {
	    return fb;
	}

	private FastBit.QueryHandle getFbHandle() {
	    return fbHandle;
	}

	private String getCharsetName() {
	    return charsetName;
	}
    }
}
