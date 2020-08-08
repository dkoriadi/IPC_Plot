#pragma once

#ifndef _IPC_PLOT_ERROR_H_
#define _IPC_PLOT_ERROR_H_

//!< Use high 16-bit
#define LIB_ERR_INPUT_PTR_NULL                  0x00010000
#define LIB_ERR_INPUT_PTR_NULL_MSG              "Input is a null pointer"
#define LIB_ERR_INPUT_PTR_NULL_ACT              "Check the input of the function"

#define LIB_ERR_CLIENT_NOT_FOUND                0x00020000
#define LIB_ERR_CLIENT_NOT_FOUND_MSG            "The Python client could not be found"
#define LIB_ERR_CLIENT_NOT_FOUND_ACT            "Check the directory again"

#define LIB_ERR_WIN_API_ERR                     0x00030000
#define LIB_ERR_WIN_API_ERR_MSG                 "Error occurred while calling WinAPI"
#define LIB_ERR_WIN_API_ERR_ACT                 "GetLastError() returned with "

#define LIB_ERR_BUFFER_OVERFLOW                 0x00040000
#define LIB_ERR_BUFFER_OVERFLOW_MSG             "The buffer size is not large enough"
#define LIB_ERR_BUFFER_OVERFLOW_ACT             "Reduce the input size"

#define LIB_ERR_SERVER_TIMEOUT                  0x00050000
#define LIB_ERR_SERVER_TIMEOUT_MSG              "The Python client failed to connect to the named pipe within timeout period"
#define LIB_ERR_SERVER_TIMEOUT_ACT              "Check the Python client and the named pipe is configured correctly"

#define LIB_ERR_NOT_FIRST_ERROR                 0x00060000
#define LIB_ERR_NOT_FIRST_ERROR_MSG             "An error occurred earlier without logging any error info"
#define LIB_ERR_NOT_FIRST_ERROR_ACT             "Ensure the error is logged properly"

#endif //_IPC_PLOT_ERROR_H_
