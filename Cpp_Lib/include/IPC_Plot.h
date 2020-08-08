#pragma once

#ifdef LIB_EXPORT
#    define LIB_API __declspec(dllexport)
#else
#    define LIB_API __declspec(dllimport)
#endif

#define LIB_MAX_DATA_SIZE 131072 //!< 1 MB of doubles over pipe
#define LIB_MAX_BUFFER_SIZE 1024 //!< Maximum buffer size for error messages
#define LIB_MAX_LABEL_SIZE 128   //!< Maximum buffer size for data column labels

#define LIB_OK 0
#define LIB_ERR 1

#include "IPC_Plot_Error.h"

//!<  Datatypes
typedef char                LIB_CHAR;   //!< 8-bit signed char
typedef unsigned int		LIB_U32;    //!< 32-bit unsigned integer
typedef int 		        LIB_INT32;  //!< 32-bit signed integer
typedef float				LIB_FLOAT;  //!< 32-bit single-precision float
typedef double				LIB_DOUBLE; //!< 64-bit double-precision double 
typedef enum LIB_BOOLEAN { LIB_FALSE, LIB_TRUE } LIB_BOOLEAN;

typedef struct LIB_INPUT
{
	LIB_U32 u32ColSize;           //!< Number of columns for plotting
	LIB_U32 u32RowSize;           //!< Number of rows of data each column
	const LIB_CHAR** prgszLabels; //!< Pointer to char array with labels for each column
	LIB_DOUBLE* prgdBuffer;       //!< Pointer to double-precision data buffer 
	LIB_INPUT()
	{
		memset(this, 0, sizeof(*this));
	}
} LIB_INPUT;

typedef struct LIB_ERROR_INFO
{
	LIB_U32 u32ErrCode;                      //!< Refer to IPC_Plot_Error.h for error codes
	LIB_CHAR szErrMsg[LIB_MAX_BUFFER_SIZE];  //!< Error description
	LIB_CHAR szErrHelp[LIB_MAX_BUFFER_SIZE]; //!< Suggested solutions to error
	LIB_CHAR szRuntime[LIB_MAX_BUFFER_SIZE]; //!< Runtime message of error for more details
	LIB_ERROR_INFO()
	{
		memset(this, 0, sizeof(*this));
	}
} LIB_ERROR_INFO;

/***************************************************************************//**
 * ipc_plot
 *
 * API for interfacing the Python tool. Plots data from a buffer and into a 
 * graph figure and saves the figure created as an image file.
 *
 * @param pstInput Input structure including data buffer and labels
 * @param pstErr   Error information structure for logging any errors
 * @return         LIB_OK if success, else LIB_ERR if any error occurred
 ******************************************************************************/
LIB_U32 LIB_API ipc_plot(LIB_INPUT* pstInput, LIB_ERROR_INFO* pstErr);
