

// 1. ���� ������ ������ �ϱ� �׷���
// 2. Ʈ���� ������ �˾� �޴� ���ؼ� �Ϻ��� ���� �ְ� �ϱ�
// 3. ���ø����̼� ���� �׷�ȭ�ѵ� �Ǻ��� ������ �����ֱ�


#include <Windows.h>
#include <stdio.h>
#include <TlHelp32.h>
#include "resource.h"

/* ����Ǵ� ���α׷��� ���� �����͸� �����ϱ� ���� ����ü */
typedef struct _TRACELOGDATA{
	SYSTEMTIME time;
	DWORD inc;
	BOOL bIncludedInSum;
	char szExe[256];
	char szTitle[256];
}TRACELOGDATA;

/*	���Ͽ� ����Ǵ� �������� ��ü���� ���뿡 ���� �����ϱ� ���� ����ü */
typedef struct _TRACELOGHEAD{
	DWORD dwNumberofentity;
	DWORD reserv[3];
}TRACELOGHEAD;


/* ������踦 �ۼ��Ҷ� ���Ǵ� ����ü */
typedef struct _USAGE_DETAIL_SUMMARY{
	char szTitle[256];
	DWORD inc;
}USAGE_DETAIL_SUMMARY;


/* ��� ���� ���� ���� ���α׷��� �ϳ��� �����ؼ� ǥ���ϱ� ���� 
    (���� ��踦 ���� ĸ���� �ٸ��� �ٸ� �����ͷ� �����ϱ� ������ ������ ������) */
typedef struct _USED_APP_SUMMARY{
	char szExe[256];
	DWORD inc;
	USAGE_DETAIL_SUMMARY * arDetail;
	DWORD dwDetailCnt;
}USED_APP_SUMMARY;




void Error(char *msg);
void Clear(void);
void InitHandle(void);
void ReportResult(void);

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
VOID CALLBACK TimerFunc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

HINSTANCE g_hInst;
HWND g_hWndmain;
LPCTSTR lpszClass = TEXT("Trace");
HANDLE hLogfile = INVALID_HANDLE_VALUE;
HANDLE hLogmap  = INVALID_HANDLE_VALUE;

VOID * pView = NULL;
TRACELOGHEAD * pLoghead = NULL;
TRACELOGDATA * pDat = NULL;


#define IDT_TRACE_SEC	1
#define NAME_SIZE		1024
#define MAPSIZE			sizeof(TRACELOGDATA)*1024
#define TRAY_NOTIFY (WM_APP + 100)


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{

	// Ÿ�̸Ӹ� ��ġ 1�ʿ� 1ȸ ����
	// Ÿ�̸� ���ν��������� Ȱ��ȭ�� �����츦 ����Ѵ�.
	// �ʴ����� ���.
	// �������� ���ӿ� ���� ó���� ���߿� �����ϱ��

	HWND hWnd;
	MSG Message;
	WNDCLASS WndClass;
	g_hInst=hInstance;

	WndClass.cbClsExtra = 0;
	WndClass.cbWndExtra = 0;
	WndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WndClass.hIcon= LoadIcon(NULL,IDI_APPLICATION);
	WndClass.hInstance = hInstance;
	WndClass.lpfnWndProc = WndProc;
	WndClass.lpszClassName = lpszClass;
	WndClass.lpszMenuName = NULL;
	WndClass.style = CS_HREDRAW | CS_VREDRAW;
	RegisterClass(&WndClass);

	hWnd = CreateWindow(lpszClass, lpszClass, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,
		NULL,(HMENU)NULL, hInstance, NULL);
	ShowWindow(hWnd, SW_HIDE);

	while(GetMessage(&Message, NULL, 0, 0)){
		TranslateMessage(&Message);
		DispatchMessage(&Message);
	}

	return (int)Message.wParam;

}

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	NOTIFYICONDATA nid;
	HMENU hMenu, hPopupMenu;
	POINT pt;
	int wmId, wmEvent, iMbRet = 0;

	switch(iMessage)
	{
	case WM_CREATE:
		g_hWndmain = hWnd;
		OutputDebugString("[Trace] Start Program.\n");
		
		nid.cbSize = sizeof(NOTIFYICONDATA);
		nid.hWnd = hWnd;
		nid.uID = 0;
		nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
		// Ʈ���̾������� �̺�Ʈ�� ���۹��� �޽��� Ÿ������
		nid.uCallbackMessage = TRAY_NOTIFY;
		nid.hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_ICON1));
		strncpy_s(nid.szTip,"���̸� ���� �� ���� ũ��.", 256);
		// ������ �� �����ڿ��� nid�� ������ ������ Ʈ���� �������� ������� �˸���.
		Shell_NotifyIcon(NIM_ADD, &nid);

		SetTimer(hWnd, IDT_TRACE_SEC, 1000, TimerFunc);
		InitHandle();
		return 0;
	case TRAY_NOTIFY:
		switch(lParam){
		case WM_RBUTTONDOWN:
			// Ʈ���� �������� ��Ŭ���� ��Ÿ���� �޴��� �����ϰ� �����ش�.   
			// �޴� �ε��ϱ�
			hMenu = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_MENU1));

			// hMenu�� ���� �޴��� �߿��� ù��° ����޴��� hPopupMenu�� ���
			hPopupMenu = GetSubMenu(hMenu, 0);

			// ���콺 ��ġ �޾ƿ���
			GetCursorPos(&pt);

			SetForegroundWindow(hWnd);
			TrackPopupMenu(hPopupMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, 
									pt.x, pt.y, 0, hWnd, NULL);
			SetForegroundWindow(hWnd);
                
			// �޴� ����
			DestroyMenu(hPopupMenu);
			DestroyMenu(hMenu);
			break;
		case WM_LBUTTONDOWN:
			break;
		}
	case WM_COMMAND:
			wmId    = LOWORD(wParam); 
			wmEvent = HIWORD(wParam); 
			// Parse the menu selections:
			switch (wmId)
			{
			case ID_CLOSE:
				ReportResult();
				SendMessage(g_hWndmain, WM_CLOSE, 0, 0);
				break;
			case ID_REPORT:
				ReportResult();
				break;
			default:
				return DefWindowProc(hWnd, iMessage, wParam, lParam);
		}
		break;
	case WM_CLOSE:
		iMbRet = MessageBox(g_hWndmain, "�����ұ��?", "����", MB_YESNO);
		if(iMbRet == IDYES)
		{
			KillTimer(hWnd, IDT_TRACE_SEC);
			Clear();
			OutputDebugString("[Trace] End Program.\n");
			DestroyWindow(g_hWndmain);
			break;
		}
		else
		{
			ShowWindow(g_hWndmain, SW_HIDE);
			return 0;
		}
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return(DefWindowProc(hWnd, iMessage, wParam, lParam));
}


VOID CALLBACK TimerFunc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	// dwTime : The number of milliseconds that have elapsed since the system was started. 
	//   This is the value returned by the GetTickCount function.


	HWND hwndTop;
	char lp_msg[NAME_SIZE] = "?";
	char lp_title[NAME_SIZE] = "?";

	SYSTEMTIME loctime;		// �� ���νð� ������ ������ ������ ���ο� ��¥�� �ٲ���Ѵ�.
	GetLocalTime(&loctime);
	if( loctime.wHour == 0 &&
		loctime.wMinute == 0 &&
		loctime.wSecond == 0 &&
		loctime.wMilliseconds >= 1 )
	{
		Clear();
		InitHandle();
	}


	hwndTop = GetForegroundWindow();
	GetWindowText(hwndTop, lp_title, NAME_SIZE);	// Ÿ��Ʋ�� ����

	DWORD pid;
	GetWindowThreadProcessId(hwndTop, &pid);
	
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	PROCESSENTRY32 entry32;
	entry32.dwSize = sizeof( PROCESSENTRY32 );

	DWORD dwCurEndp = pLoghead->dwNumberofentity;

	Process32First(hSnap, &entry32);
	do{
		if(entry32.th32ProcessID == pid)	// ��Ʈ������ ���α׷� ã������ ���μ����̸��� �����;���
		{
			BOOL bFoundEqual = FALSE;
			for(DWORD dwIndexSearchEqual=0; 
					dwIndexSearchEqual < dwCurEndp;
					dwIndexSearchEqual++)
			{
				if(    !strcmp(entry32.szExeFile,   pDat[dwIndexSearchEqual].szExe  )     // �������� �̸��� ����
				    && !strcmp(			lp_title,	pDat[dwIndexSearchEqual].szTitle)  )  // Ÿ��Ʋ �ٰ� ������
				{
					pDat[dwIndexSearchEqual].inc+=1;
					bFoundEqual = TRUE;
					break;
				}
			}
			if(bFoundEqual == FALSE)
			{
				dwCurEndp = pLoghead->dwNumberofentity;

				GetLocalTime( &pDat[dwCurEndp].time);									// ���� �ð�	
				_snprintf_s( pDat[dwCurEndp].szExe,   256, "%s", entry32.szExeFile);	// exe
				_snprintf_s( pDat[dwCurEndp].szTitle, 256, "%s", lp_title);			// title
				  		     pDat[dwCurEndp].inc = 1;	  								// inc �ʱⰪ

				pLoghead->dwNumberofentity+=1;
				
			}

		}
	}while(Process32Next(hSnap, &entry32));

}



void Error(char *arg_msg)
{
	char outstr[256];
	_snprintf_s(outstr,256, "%s, ErrorCode: 0x%08x\n", arg_msg, GetLastError());
	OutputDebugString( outstr );
	SendMessage(g_hWndmain, WM_CLOSE, 0, 0);
}


void Clear(void){
	if( pView != NULL )
		UnmapViewOfFile(pView);

	if( hLogmap != INVALID_HANDLE_VALUE )
		CloseHandle(hLogmap);

	if( hLogfile != INVALID_HANDLE_VALUE )
		CloseHandle(hLogfile);

	OutputDebugString("[Trace] Clear all handle value\n");
}

void InitHandle(void){
	SYSTEMTIME localtime;
	char logfilename[32];

	GetLocalTime(&localtime);
	_snprintf_s(logfilename,32, "%04d%02d%02d.dat", 
							localtime.wYear, localtime.wMonth, localtime.wDay);
	
	// �α� ���� ����
	hLogfile = CreateFile(logfilename, GENERIC_READ | GENERIC_WRITE, 0, NULL,
			OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hLogfile == INVALID_HANDLE_VALUE){
		Error("Create File Error");
	}

	// ���� ���� ����
	hLogmap = CreateFileMapping(hLogfile, NULL, PAGE_READWRITE, 0, MAPSIZE, NULL);
	if(hLogmap == INVALID_HANDLE_VALUE){
		Error("Create FileMapping Error");
	}

	// �� �� ����
	pView = (VOID*)MapViewOfFile(hLogmap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if( pView == NULL){
		Error("Mapping View Error");
	}
	//endoffile = GetFileSize(hLogfile, NULL);
	pLoghead =(TRACELOGHEAD*) pView;						// ������� ����Ŵ
	pDat =(TRACELOGDATA*)((DWORD)pView+sizeof(TRACELOGHEAD));		// ������ ����Ŵ

}

#define SEC_OF_MIN 60
void ReportResult(void)
{
	char report_msg[4096]={0,};

	SYSTEMTIME loctime;

	GetLocalTime(&loctime);
	_snprintf_s(report_msg,4096, "## REPORT %04d-%02d-%02d ##\n", 
								loctime.wYear, loctime.wMonth, loctime.wDay);
	_snprintf_s(report_msg,4096, "%s Total action : %d\n", 
								report_msg, pLoghead->dwNumberofentity-1);

	DWORD usedAppEndp=0, dwTotalUsageSec = 0;
	USED_APP_SUMMARY *arUsedAppSum = (USED_APP_SUMMARY*)malloc(
								pLoghead->dwNumberofentity * sizeof(USED_APP_SUMMARY) );
	
	
	/* ��� ���α׷��� ����Ǿ����� ���� */
	for(DWORD curDataIndex = 0; 
			curDataIndex < pLoghead->dwNumberofentity; 
			curDataIndex++)															// ���ߴ��� �����ϴ� ��
	{																				// Dat�� ���� �ִ��� ó������ �����ϸ鼭
		BOOL bIsNew = FALSE;
		for(DWORD cmpDataIndex = 0; 
				cmpDataIndex < usedAppEndp; 
				cmpDataIndex++)														// rank�� 0~k ���� �����ؼ�
		{
			if( !strcmp( arUsedAppSum[cmpDataIndex].szExe, pDat[curDataIndex].szExe) ) 
			{																		// ���� rank �� ���� dat�� ���� exe�� ������ 
				bIsNew = TRUE;														// rank�� �߰����� �ʰ� sec�� ��ģ��.
				arUsedAppSum[cmpDataIndex].inc += pDat[curDataIndex].inc;
			}
		}
		if(bIsNew == FALSE)
		{
			strncpy_s(arUsedAppSum[usedAppEndp].szExe, pDat[curDataIndex].szExe, 256);
			arUsedAppSum[usedAppEndp].inc = pDat[curDataIndex].inc;					// time �� ���� �����ϱ� ����..
			usedAppEndp++;
		}
		dwTotalUsageSec += pDat[curDataIndex].inc;
	}


	/* ���� ��� ������ */
	for(DWORD app_iter = 0 ;
			app_iter < usedAppEndp ;
			app_iter++
			)
	{
		DWORD dwDiffTitleCnt = 0;	
		for(DWORD dwCmpIndex = 0 ;
				dwCmpIndex < pLoghead->dwNumberofentity;
				dwCmpIndex ++)
		{
			if( !strcmp(arUsedAppSum[app_iter].szExe, pDat[dwCmpIndex].szExe) )
			{
				dwDiffTitleCnt++;
			}
		}

		// Ÿ��Ʋ���� �������ü�� �����ϱ� ���� �����Ҵ�
		arUsedAppSum[app_iter].arDetail = 
				(USAGE_DETAIL_SUMMARY*)malloc(sizeof(USAGE_DETAIL_SUMMARY) * dwDiffTitleCnt);
		memset(arUsedAppSum[app_iter].arDetail, 0, sizeof(USAGE_DETAIL_SUMMARY) * dwDiffTitleCnt );

		arUsedAppSum[app_iter].dwDetailCnt = dwDiffTitleCnt;


		// Ÿ��Ʋ���� �����ϱ� ���� ����.
		DWORD dwCopyIndex = 0;
		for(DWORD title_iter = 0 ;
				  title_iter < pLoghead->dwNumberofentity;
				  title_iter ++)
		{
			if( !strcmp(arUsedAppSum[app_iter].szExe, pDat[title_iter].szExe) )
			{
				// �� Ÿ��Ʋ�� ����
				if( !strncmp(pDat[title_iter].szTitle, "", 256) ) 
				{
					strncpy_s(
							arUsedAppSum[app_iter].arDetail[dwCopyIndex].szTitle,
							"--none title", 
							256);
				}				
				else// ���� ����
				{
					strncpy_s(
							arUsedAppSum[app_iter].arDetail[dwCopyIndex].szTitle,
							pDat[title_iter].szTitle, 
							256);
				}
				arUsedAppSum[app_iter].arDetail[dwCopyIndex].inc = pDat[title_iter].inc;
				dwCopyIndex++;
			}
		}
			

		/* ��뷮 ���� ������ ���� */

	}






	memset(report_msg, 0, sizeof(report_msg));
	_snprintf_s(report_msg,4096, "================ Rank ================\n");

	if( dwTotalUsageSec >= SEC_OF_MIN)
	{
		_snprintf_s(report_msg,4096, "%s�� %d�� %d�� ���\n",
					report_msg, dwTotalUsageSec/SEC_OF_MIN, dwTotalUsageSec%SEC_OF_MIN);
	}
	else
	{
		_snprintf_s(report_msg,4096, "%s�� %d�� ���\n",report_msg, dwTotalUsageSec);
	}

	for(DWORD app_iter=0; app_iter < usedAppEndp; app_iter++)
	{
		if( arUsedAppSum[app_iter].inc >= SEC_OF_MIN )
		{
			_snprintf_s(report_msg, 4096, "%s   %s (%d�� %d��)\n", 
					report_msg,  
					arUsedAppSum[app_iter].szExe,
					arUsedAppSum[app_iter].inc/SEC_OF_MIN, 
					arUsedAppSum[app_iter].inc%SEC_OF_MIN);
		}
		else
		{
			_snprintf_s(report_msg, 4096, "%s   %s (%d��)\n", 
					report_msg,  
					arUsedAppSum[app_iter].szExe, 
					arUsedAppSum[app_iter].inc);
		}

		for(DWORD title_iter=0; 
				title_iter < arUsedAppSum[app_iter].dwDetailCnt; 
				title_iter++)
		{
			if( arUsedAppSum[app_iter].arDetail[title_iter].inc >= SEC_OF_MIN )
			{
				_snprintf_s(report_msg, 4096, "%s        %s (%d�� %d��)\n", 
										report_msg, 
										arUsedAppSum[app_iter].arDetail[title_iter].szTitle,
										arUsedAppSum[app_iter].arDetail[title_iter].inc/SEC_OF_MIN,
										arUsedAppSum[app_iter].arDetail[title_iter].inc%SEC_OF_MIN);
			}
			else
			{
				_snprintf_s(report_msg, 4096, "%s        %s (%d��)\n", 
										report_msg, 
										arUsedAppSum[app_iter].arDetail[title_iter].szTitle,
										arUsedAppSum[app_iter].arDetail[title_iter].inc);
			}
		}
	}

	MessageBox(NULL, report_msg, "���", MB_OK);
	
	//ShowWindow(g_hWndmain, SW_SHOW);
	RECT rt;
	HDC hdc = GetDC(g_hWndmain);
	
	GetClientRect(g_hWndmain, &rt);
	rt.left = 40;
	rt.top = 10;

	SetTextColor(hdc, 0x00000000);
	SetBkMode(hdc,TRANSPARENT);

	//DrawText(hdc, report_msg, -1, &rt, DT_WORDBREAK | DT_NOCLIP | DT_LEFT);

	DeleteDC(hdc);
}



