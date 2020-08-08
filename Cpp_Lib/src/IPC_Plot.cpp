#include <stdio.h>
#include <windows.h>

#include "IPC_Plot_Internal.h"

LIB_INT32 GetWinAPIErrMessage(DWORD dwErrCode, LIB_CHAR* pszOutWinAPIErrorMsg, LIB_ERROR_INFO* pstErr);
LIB_INT32 ReadWritePipe(LIB_PIPE_INFO* pstPipeInfo, LIB_INPUT* pstInput, LIB_ERROR_INFO* pstErr);

#define PYTHON_PATH ".\\..\\Python\\IPC_Plot.py"

#define __AT__  __FILE__ , __LINE__

#define LOG_ERROR(pstERR_Out, u32CODE_In, pcERRDEF_In, pcERRACTION_In, pcRUNTIME_In)    \
    if (pstERR_Out->u32ErrCode == 0) \
    { \
    	pstERR_Out->u32ErrCode = u32CODE_In;\
    	sprintf(pstERR_Out->szErrMsg, "%s", pcERRDEF_In);\
    	sprintf(pstERR_Out->szErrHelp, "%s", pcERRACTION_In);\
    	snprintf(pstERR_Out->szRuntime, sizeof(pstERR_Out->szRuntime), "%s (%s, %d)", pcRUNTIME_In, __AT__); \
    }

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
LIB_U32 ipc_plot(LIB_INPUT* pstInput, LIB_ERROR_INFO* pstErr)
{
	LIB_CHAR szRuntimeMsg[LIB_MAX_BUFFER_SIZE] = "";

	// Check for null pointers
	if (pstErr == NULL)
	{
		return LIB_ERR;
	}
	if (pstInput == NULL || pstInput->prgdBuffer == NULL || pstInput->prgszLabels == NULL)
	{
		snprintf(szRuntimeMsg, sizeof(szRuntimeMsg), "Input struct is null pointer");
		LOG_ERROR(pstErr, LIB_ERR_INPUT_PTR_NULL, LIB_ERR_INPUT_PTR_NULL_MSG, LIB_ERR_INPUT_PTR_NULL_ACT, szRuntimeMsg)
		return LIB_ERR;
	}

	// Check for buffer overflow
	if (pstInput->u32RowSize * pstInput->u32ColSize > LIB_MAX_DATA_SIZE)
	{
		snprintf(szRuntimeMsg, sizeof(szRuntimeMsg), "Buffer size of %d is larger than maximum of %d", 
			pstInput->u32RowSize * pstInput->u32ColSize, LIB_MAX_DATA_SIZE);
		LOG_ERROR(pstErr, LIB_ERR_BUFFER_OVERFLOW, LIB_ERR_BUFFER_OVERFLOW_MSG, LIB_ERR_BUFFER_OVERFLOW_ACT, szRuntimeMsg)
		return LIB_ERR;
	}

	// Check if Python tool is found
	DWORD dwFileAttribs = GetFileAttributesA(PYTHON_PATH);
	if (dwFileAttribs == INVALID_FILE_ATTRIBUTES)
	{
		// Python tool not found 
		snprintf(szRuntimeMsg, sizeof(szRuntimeMsg), "No Python tool in %s", PYTHON_PATH);
		LOG_ERROR(pstErr, LIB_ERR_CLIENT_NOT_FOUND, LIB_ERR_CLIENT_NOT_FOUND_MSG, LIB_ERR_CLIENT_NOT_FOUND_ACT, szRuntimeMsg)
		return LIB_ERR;
	}

	// Dereference data in buffer and user labels in char array into structs for named pipe
	LIB_BUFFER_ST* stDataArray = (LIB_BUFFER_ST*)malloc(LIB_MAX_DATA_SIZE * sizeof(LIB_BUFFER_ST));
	LIB_LABEL_ST* stLabelArray = (LIB_LABEL_ST*)malloc(pstInput->u32ColSize * sizeof(LIB_LABEL_ST));
	LIB_U32 u32BufferSize = pstInput->u32RowSize * pstInput->u32ColSize;
	for (LIB_U32 u32Offset = 0; u32Offset < u32BufferSize; u32Offset++)
	{
		stDataArray[u32Offset].dData = pstInput->prgdBuffer[u32Offset];
	}

	for (LIB_U32 u32Offset = 0; u32Offset < pstInput->u32ColSize; u32Offset++)
	{
		strncpy(stLabelArray[u32Offset].szLabel, pstInput->prgszLabels[u32Offset],
			sizeof(stLabelArray[u32Offset].szLabel) - 1);
	}

	// Run the Python tool via CreateProcess()
	LIB_CHAR szCmdLine[LIB_MAX_BUFFER_SIZE] = "python.exe ";
	strncat(szCmdLine, PYTHON_PATH, LIB_MAX_BUFFER_SIZE - strlen(szCmdLine) - 1);

	STARTUPINFO si = { sizeof(si) };
	PROCESS_INFORMATION pi;

	// memset() to 0
	ZeroMemory(&si, sizeof(si));
	ZeroMemory(&pi, sizeof(pi));

	if (!CreateProcess(NULL,   // No module name (use command line)
		(LPTSTR)szCmdLine,     // Command line (<exename> <arg1> <arg2> etc)
		NULL,                  // Process handle not inheritable
		NULL,                  // Thread handle not inheritable
		TRUE,                  // Set handle inheritance to FALSE
		0,                     // No creation flags
		NULL,                  // Use parent's environment block
		NULL,                  // Use parent's starting directory 
		&si,                   // Pointer to STARTUPINFO structure
		&pi)                   // Pointer to PROCESS_INFORMATION structure
		)
	{
		// CreateProcess() error
		LIB_CHAR szErrHelpString[LIB_MAX_BUFFER_SIZE] = LIB_ERR_WIN_API_ERR_ACT;
		GetWinAPIErrMessage(GetLastError(), szErrHelpString, pstErr);
		snprintf(szRuntimeMsg, sizeof(szRuntimeMsg), "Error from the Python tool at %s", PYTHON_PATH);
		LOG_ERROR(pstErr, LIB_ERR_WIN_API_ERR, LIB_ERR_WIN_API_ERR_MSG, szErrHelpString, szRuntimeMsg)
		free(stDataArray);
		free(stLabelArray);
		return LIB_ERR;
	}

	CloseHandle(pi.hThread);

	LIB_PIPE_INFO stPipeInfo;
	HANDLE hNamedPipe;
	DWORD dwszInputBuffer = sizeof(LIB_BUFFER_ST) * LIB_MAX_DATA_SIZE;
	DWORD dwszOutputBuffer = sizeof(LIB_ERROR_INFO);

	// Get the process ID to create the unique pipe name
	LIB_CHAR szProcessID[10];
	sprintf(szProcessID, "%d", GetProcessId(pi.hProcess));

	// Named pipes steps (labelled in numerical order)
	// 01. CreateNamedPipe() to create the named pipe
	// The name of the pipe is based on the process ID of the Python tool
	LIB_CHAR szNamedPipe[64] = "\\\\.\\pipe\\";
	strcat(szNamedPipe, szProcessID);
	hNamedPipe = CreateNamedPipe(
		(LPCSTR)szNamedPipe, // Name of the pipe, unique for each application and client instance
		// PIPE_ACCESS_DUPLEX: Both server and client processes can read from and write to the pipe
		// FILE_FLAG_OVERLAPPED: Prevent server from waiting infinitely for a client to connect	
		PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
		// PIPE_TYPE_MESSAGE: The pipe treats the bytes written during each write operation as a message unit. 
		// PIPE_READMODE_MESSAGE: Data is read from the pipe as a stream of messages
		// PIPE_WAIT: Blocking mode - wait until client connects to named pipe (default) or timeout
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
		// Maximum number of instances that can be created for this pipe (from 1 to 255)
		1,
		// The non-paged pool (physical memory used by the kernel) is used to create the size of the pipe
		// Insufficient buffer will just block the write operation until buffer (the non-paged pool) is expanded by OS
		dwszOutputBuffer,
		dwszInputBuffer,
		0, // How long to wait until the named pipe is successfully created. Default value of 50ms
		NULL // Prevent handle (hNamedPipe) from being inherited
	);

	if (hNamedPipe == NULL)
	{
		CloseHandle(pi.hProcess);
		free(stDataArray);
		free(stLabelArray);
		// Error creating the pipe
		LIB_CHAR szErrHelpString[LIB_MAX_BUFFER_SIZE] = LIB_ERR_WIN_API_ERR_ACT;
		GetWinAPIErrMessage(GetLastError(), szErrHelpString, pstErr);
		snprintf(szRuntimeMsg, sizeof(szRuntimeMsg), "Error from the Python tool at %s", PYTHON_PATH);
		LOG_ERROR(pstErr, LIB_ERR_WIN_API_ERR, LIB_ERR_WIN_API_ERR_MSG, szErrHelpString, szRuntimeMsg)
		return LIB_ERR;
	}

	if (hNamedPipe == INVALID_HANDLE_VALUE && hNamedPipe != NULL)
	{
		CloseHandle(hNamedPipe);
		CloseHandle(pi.hProcess);
		free(stDataArray);
		free(stLabelArray);
		// Error creating the pipe
		LIB_CHAR szErrHelpString[LIB_MAX_BUFFER_SIZE] = LIB_ERR_WIN_API_ERR_ACT;
		GetWinAPIErrMessage(GetLastError(), szErrHelpString, pstErr);
		snprintf(szRuntimeMsg, sizeof(szRuntimeMsg), "Error from the Python tool at %s", PYTHON_PATH);
		LOG_ERROR(pstErr, LIB_ERR_WIN_API_ERR, LIB_ERR_WIN_API_ERR_MSG, szErrHelpString, szRuntimeMsg)
		return LIB_ERR;
	}

	stPipeInfo.hNamedPipe = hNamedPipe;
	stPipeInfo.hChildProcess = pi.hProcess;
	stPipeInfo.pstDataArray = stDataArray;
	stPipeInfo.pstLabelArray = stLabelArray;

	while (1)
	{
		ReadWritePipe(&stPipeInfo, pstInput, pstErr);		
		if (pstErr->u32ErrCode != 0)
		{
			break;
		}
	}
	// 07. Free resources (handles) after server is done with the named pipe
	CloseHandle(pi.hProcess);
	CloseHandle(hNamedPipe);
	free(stDataArray);
	free(stLabelArray);

	if (pstErr->u32ErrCode != 0x0000FFFF)
	{
		return LIB_ERR;
	}
	return LIB_OK;
}

/***************************************************************************//**
 * ReadWritePipe
 *
 * Sends and receives data from the named pipe
 *
 * @param pstPipeInfo  Internal struct including handles and pointer to the 
 *                     array of structs
 * @param pstInput     Input structure for denoting row and col sizes
 * @param pstErr       Error information structure for logging any errors
 * @return             LIB_OK if success, else LIB_ERR if any error occurred
 ******************************************************************************/
LIB_INT32 ReadWritePipe(LIB_PIPE_INFO* pstPipeInfo, LIB_INPUT* pstInput, LIB_ERROR_INFO* pstErr)
{
	LIB_CHAR szRuntimeMsg[LIB_MAX_BUFFER_SIZE] = "";

	DWORD dwErr, dwOverlappedResult, dwResult;
	// For WriteFile()
	DWORD dwWriteBufferSizeInput = sizeof(LIB_INPUT);
	DWORD dwWriteBufferSizeData = sizeof(LIB_BUFFER_ST) * LIB_MAX_DATA_SIZE;
	DWORD dwWriteBufferSizeLabels = sizeof(LIB_LABEL_ST) * pstInput->u32ColSize;
	// For ReadFile()
	DWORD dwReadBufferSize = sizeof(LIB_ERROR_INFO);

	// Struct contains information used in asynchronous (a.k.a. overlapped) input and output
	// Used to add the event flag to the struct
	OVERLAPPED stOverlapped = { 0 };
	// Create event flag to check if client has connected
	stOverlapped.hEvent = CreateEvent(NULL,  // SECURITY_ATTRIBUTES, cannot be inherited by child processes
									  TRUE,  // Requires ResetEvent() to manually set the event to non-signalled (wait)
									  FALSE, // Initial state of event is non-signalled (wait)
									  NULL); // Name of event object is not set

	// 02. ConnectNamedPipe() to check if the client has connected to the pipe
	// If not, wait for 10 seconds until timeout
	if (!ConnectNamedPipe(pstPipeInfo->hNamedPipe, &stOverlapped))
	{
		dwErr = GetLastError();
		if (!(dwErr == ERROR_IO_PENDING || dwErr == ERROR_PIPE_LISTENING))
		{
			CloseHandle(stOverlapped.hEvent);
			// Some other error occurred while connecting server to pipe
			LIB_CHAR szErrHelpString[LIB_MAX_BUFFER_SIZE] = LIB_ERR_WIN_API_ERR_ACT;
			GetWinAPIErrMessage(GetLastError(), szErrHelpString, pstErr);
			snprintf(szRuntimeMsg, sizeof(szRuntimeMsg), "ConnectNamedPipe() failed");
			LOG_ERROR(pstErr, LIB_ERR_WIN_API_ERR, LIB_ERR_WIN_API_ERR_MSG, szErrHelpString, szRuntimeMsg)
			return LIB_ERR;
		}
		// Keep the server pending for 10 seconds, or until client connects within 10 seconds
		LIB_INT32 nServerWaitTimeMs = 10000; 
		WaitForSingleObject(stOverlapped.hEvent, nServerWaitTimeMs);
		// Check if event flag is signalled
		if (!GetOverlappedResult(pstPipeInfo->hNamedPipe, // The handle to the named pipe
								 &stOverlapped,           // A pointer to an OVERLAPPED structure
								 &dwOverlappedResult,     // Not used: Receives the number of bytes that were
														  // transferred by a read or write operation
								 FALSE))                  // Allow function to return while server is pending
		{
			dwErr = GetLastError();
			if (!(dwErr == ERROR_IO_INCOMPLETE))
			{
				CloseHandle(stOverlapped.hEvent);
				// Some other error occurred while connecting server to pipe
				LIB_CHAR szErrHelpString[LIB_MAX_BUFFER_SIZE] = LIB_ERR_WIN_API_ERR_ACT;
				GetWinAPIErrMessage(GetLastError(), szErrHelpString, pstErr);
				snprintf(szRuntimeMsg, sizeof(szRuntimeMsg), "ConnectNamedPipe() failed");
				LOG_ERROR(pstErr, LIB_ERR_WIN_API_ERR, LIB_ERR_WIN_API_ERR_MSG, szErrHelpString, szRuntimeMsg)
				return LIB_ERR;
			}
			// Timeout lapsed, terminate process before reporting error
			TerminateProcess(pstPipeInfo->hChildProcess, 0);
			CloseHandle(stOverlapped.hEvent);
			snprintf(szRuntimeMsg, sizeof(szRuntimeMsg), "No response from the Python tool after %d seconds", nServerWaitTimeMs / 1000);
			LOG_ERROR(pstErr, LIB_ERR_SERVER_TIMEOUT, LIB_ERR_SERVER_TIMEOUT_MSG, LIB_ERR_SERVER_TIMEOUT_ACT, szRuntimeMsg)
			return LIB_ERR;
		}
	}

	// 03(a). WriteFile() to send input struct to Python tool
	if (!(WriteFile(
		pstPipeInfo->hNamedPipe, // The handle to the named pipe
		pstInput,                // A pointer to the buffer that sends the data to the named pipe
		dwWriteBufferSizeInput,  // The number of bytes to be written to the file 
		&dwResult,               
		NULL                     
	)))
	{
		CloseHandle(stOverlapped.hEvent);
		// Error writing to named pipe
		LIB_CHAR szErrHelpString[LIB_MAX_BUFFER_SIZE] = LIB_ERR_WIN_API_ERR_ACT;
		GetWinAPIErrMessage(GetLastError(), szErrHelpString, pstErr);
		snprintf(szRuntimeMsg, sizeof(szRuntimeMsg), "WriteFile() failed - error writing to pipe");
		LOG_ERROR(pstErr, LIB_ERR_WIN_API_ERR, LIB_ERR_WIN_API_ERR_MSG, szErrHelpString, szRuntimeMsg)
		return LIB_ERR;
	}

	// 03(B). WriteFile() to send data to Python tool
	if (!(WriteFile(
		pstPipeInfo->hNamedPipe,   // The handle to the named pipe
		pstPipeInfo->pstDataArray, // A pointer to the buffer that sends the data to the named pipe
		dwWriteBufferSizeData,     // The number of bytes to be written to the file 
		&dwResult,                 
		NULL                       
	)))
	{
		CloseHandle(stOverlapped.hEvent);
		// Error writing to named pipe
		LIB_CHAR szErrHelpString[LIB_MAX_BUFFER_SIZE] = LIB_ERR_WIN_API_ERR_ACT;
		GetWinAPIErrMessage(GetLastError(), szErrHelpString, pstErr);
		snprintf(szRuntimeMsg, sizeof(szRuntimeMsg), "WriteFile() failed - error writing to pipe");
		LOG_ERROR(pstErr, LIB_ERR_WIN_API_ERR, LIB_ERR_WIN_API_ERR_MSG, szErrHelpString, szRuntimeMsg)
		return LIB_ERR;
	}

	// 03(c). WriteFile() to send user labels to Python tool
	if (!(WriteFile(
		pstPipeInfo->hNamedPipe,    // The handle to the named pipe
		pstPipeInfo->pstLabelArray, // A pointer to the buffer that sends the data to the named pipe
		dwWriteBufferSizeLabels,    // The number of bytes to be written to the file 								   
		&dwResult,                  
		NULL                        
	)))
	{
		CloseHandle(stOverlapped.hEvent);
		// Error writing to named pipe
		LIB_CHAR szErrHelpString[LIB_MAX_BUFFER_SIZE] = LIB_ERR_WIN_API_ERR_ACT;
		GetWinAPIErrMessage(GetLastError(), szErrHelpString, pstErr);
		snprintf(szRuntimeMsg, sizeof(szRuntimeMsg), "WriteFile() failed - error writing to pipe");
		LOG_ERROR(pstErr, LIB_ERR_WIN_API_ERR, LIB_ERR_WIN_API_ERR_MSG, szErrHelpString, szRuntimeMsg)
		return LIB_ERR;
	}

	// 04. FlushFileBuffers() after writing data
	// FlushFileBuffers() ensure that all bytes or messages written to the pipe are read by the client
	if (!FlushFileBuffers(pstPipeInfo->hNamedPipe))
	{
		CloseHandle(stOverlapped.hEvent);
		// Error flushing buffer because client has not finished reading all bytes
		LIB_CHAR szErrHelpString[LIB_MAX_BUFFER_SIZE] = LIB_ERR_WIN_API_ERR_ACT;
		GetWinAPIErrMessage(GetLastError(), szErrHelpString, pstErr);
		snprintf(szRuntimeMsg, sizeof(szRuntimeMsg), "FlushFileBuffer() failed");
		LOG_ERROR(pstErr, LIB_ERR_WIN_API_ERR, LIB_ERR_WIN_API_ERR_MSG, szErrHelpString, szRuntimeMsg)
		return LIB_ERR;
	}

	// 05. ReadFile() to receieve status
	if (!(ReadFile(
		pstPipeInfo->hNamedPipe, // The handle to the named pipe
		pstErr,                  // A pointer to the buffer that receives the data from the named pipe
		dwReadBufferSize,        // The maximum number of bytes to be read
		&dwResult,               
		NULL                     
	)))
	{
		CloseHandle(stOverlapped.hEvent);
		// Error reading data from client
		LIB_CHAR szErrHelpString[LIB_MAX_BUFFER_SIZE] = LIB_ERR_WIN_API_ERR_ACT;
		GetWinAPIErrMessage(GetLastError(), szErrHelpString, pstErr);
		snprintf(szRuntimeMsg, sizeof(szRuntimeMsg), "ReadFile() failed - error reading from pipe");
		LOG_ERROR(pstErr, LIB_ERR_WIN_API_ERR, LIB_ERR_WIN_API_ERR_MSG, szErrHelpString, szRuntimeMsg)
		return LIB_ERR;
	}

	// 06. DisconnectNamedPipe() after the data is successfully sent
	// Any unread data in the pipe is discarded, and makes the client's named pipe handle invalid.
	// The server can use ConnectNamedPipe() to enable the client to connect again to this instance 
	// of the named pipe.
	DisconnectNamedPipe(pstPipeInfo->hNamedPipe);

	CloseHandle(stOverlapped.hEvent);
	return LIB_OK;
}

/***************************************************************************//**
 * GetWinAPIErrMessage
 *
 * Retrives the error message from GetLastError() and logs the message 
 * into the runtime message
 *
 * @param dwErrCode            Error code generated by GetLastError()
 * @param pszOutWinAPIErrorMsg Runtime message for WinAPI errors
 * @param pstErr               Error information structure for logging any errors
 * @return                     LIB_OK if success, else LIB_ERR if any error 
 *                             occurred
 ******************************************************************************/
LIB_INT32 GetWinAPIErrMessage(DWORD dwErrCode, LIB_CHAR* pszOutWinAPIErrorMsg, LIB_ERROR_INFO* pstErr)
{
	LPVOID lpMsgBuf;
	pstErr = pstErr;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
				  FORMAT_MESSAGE_FROM_SYSTEM |
				  FORMAT_MESSAGE_IGNORE_INSERTS,
				  NULL,
				  dwErrCode,
				  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				  (LPTSTR)&lpMsgBuf,
				  0, 
		          NULL);


	LIB_CHAR szErrorCodeBuf[LIB_MAX_BUFFER_SIZE] = "";
	sprintf(szErrorCodeBuf, "Error %d: ", dwErrCode);
	strncat(pszOutWinAPIErrorMsg, szErrorCodeBuf, LIB_MAX_BUFFER_SIZE - strlen(pszOutWinAPIErrorMsg) - 1);
	strncat(pszOutWinAPIErrorMsg, (LIB_CHAR*)lpMsgBuf, LIB_MAX_BUFFER_SIZE - strlen(pszOutWinAPIErrorMsg) - 1);
	return LIB_OK;
}