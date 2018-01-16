/*
 *@Author Chris Voncina
 * A C++ Plugin that streams Binary logs from mysql to AWS Kinesis
 */
#pragma GCC diagnostic ignored "-fpermissive" 
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <stdio.h>
#include <mysql/plugin.h>
#include <mysql/plugin_audit.h>
#include <m_ctype.h>
#include <my_sys.h>
#include <mysqld_error.h>
#include <mysql.h>
#include "Bin_Stream.h"
#include "my_compiler.h"
#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/kinesis/KinesisClient.h>
#include <aws/kinesis/model/PutRecordRequest.h>
#include <aws/kinesis/model/StreamStatus.h>
#include <aws/kinesis/model/CreateStreamRequest.h>
#include <aws/kinesis/model/DeleteStreamRequest.h>
#include <aws/kinesis/model/DescribeStreamRequest.h>
#include <aws/kinesis/model/Record.h>
#include <aws/core/utils/StringUtils.h>
#include <aws/core/utils/Array.h>
#include <aws/core/utils/Outcome.h>
#include <iostream>
#include <thread>
#include <unistd.h>
using namespace std;
/*
  to grab any binary log the stream has to be set first
*/
const char * streamName = "BinLogStream32";
//const char *accessKeyId = "AKIAIKIEWEP4FCYW4AJQ";
//const char * secretKey = "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";
const Aws::Client::ClientConfiguration clientConfig;
typedef Aws::Utils::Array<unsigned char> ByteBuffer;
const char * ALLOCATION_TAG = "BinlogStreamTest";
Aws::SDKOptions options;
//const char * ALLOCATION_TAG_NOTIFY = "BinlogStreamNotify";
Aws::Kinesis::Model::PutRecordRequest record;
std::shared_ptr<Aws::Kinesis::KinesisClient> Client;
Aws::String announcement("AWS Binary Log for C++ ");

static void bin_log_finalize() { //this function is run after quiting mysql
    printf("Entering bin_log_finalize\n"); //I have not implement anything but left this here.

}
// #define USE_IMPORT_EXPORT already defined
 //static int bin_log_notify(){ 
//thd: connection context event_classs: event class value, event: event data
 static void bin_log_notify(MYSQL_THD thd, mysql_event_class_t event_class, const void * event) {  
	//use the audti notify function to grab mysql server information
  Aws::Kinesis::Model::PutRecordRequest BinaryRecord;
  BinaryRecord.SetStreamName(streamName);
  BinaryRecord.SetPartitionKey("1");  
  if (event_class == MYSQL_AUDIT_GENERAL_CLASS) {
    const struct mysql_event_general *event_general = (const struct mysql_event_general *)event; //grab the event from the general class
  
  if (event_general->event_subclass == MYSQL_AUDIT_GENERAL_LOG) {

      BinaryRecord.SetData(ByteBuffer((unsigned char *) event_general->general_query.str, event_general->general_query.length)); //the record is being set here
      Client->PutRecord(BinaryRecord); //adding the record to the client
     
     }
  }
/* below is more features to add into the project I decided not to implement them.
  else if (event_class == MYSQL_AUDIT_SERVER_STARTUP_CLASS) {
  }
  else if (event_class == MYSQL_AUDIT_SERVER_SHUTDOWN_CLASS){
  }
  else if (event_class == MYSQL_AUDIT_QUERY_CLASS) {
    const struct mysql_event_query *event_query = (const struct mysql_event_query *)event;
    buffer_data= sprintf(buffer, "sql_command_id=\"%d\"",
                         (int) event_query->sql_command_id);
   switch (event_query->event_subclass) {
    case MYSQL_AUDIT_QUERY_START:
      break;
    case MYSQL_AUDIT_QUERY_NESTED_START:
      break;
    case MYSQL_AUDIT_QUERY_STATUS_END:
      break;
    case MYSQL_AUDIT_QUERY_NESTED_STATUS_END:
      break;
    default:
      break;
    }
  }
  else {
	//do nothing
  }
*/
}
extern "C" void initialize(Aws::Client::ClientConfiguration clientConfig, const char * streamName, const char * ALLOCATION_TAG, const int shardNumber) {

	Client = Aws::MakeShared<Aws::Kinesis::KinesisClient>(ALLOCATION_TAG, clientConfig); //using an IAM add security feature no need for adding secret key
	Aws::Kinesis::Model::DescribeStreamRequest currentStream; 
	currentStream.SetStreamName(streamName);//look for this stream name
	Aws::Kinesis::Model::CreateStreamRequest streamCreated;
	auto outcome_described = Client->DescribeStream(currentStream); //
	if (outcome_described.IsSuccess() && outcome_described.GetResult().GetStreamDescription().GetStreamName() == streamName) { //look for stream name if there do nothing
		//the stream exists
		printf("The stream is already created \n");
	}	
	else {
		
		printf("The stream does not exist creating it now\n");	
		streamCreated.SetStreamName(streamName);
		streamCreated.SetShardCount(shardNumber); //shardNumber should be 1 
		auto outcome =  Client->CreateStream(streamCreated);
			
		if (outcome.IsSuccess()) {
			printf("Congrats you have sucessifully created a stream \n");
		}
		else {
			printf("Error stream could not be created\n");
		}
	//	std::this_thread::sleep_for(10);//sleep for 10 seocnds waiting for stream to generate
		usleep(30000000);
	
	}
}

//static int  simple_streamer_plugin_init() {
 extern "C" int  simple_streamer_plugin_init() {
	 //a main function is not necessary only an init and deinit function for this project
	  Aws::InitAPI(options);
	  {
		const int shardNumber =1;
	
	        
		//clientConfig.region = Aws::Region::US_WEST_1; this sets region pretty unnecessary
	     initialize(clientConfig, streamName, ALLOCATION_TAG, shardNumber);
		/*
		 below is code to create a record. Useful to know for testing not applicable to my project
		 //	Aws::String announcement("AWS Binary Log for C++ ");
		record.SetStreamName(streamName);
		record.SetData(ByteBuffer((unsigned char *)announcement.c_str(),announcement.length())); //the record is being set here
		record.SetPartitionKey("1");
		Client->PutRecord(record); these work calling global client*/ 
		/*
		auto outcome1 = client->PutRecord(record);
		if (outcome1.IsSuccess()){
			printf("record Created\n");
			outcome1.GetResult();
		}
		else {
			printf("Record could not be created\n");
			outcome1.GetError();
		}
		*/
	  }
	   return 0; 
}
// static int simple_streamer_plugin_deinit() {
extern "C" int simple_streamer_plugin_deinit() {
	  Aws::ShutdownAPI(options); //the plugin will be  uninstalled from this call
 	return 0;
}
//the plugin structure architecture
extern "C" mysql_declare_plugin(Binary_Stream) {
  MYSQL_AUDIT_PLUGIN,      /* type                            */
  &bin_log_stream_descriptor,  /* descriptor                      */
  "Binary_Log_Streamer", /* name                            */
  "Chris Voncina",       	  /* author                          */
  "Simple Binary log streaming app that goes from mysql to amazon kinesis",  /* description                     */
  PLUGIN_LICENSE_GPL,         /* plugin license                  */
  simple_streamer_plugin_init,  /* init function for read (when loaded)     */
  simple_streamer_plugin_deinit,/* deinit function (when unloaded) for read*/
  0x0001,                     /* version                         */
  simple_status,              /* status variables                */
  NULL,    /* system variables                */
  NULL,
  0	
} 
mysql_declare_plugin_end;
