"""
IPC_Plot_Pipe.py

Summary
-------
This script acts as a client for a named pipe between a Windows C/C++ application (server) and this Python tool.

In the C/C++ application, a struct or an array of structs is created and passed to Python using named pipes in a 
byte stream. The Python tool then decodes the bytes stream and converts members of the struct to Python native 
types e.g. char buffer in C/C++ becomes string type in Python.

The client disconnects from the named pipe after all data has been sent and received. The client needs to 
create the thread and reconnect to the named pipe to send/receive more data.

"""
# Standard libraries
import ctypes
import os
import threading
import time
from io import BytesIO

# Modules for interfacing with Windows APIs
import win32file
import win32pipe
# Only import this after the first 2 imports
import pywintypes

# Constants to define size of struct and pipe
LIB_MAX_DATA_SIZE = 131072 # 1 MB of doubles over pipe
SIZEOF_1024CHAR_BUFFER = 1024
SIZEOF_512CHAR_BUFFER = 512
SIZEOF_128CHAR_BUFFER = 128
SIZEOF_UINT = 4
SIZEOF_DOUBLE = 8

# Define char array buffer sizes
CHAR_ARR_1024 = ctypes.c_char * 1024
CHAR_ARR_512 = ctypes.c_char * 512
CHAR_ARR_128 = ctypes.c_char * 128

# Templates of structs for decoding the byte stream 
class LIB_ERR_INFO(ctypes.Structure):
    """ 
    Error information structure 

    """
    _fields_ = [('u32ErrCode', ctypes.c_uint),
                ('szErrMsg', CHAR_ARR_1024), 
                ('szErrHelp', CHAR_ARR_1024),
                ('szRuntime', CHAR_ARR_1024)]

class LIB_INPUT(ctypes.Structure):
    """ 
    Input structure 

    """
    _fields_ = [('u32ColSize', ctypes.c_uint),
                ('u32RowSize', ctypes.c_uint), 
                ('prgszLabels', ctypes.POINTER(ctypes.POINTER(ctypes.c_char))),
                ('prgdBuffer', ctypes.POINTER(ctypes.c_double))]

class LIB_BUFFER_ST(ctypes.Structure):
    """ 
    Data
    Maximum buffer size defined by LIB_MAX_DATA_SIZE

    """
    _fields_ = [('dData', ctypes.c_double)]

class LIB_LABEL_ST(ctypes.Structure):
    """ 
    User labels

    """
    _fields_ = [('szLabel', CHAR_ARR_128)]

def retrieveData():
    """
    Initial connection to named pipe to retrieve data
    Create the thread to send data back to application. Thread blocks until 
    client pipe is disconnected.    

    Returns
    -------
    nColSize : int
        Number of columns in the data buffer. Corresponds to number of
        lines plotted on the figure
    
    nRowSize : int
        Number of rows of data for each column.

    lstData : list
        The data buffer containing doubles only

    lstGraphLabels : list
        A list of user labels to be displayed on the legend of the 
        figure  

    """
    stErrInfo = LIB_ERR_INFO() 
    lstData = []
    lstColRowSize = []
    lstGraphLabels = []
    thread = threading.Thread(target=_createPipe, args=(stErrInfo, lstData, 
        lstColRowSize, lstGraphLabels, ))
    thread.start() 
    thread.join() 
    nColSize = lstColRowSize[0]
    nRowSize = lstColRowSize[1]
    return nColSize, nRowSize, lstData, lstGraphLabels

def updateStatus():
    """
    Update status of Python tool upon completion
    Create the thread to send data back to application. Thread blocks until 
    client pipe is disconnected.      

    """
    stErrInfo = LIB_ERR_INFO() 
    stErrInfo.u32ErrCode = 0x0000FFFF
    thread = threading.Thread(target=_createPipe, args=(stErrInfo,))
    thread.start() 
    thread.join() 

def _createPipe(stErrInfo, lstData=None, lstColRowSize=None, lstGraphLabels=None):
    """
    Connect to the existing named pipe based on the process ID of this Python tool.
    The process ID is used for naming the named pipe so that multiple instances of
    the Python tool can be executed concurrently.

    """
    bIsComplete = False
    pid = os.getpid() 
    # Maximum size of pipe is the data structs of doubles
    nSizeofPipe = LIB_MAX_DATA_SIZE * (SIZEOF_DOUBLE)
    szNamedPipe = "\\\\.\\pipe\\" + str(pid)

    stInput = LIB_INPUT() 
    stData = LIB_BUFFER_ST() 
    stLabel = LIB_LABEL_ST()

    # Only close the pipe once the data is successfully received, or some error happened
    while not bIsComplete:
        try:
            # A handle to the named pipe is returned
            handle = win32file.CreateFile(
                szNamedPipe,
                win32file.GENERIC_READ | win32file.GENERIC_WRITE,
                0,
                None,
                win32file.OPEN_EXISTING,
                0,
                None
            )

            # Set the named pipe to read mode using the handle
            bResult = win32pipe.SetNamedPipeHandleState(
                handle, 
                win32pipe.PIPE_READMODE_MESSAGE | win32pipe.PIPE_WAIT, 
                None, 
                None)

            # ReadFile() returns a tuple
            # First item is integer denoting return value (success or failure)
            # Second item in the tuple is the byte stream of struct
            tupleBytesStream = win32file.ReadFile(handle, nSizeofPipe)

            # Create a buffered stream using the bytes stream from the named pipe 
            file = BytesIO(tupleBytesStream[1])
            # Get the number of rows and columns from input struct
            file.readinto(stInput)   
            if (lstColRowSize is not None):
                lstColRowSize.append(stInput.u32ColSize)
                lstColRowSize.append(stInput.u32RowSize)     

            # Retrieving all data from array of structs
            tupleBytesStream = win32file.ReadFile(handle, nSizeofPipe)
            file = BytesIO(tupleBytesStream[1])            
            if (lstData is not None):
                while file.readinto(stData):
                    lstData.append(stData.dData)                    
                    if (len(lstData) == stInput.u32ColSize * stInput.u32RowSize):
                        break
            
            # Retrieving all user labels from array of structs
            tupleBytesStream = win32file.ReadFile(handle, nSizeofPipe)
            file = BytesIO(tupleBytesStream[1])            
            if (lstGraphLabels is not None):
                while file.readinto(stLabel):
                    lstGraphLabels.append(stLabel.szLabel.decode("utf-8"))
                    if (len(lstGraphLabels) == stInput.u32ColSize):
                        break

            # Write to named pipe error info struct
            win32file.WriteFile(handle, stErrInfo)
            bIsComplete = True

        except pywintypes.error as e:            
            if e.args[0] == 2:
                # Named pipe connected but server not found
                print("Pipe not found, exiting")
                bIsComplete = True
            elif e.args[0] == 109:
                # If the server terminates the named pipe while the client is still connected
                print("Broken pipe, exiting")
                bIsComplete = True
            elif e.args[0] == 231:
                # Pipe busy, timeout before trying again
                time.sleep(0.5)
            else:
                # Some other exception while ReadFile() or WriteFile()
                print("Exception: ", e)
                bIsComplete = True

if __name__ == '__main__':
    """ Unit testing """
    nColSize, nRowSize, lstData, lstGraphLabels = retrieveData()
    print(nColSize, nRowSize)
    print(lstData)
    print(lstGraphLabels)
    updateStatus()
