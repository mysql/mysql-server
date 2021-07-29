package tests;


import gov.lbl.fastbit.FastBit;
import gov.lbl.fastbit.FastBitStringReader;
import gov.lbl.fastbit.FastBitStringReaderException;
import gov.lbl.fastbit.FastBitStringWriter;
import gov.lbl.fastbit.FastBitStringWriterException;
import gov.lbl.fastbit.FastBit.QueryHandle;
import gov.lbl.fastbit.FastBitStringWriter.WriteHandle;

import java.io.File;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.RandomAccessFile;
import java.io.Reader;
import java.nio.channels.FileChannel;
import java.util.Date;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.apache.log4j.PropertyConfigurator;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import static org.junit.Assert.*;

/**
 * JUnit test class.  It exercises FastBit JNI interface as well as
 * FastBitStringReader and FastBitStringWriter.
 * @author Andrey Kolchanov
 */
public class TestFastBitJava {
    protected static Log l = LogFactory.getLog("TestFastBitJava");
    static int [] intArray = null;
    static String [] strArray = null;
    private static int ROW_COUNT = 10000000;
    private static String TDIR = "tmp";

    @BeforeClass
	public static void setUp() throws Exception {
	PropertyConfigurator.configure("tests/log4j.properties");
	l.info("Prepare test data");
	prepareData();
    }

    /**
     * Prepare simple test data
     * @throws Exception
     */
    public static void prepareData() throws Exception {
	String [] ar = {"a0","a1","a2","a3","a4","a5","a6","a7","a8","a9"};
	intArray = new int[ROW_COUNT];			
	strArray = new String[ROW_COUNT];
	for (int i=0; i< ROW_COUNT; i++){
	    intArray[i] = i;
	    strArray[i] = ar[i % 9];
	}
    }

    public void touchDir(String name) {
	File f = new File (name);	
	if (!f.exists()){
	    l.info("Trying to create dir "+name);
	    f.mkdir();
	}
    }

    @Test
    /**
     * Test call FastBit native write function
     */
	public void testFastBitInt() {
	String testName = "testFastBitInt";
	String partName = TDIR+"/testFastBitInt";
	l.info(testName+" started with "+ ROW_COUNT + " rows");
	Date d1 = new Date();

	FastBit fb = new FastBit(null);
	if (!new File(partName).exists()) {
	    fb.add_ints("i", intArray);
	    fb.write_buffer(partName);			
	} else {
	    l.info("Directory "+partName+" already exists");
	}

	l.info(testName + " passed in " + (new Date().getTime() - d1.getTime())
	       + " ms.");
    }

    @Test
    /**
     * Test call FastBit native read function
     */
	public void testReadFastBitInt() {
	String testName = "testReadFastBitInt";
	String partName = TDIR+"/testFastBitInt";
	l.info(testName+" started with "+ ROW_COUNT + " rows");
	Date d1 = new Date();

	FastBit fb = new FastBit(null);

	QueryHandle handle = fb.build_query("i", partName, "i>=0");
	int count = fb.get_result_size(handle);

	assertEquals(ROW_COUNT, count);

	l.info(testName + " passed in " + (new Date().getTime() - d1.getTime())
	       + " ms.");
    }	


    @Test
    /**
     * Test raw fastbit java writer.  Add metadata to existing index
     * header -part.txt.
     */
	public void testFastBitJavaAddArrayText()
	throws FastBitStringWriterException {
	String testName = "testFastBitJavaAddArrayText";
	String partName = TDIR+"/testFastBitAddJavaText";
	l.info(testName+" started with "+ ROW_COUNT + " rows");
	Date d1 = new Date();

	FastBit fb = new FastBit(null);

	if (!new File(partName).exists()) {
	    fb.add_ints("i", intArray);
	    fb.write_buffer(partName);

	    FastBitStringWriter writer = new FastBitStringWriter();
	    // Add text data
	    writer.addText(partName, "t", strArray, "UTF-8");
	    writer.addStringColumnToMetadata(partName, "t", "text");

	    // Add category data			
	    writer.addCategories(partName, "k", strArray, "UTF-8");
	    writer.addStringColumnToMetadata(partName, "k", "key");
	} else {
	    l.info("Directory "+partName+" already exists");
	}


	l.info(testName + " passed in " + (new Date().getTime() - d1.getTime())
               + " ms.");
    }

    @Test
    /**
     * Test raw fastbit java writer. Create a new -part.txt 
     */	
	public void testFastBitJavaCreateArrayText()
	throws FastBitStringWriterException {
	String testName = "testFastBitJavaCreateArrayText";
	String partName = TDIR+"/testFastBitCreateJavaText";
	l.info(testName+" started with "+ ROW_COUNT + " rows");
	Date d1 = new Date();

	if (!new File(partName).exists()) {
	    touchDir(partName);

	    FastBitStringWriter writer = new FastBitStringWriter();
	    // Add text data
	    writer.addText(partName, "t", strArray, "UTF-8");
	    writer.createMetadata(partName, "t", "text", strArray.length);

	    // Add category data			
	    writer.addCategories(partName, "k", strArray, "UTF-8");
	    writer.addStringColumnToMetadata(partName, "k", "key");
	} else {
	    l.info("Directory "+partName+" already exists");
	}


	l.info(testName + " passed in " + (new Date().getTime() - d1.getTime())
	       + " ms.");
    }

    @Test
    /**
     * Test raw fastbit java writer in a row by row manner.  
     */		
	public void testFastBitJavaCreateRowByRowText()
	throws FastBitStringWriterException {
	String testName = "testFastBitJavaCreateRowByRowText";
	String partName = TDIR+"/testFastBitJavaCreateRowByRowText";
	l.info(testName+" started with "+ ROW_COUNT + " rows");
	Date d1 = new Date();

	if (!new File(partName).exists()) {
	    touchDir(partName);

	    FastBitStringWriter writer = new FastBitStringWriter();

	    // Add text data
	    WriteHandle handle = writer.getHandle(partName, "t", "UTF-8");
	    for (int i=0; i<strArray.length; i++){
		writer.addRow(handle, strArray[i]);
	    }
	    handle.close();			

	    writer.createMetadata(partName, "t", "text", strArray.length);
	} else {
	    l.info("Directory "+partName+" already exists");
	}

	l.info(testName + " passed in " + (new Date().getTime() - d1.getTime())
	       + " ms.");
    }	

    @Test
    /**
     * Test java fastbit reader.
     */	
	public void testFastBitJavaReadText()
	throws FastBitStringReaderException {
	String testName = "testFastBitJavaReadText";
	String partName = TDIR+"/testFastBitCreateJavaText";
	l.info(testName+" started with "+ ROW_COUNT + " rows");
	Date d1 = new Date();

	FastBit fb = new FastBit(null);
	//fb.set_message_level(9);
	QueryHandle fbHandle = fb.build_query("k", partName,
					      "k= 'a7' and t = 'a7'");

	FastBitStringReader reader =  new FastBitStringReader();
	FastBitStringReader.ReadHandle handle =
	    reader.getHandle(fb, fbHandle, partName, "UTF-8");
	String[] res = reader.getQualifiedStrings(handle, "k");

	assertNotNull("Result String array is null", res);
	assertEquals("a7", res[0]);

	l.info(testName + " passed in " + (new Date().getTime() - d1.getTime())
	       + " ms.");
    }

    @Test
    /**
     * Read text data from existing FileChannel
     */
	public void testFastBitJavaReadFileChannelText()
	throws FastBitStringReaderException, IOException {
	String testName = "testFastBitJavaReadFileChannelText";
	String partName = TDIR+"/testFastBitJavaCreateRowByRowText";
	l.info(testName+" started with "+ ROW_COUNT + " rows");
	Date d1 = new Date();

	FastBit fb = new FastBit(null);
	//fb.set_message_level(9);
	QueryHandle fbHandle = fb.build_query("t", partName, "t = 'a7'");

	FastBitStringReader reader =  new FastBitStringReader();
	FastBitStringReader.ReadHandle handle =
	    reader.getHandle(fb, fbHandle, partName, "UTF-8");

	FileChannel ch = new RandomAccessFile(partName+"/t","r").getChannel();
	String[] res = reader.getQualifiedStrings(ch, handle, "t");
	ch.close();

	assertNotNull("Result String array is null",res);
	assertEquals("a7", res[0]);

	l.info(testName + " passed in " + (new Date().getTime() - d1.getTime())
	       + " ms.");
    }
}
