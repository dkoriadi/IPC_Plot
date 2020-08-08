#pragma once

#include "IPC_Plot.h"

typedef struct LIB_BUFFER_ST
{
	LIB_DOUBLE dData;
} LIB_BUFFER_ST;

typedef struct LIB_LABEL_ST
{
	LIB_CHAR szLabel[LIB_MAX_LABEL_SIZE];
} LIB_LABEL_ST;

typedef struct LIB_PIPE_INFO
{
	HANDLE hNamedPipe;
	HANDLE hChildProcess;
	LIB_BUFFER_ST* pstDataArray;
	LIB_LABEL_ST* pstLabelArray;
} LIB_PIPE_INFO;