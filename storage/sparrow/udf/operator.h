#ifndef _operator_h_
#define _operator_h_

#include "udfargument.h"

// comment out to log values in directory : LOG_REPOSITORY_OPERATOR
//#define LOG_OPERATOR

#ifdef LOG_OPERATOR
#ifndef LOG_REPOSITORY_OPERATOR
// directory where files are written
#define LOG_REPOSITORY_OPERATOR "D:/debug"
#endif LOG_REPOSITORY_OPERATOR
#endif //#ifdef LOG_OPERATOR

////////////////////////////////////////////////////////////////////////////////
// OperatorData
////////////////////////////////////////////////////////////////////////////////

template<class T, class U>
class OperatorData
{
public:

	// constructor
	OperatorData();

	// virtual destructor
	virtual ~OperatorData();

	// return determiner
	const T& getDeterminer() const;

	// return indicator
	const U& getIndicator() const;

	// empty the structure
	void empty();
	// tell whether structure is empty or not (has been used to store values)
	bool isEmpty() const;

	// update determiner and indicator if determiner is greater than the stored one
	void updateIfGreater(UDF_ARGS* args);

#ifdef LOG_OPERATOR
	// log current determiner and indicator
	void logContent();
#endif //#ifdef LOG_OPERATOR

protected:
	// the maximum reached value of the determiner
	T determiner_;

	// the indicator value that must be finally returned
	U indicator_;

	// flag indicating whether the structure has been used at least one time and so contains valid values
	bool empty_;

#ifdef LOG_OPERATOR
	// create a log file
	void createLogFile(const char* szLogFilePath);
	// close a log file
	void closeLogFile();
	// write data type
	static void writeType(FILE * fp, enum Item_result arg_type);
	// write determiner
	static void writeDeterminer(FILE * fp, const T& determiner);
	// write indicator
	static void writeIndicator(FILE * fp, const U& indicator);
	// write determiner and indicator
	static void writeValues(FILE * fp, const T& determiner, const U& indicator);
	// log input
	static void logInput(FILE * fp, const T& determiner, const U& indicator);
	// file handler
	FILE * fp_;
#endif //#ifdef LOG_OPERATOR
};




////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
// OperatorData
////////////////////////////////////////////////////////////////////////////////

// constructor
template<class T, class U> 
inline OperatorData<T,U>::OperatorData()
{
	empty_ = true;
#ifdef LOG_OPERATOR
	// create log file
	char filepath[1024];	
	clock_t mytime = clock();
	sprintf(filepath,"%s/udf_operator_%ld.log",LOG_REPOSITORY_OPERATOR,mytime);
	OperatorData<T,U>::createLogFile(filepath);
#endif //#ifdef LOG_OPERATOR
}

// destructor
template<class T, class U> 
inline OperatorData<T,U>::~OperatorData()
{
#ifdef LOG_OPERATOR
	closeLogFile();
#endif //#ifdef LOG_OPERATOR
}

// return determiner
template<class T, class U> 
inline const T& OperatorData<T,U>::getDeterminer() const
{
	return determiner_;
}

// return indicator
template<class T, class U> 
inline const U& OperatorData<T,U>::getIndicator() const
{
	return indicator_;
}

// empty the structure
template<typename T, typename U> 
inline void OperatorData<T,U>::empty()
{
	empty_ = true;
#ifdef LOG_OPERATOR
	// update log file
	if (fp_ == NULL) return;

	clock_t mytime = clock();
	fprintf(fp_,"\n\n===== EMPTY =====\n");
	fprintf(fp_,"\n%ld\n",mytime);
	fflush(fp_);
#endif //#ifdef LOG_OPERATOR
}

// tell whether structure is empty or not (has been used to store values)
template<typename T, typename U> 
inline bool OperatorData<T,U>::isEmpty() const
{
	return empty_;
}

// update determiner and indicator if determiner is greater than the stored one
template<typename T, typename U> 
inline void OperatorData<T,U>::updateIfGreater(UDF_ARGS* args)
{
	// Test arguments
	if (args == NULL) return;

	// If no new determiner value, can't do anything
	if (args->args[1] == NULL) return;

	// Retrieves new determiner value
	T newDeterminer(args->args[1],args->lengths[1]);

#ifdef LOG_OPERATOR
	U logIndicator(args->args[0],args->lengths[0]);
	OperatorData<T,U>::logInput(fp_,newDeterminer,logIndicator);
#endif //#ifdef LOG_OPERATOR

	// If first pass, store determiner and indicator
	if(empty_) {

		// Store determiner and indicator
		determiner_ = newDeterminer;
		indicator_.setValue(args->args[0],args->lengths[0]);
		empty_ = false;

#ifdef LOG_OPERATOR
		logContent();
#endif //#ifdef LOG_OPERATOR

	} else {
		// Test if new determiner is greater than the stored one
		//if (newDeterminer.getValue() > determiner_.getValue()) {
		if (newDeterminer > determiner_) {

			// Store determiner and indicator
			determiner_ = newDeterminer;
			indicator_.setValue(args->args[0],args->lengths[0]);

#ifdef LOG_OPERATOR
			logContent();
#endif //#ifdef LOG_OPERATOR

		}
	}
}

// create a log file
#ifdef LOG_OPERATOR
template<typename T, typename U> 
inline void OperatorData<T,U>::createLogFile(const char* szLogFilePath)
{
	fp_ = fopen(szLogFilePath,"w");

	if (fp_ == NULL) return;

	clock_t mytime = clock();
	fprintf(fp_,"\n\n===== CREATE =====\n");
	fprintf(fp_,"%ld\n",mytime);

	fflush(fp_);
}

// close a log file
template<typename T, typename U> 
inline void OperatorData<T,U>::closeLogFile()
{
	if (fp_ == NULL) return;

	fclose(fp_);
}

// write data type
template<typename T, typename U> 
inline void OperatorData<T,U>::writeType(FILE * fp, enum Item_result argType)
{
	if (fp == NULL) return;

	switch(argType) {
	case STRING_RESULT:
		fprintf(fp,"\t STRING_RESULT");
		break;
	case REAL_RESULT:
		fprintf(fp,"\t REAL_RESULT");
		break;
	case INT_RESULT:
		fprintf(fp,"\t INT_RESULT");
		break;
	case ROW_RESULT:
		fprintf(fp,"\t ROW_RESULT");
		break;
	case DECIMAL_RESULT:
		fprintf(fp,"\t DECIMAL_RESULT");
		break;
	default:
		break;
	}
	fflush(fp);
}

// write determiner
template<typename T, typename U> 
inline void OperatorData<T,U>::writeDeterminer(FILE * fp, const T & input)
{
	if (fp == NULL) return;

	fprintf(fp,"\tdeterminer: ");
	switch(input.getType()) {
	case STRING_RESULT:
		if (input.isNull())
		{
			fprintf(fp,"\t null");
		}
		else
		{
			fprintf(fp,"\t %s",input.getValue());
		}
		fprintf(fp,"\t STRING_RESULT");
		break;
	case REAL_RESULT:
		if (input.isNull())
		{
			fprintf(fp,"\t null");
		}
		else
		{
			fprintf(fp,"\t %lf",input.getValue());
		}
		fprintf(fp,"\t REAL_RESULT");
		break;
	case INT_RESULT:
		if (input.isNull())
		{
			fprintf(fp,"\t null");
		}
		else
		{
			fprintf(fp,"\t %lld",input.getValue());
		}
		fprintf(fp,"\t INT_RESULT");
		break;
	case ROW_RESULT:
		if (input.isNull())
		{
			fprintf(fp,"\t null");
		}
		else
		{
			fprintf(fp,"\t ROW_RESULT");
		}
		break;
	case DECIMAL_RESULT:
		if (input.isNull())
		{
			fprintf(fp,"\t null");
		}
		else
		{
			fprintf(fp,"\t %s",input.getValue());
		}
		fprintf(fp,"\t DECIMAL_RESULT");
		break;
	default:
		break;
	}
	fflush(fp);
}

// write indicator
template<typename T, typename U> 
inline void OperatorData<T,U>::writeIndicator(FILE * fp, const U & input)
{
	if (fp == NULL) return;

	fprintf(fp,"\tindicator: ");
	switch(input.getType()) {
	case STRING_RESULT:
		if (input.isNull())
		{
			fprintf(fp,"\t null");
		}
		else
		{
			fprintf(fp,"\t %s",input.getValue());
		}
		fprintf(fp,"\t STRING_RESULT");
		break;
	case REAL_RESULT:
		if (input.isNull())
		{
			fprintf(fp,"\t null");
		}
		else
		{
			fprintf(fp,"\t %lf",input.getValue());
		}
		fprintf(fp,"\t REAL_RESULT");
		break;
	case INT_RESULT:
		if (input.isNull())
		{
			fprintf(fp,"\t null");
		}
		else
		{
			fprintf(fp,"\t %lld",input.getValue());
		}
		fprintf(fp,"\t INT_RESULT");
		break;
	case ROW_RESULT:
		if (input.isNull())
		{
			fprintf(fp,"\t null");
		}
		else
		{
			fprintf(fp,"\t ROW_RESULT");
		}
		break;
	case DECIMAL_RESULT:
		if (input.isNull())
		{
			fprintf(fp,"\t null");
		}
		else
		{
			fprintf(fp,"\t %s",input.getValue());
		}
		fprintf(fp,"\t DECIMAL_RESULT");
		break;
	default:
		break;
	}
	fflush(fp);
}

// write determiner and indicator
template<typename T, typename U> 
inline void OperatorData<T,U>::writeValues(FILE * fp, const T& determiner, const U& indicator)
{
	if (fp == NULL) return;

	OperatorData<T,U>::writeDeterminer(fp, determiner);
	OperatorData<T,U>::writeIndicator(fp, indicator);
}

// log input
template<typename T, typename U> 
inline void OperatorData<T,U>::logInput(FILE * fp, const T& determiner, const U& indicator)
{
	if (fp == NULL) return;

	fprintf(fp,"\nInput");
	OperatorData<T,U>::writeValues(fp, determiner, indicator);
}

// log current determiner and indicator
template<typename T, typename U> 
inline void OperatorData<T,U>::logContent()
{
	if (fp_ == NULL) return;

	fprintf(fp_,"\nStored values");
	OperatorData<T,U>::writeValues(fp_, determiner_, indicator_);
}
#endif //#ifdef LOG_OPERATOR

#endif //#define _operator_h_
