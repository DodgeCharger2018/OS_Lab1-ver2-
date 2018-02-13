#define _CRT_SECURE_NO_WARNINGS
#include <windows.h> 
#include <stdio.h> 
#include <tchar.h>
#include <strsafe.h>
#include <string> 
#include <time.h>
#include <vector>
#include <iostream>
#include <cstring>
#pragma comment(lib, "User32.lib")

#define BUFSIZE 512

using namespace std;

DWORD WINAPI InstanceThread(LPVOID);
LPSTR GetAnswerToRequest(LPTSTR, LPTSTR, LPDWORD);
LPTSTR search(TCHAR* catalogName, LPTSTR pchReply, LPDWORD);

int _tmain(VOID)
{
	BOOL   fConnected = FALSE;
	DWORD  dwThreadId = 0;
	HANDLE hPipe = INVALID_HANDLE_VALUE, hThread = NULL;
	LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\mynamedpipe");

	// The main loop creates an instance of the named pipe and 
	// then waits for a client to connect to it. When the client 
	// connects, a thread is created to handle communications 
	// with that client, and this loop is free to wait for the
	// next client connect request. It is an infinite loop.

	for (;;)
	{
		_tprintf(TEXT("\nPipe Server: Main thread awaiting client connection on %s\n"), lpszPipename);
		hPipe = CreateNamedPipe(
			lpszPipename,             // pipe name 
			PIPE_ACCESS_DUPLEX,       // read/write access 
			PIPE_TYPE_MESSAGE |       // message type pipe 
			PIPE_READMODE_MESSAGE |   // message-read mode 
			PIPE_WAIT,                // blocking mode 
			PIPE_UNLIMITED_INSTANCES, // max. instances  
			BUFSIZE,                  // output buffer size 
			BUFSIZE,                  // input buffer size 
			0,                        // client time-out 
			NULL);                    // default security attribute 

		if (hPipe == INVALID_HANDLE_VALUE)
		{
			//_tprintf(TEXT("CreateNamedPipe failed, GLE=%d.\n"), GetLastError());
			cerr << (TEXT("CreateNamedPipe failed, GLE=%d.\n"), GetLastError());
			return -1;
		}

		// Wait for the client to connect; if it succeeds, 
		// the function returns a nonzero value. If the function
		// returns zero, GetLastError returns ERROR_PIPE_CONNECTED. 

		fConnected = ConnectNamedPipe(hPipe, NULL) ?
			TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

		if (fConnected)
		{
			printf("Client connected, creating a processing thread.\n");

			// Create a thread for this client. 
			hThread = CreateThread(
				NULL,              // no security attribute 
				0,                 // default stack size 
				InstanceThread,    // thread proc
				(LPVOID)hPipe,    // thread parameter 
				0,                 // not suspended 
				&dwThreadId);      // returns thread ID 

			if (hThread == NULL)
			{
				//_tprintf(TEXT("CreateThread failed, GLE=%d.\n"), GetLastError());
				cerr << (TEXT("CreateThread failed, GLE=%d.\n"), GetLastError());
				return -1;
			}
			else CloseHandle(hThread);
		}
		else
			// The client could not connect, so close the pipe. 
			CloseHandle(hPipe);
	}

	return 0;
}

DWORD WINAPI InstanceThread(LPVOID lpvParam)
// This routine is a thread processing function to read from and reply to a client
// via the open pipe connection passed from the main loop. Note this allows
// the main loop to continue executing, potentially creating more threads of
// of this procedure to run concurrently, depending on the number of incoming
// client connections.
{
	HANDLE hHeap = GetProcessHeap();
	TCHAR* pchRequest = (TCHAR*)HeapAlloc(hHeap, 0, BUFSIZE * sizeof(TCHAR));
	TCHAR* pchReply = (TCHAR*)HeapAlloc(hHeap, 0, BUFSIZE * sizeof(TCHAR));

	DWORD cbBytesRead = 0, cbReplyBytes = 0, cbWritten = 0;
	BOOL fSuccess = FALSE;
	HANDLE hPipe = NULL;

	// Do some extra error checking since the app will keep running even if this
	// thread fails.

	if (lpvParam == NULL)
	{
		//Провекра на ошибки
		cerr << "\nERROR - Pipe Server Failure:\n";
		cerr << "   InstanceThread got an unexpected NULL value in lpvParam.\n";
		cerr << "   InstanceThread exitting.\n";
		if (pchReply != NULL) HeapFree(hHeap, 0, pchReply);
		if (pchRequest != NULL) HeapFree(hHeap, 0, pchRequest);
		return (DWORD)-1;
	}

	if (pchRequest == NULL)
	{
		//Проверка на ошибки
		cerr << "\nERROR - Pipe Server Failure:\n";
		cerr << "   InstanceThread got an unexpected NULL heap allocation.\n";
		cerr << "   InstanceThread exitting.\n";
		if (pchReply != NULL) HeapFree(hHeap, 0, pchReply);
		return (DWORD)-1;
	}

	if (pchReply == NULL)
	{
		//Проверка на ошибки
		cerr << "\nERROR - Pipe Server Failure:\n";
		cerr << "   InstanceThread got an unexpected NULL heap allocation.\n";
		cerr << "   InstanceThread exitting.\n";
		if (pchRequest != NULL) HeapFree(hHeap, 0, pchRequest);
		return (DWORD)-1;
	}


	printf("InstanceThread created, receiving and processing messages.\n");

	// Экземляр объекта 

	hPipe = (HANDLE)lpvParam;

	//Пока считываем данные с клиента...
	while (1)
	{
		// Read client requests from the pipe. This simplistic code only allows messages
		// up to BUFSIZE characters in length.
		fSuccess = ReadFile(
			hPipe,        // handle to pipe 
			pchRequest,    // buffer to receive data 
			BUFSIZE * sizeof(TCHAR), // size of buffer 
			&cbBytesRead, // number of bytes read 
			NULL);        // not overlapped I/O 

		if (!fSuccess || cbBytesRead == 0)
		{
			if (GetLastError() == ERROR_BROKEN_PIPE)
			{
				//Проверка на ошибки
				cerr << (TEXT("InstanceThread: client disconnected.\n"), GetLastError());
			}
			else
			{
				//Проверка на ошибки
				cerr << (TEXT("InstanceThread ReadFile failed, GLE=%d.\n"), GetLastError());
			}
			break;
		}

		/////////////////////////////////
		// Обрабатываем основную операцию
		/////////////////////////////////
			_tprintf(TEXT("Client Request String:\"%s\"\n"), pchRequest);

			WIN32_FIND_DATA ffd;
			LARGE_INTEGER filesize;
			TCHAR szDir[MAX_PATH];
			size_t length_of_arg;
			HANDLE hFind = INVALID_HANDLE_VALUE;
			DWORD dwError = 0;
			char buff[100];

			StringCchLength(pchRequest, MAX_PATH, &length_of_arg);

			if (length_of_arg > (MAX_PATH - 3))
			{
				//_tprintf(TEXT("\nDirectory path is too long.\n"));
				cerr << (TEXT("\nDirectory path is too long.\n"));
				return 0;
			}
			_tprintf(TEXT("\nTarget directory is %s\n\n"), pchRequest);
			StringCchCopy(szDir, MAX_PATH, pchRequest);
			StringCchCat(szDir, MAX_PATH, TEXT("\\*"));

			hFind = FindFirstFile(szDir, &ffd);

			if (INVALID_HANDLE_VALUE == hFind)
			{
				cerr << (TEXT("No Find First File"));
				return 0;
			}

			do
			{
				if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					sprintf(buff, "%s <DIR>", ffd.cFileName);
					cbReplyBytes = (lstrlen(buff) + 1) * sizeof(TCHAR);
					cout << buff << " IT`s OK" << endl;
					fSuccess = WriteFile(
						hPipe,        // handle to pipe 
						buff,     // buffer to write from 
						cbReplyBytes, // number of bytes to write 
						&cbWritten,   // number of bytes written 
						NULL);        // not overlapped I/O 
					if (!fSuccess || cbReplyBytes != cbWritten)
					{
						//Проверка на ошибку записи
						cerr << (TEXT("InstanceThread WriteFile failed, GLE=%d.\n"), GetLastError());
						break;
					}

				}
				else
				{
					filesize.LowPart = ffd.nFileSizeLow;
					filesize.HighPart = ffd.nFileSizeHigh;

					TCHAR tchDate[80];
					SYSTEMTIME st;
					FileTimeToSystemTime(&ffd.ftLastWriteTime, &st);

					GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE,
						&st, nullptr, tchDate, _countof(tchDate));

					sprintf(buff, "%s	%d bayte %d  %dday %dmonth %dyear", ffd.cFileName, filesize.QuadPart, st.wDay,st.wMonth,st.wYear);
					cbReplyBytes = (lstrlen(buff) + 1) * sizeof(TCHAR);
					cout << buff << " IT`s OK" << endl;
					fSuccess = WriteFile(
						hPipe,        // handle to pipe 
						buff,     // buffer to write from 
						cbReplyBytes, // number of bytes to write 
						&cbWritten,   // number of bytes written 
						NULL);        // not overlapped I/O 
					if (!fSuccess || cbReplyBytes != cbWritten)
					{
						//Проверка на ошибку записи
						cerr << (TEXT("InstanceThread WriteFile failed, GLE=%d.\n"), GetLastError());
						break;
					}
				}
			} while (FindNextFile(hFind, &ffd) != 0);


			dwError = GetLastError();
			if (dwError != ERROR_NO_MORE_FILES)
			{
				cerr << (TEXT("No Find First File"));
			}
			//system("pause");

			//Закрываем
			FindClose(hFind);
			break;
	}

	// Flush the pipe to allow the client to read the pipe's contents 
	// before disconnecting. Then disconnect the pipe, and close the 
	// handle to this pipe instance. 

	FlushFileBuffers(hPipe);
	DisconnectNamedPipe(hPipe);
	CloseHandle(hPipe);

	HeapFree(hHeap, 0, pchRequest);
	HeapFree(hHeap, 0, pchReply);

	printf("InstanceThread exitting.\n");
	return 1;
}
