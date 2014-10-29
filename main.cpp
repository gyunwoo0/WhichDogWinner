

// 1. ���� ������ ������ �ϱ� �׷���
// 2. Ʈ���� ������ �˾� �޴� ���ؼ� �Ϻ��� ���� �ְ� �ϱ�
//


#include <Windows.h>
#include <stdio.h>
#include <TlHelp32.h>
#include "resource.h"


typedef struct TRACELOGDATA{
	SYSTEMTIME time;
	DWORD inc;
	char act[128];
};

typedef struct TRACELOGHEAD{
	DWORD dwNumberofentity;
	DWORD reserv[3];
};

void Error(char *msg);
void Clear(void);
void InitHandle(void);
void ReportResult(void);

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
VOID CALLBACK TimerFunc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

HINSTANCE g_hInst;
HWND hWndmain;
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
	HDC hdc;
	NOTIFYICONDATA nid;
	HMENU hMenu, hPopupMenu;
	POINT pt;
	int wmId, wmEvent;

	switch(iMessage)
	{
	case WM_CREATE:
		hWndmain = hWnd;
		OutputDebugString("[Trace] Start Program.\n");
		
		nid.cbSize = sizeof(NOTIFYICONDATA);
		nid.hWnd = hWnd;
		nid.uID = 0;
		nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
		// Ʈ���̾������� �̺�Ʈ�� ���۹��� �޽��� Ÿ������
		nid.uCallbackMessage = TRAY_NOTIFY;
		nid.hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_ICON1));
		strcpy(nid.szTip,"���̸� ���� �ذ��� ���� ũ��.");
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
			TrackPopupMenu(hPopupMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
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
				SendMessage(hWndmain, WM_CLOSE, 0, 0);
				break;
			case ID_REPORT:
				ReportResult();
				break;
			default:
				return DefWindowProc(hWnd, iMessage, wParam, lParam);
		}
		break;
	case WM_CLOSE:
		KillTimer(hWnd, IDT_TRACE_SEC);
		ReportResult();
		Clear();
		OutputDebugString("[Trace] End Program.\n");
		DestroyWindow(hWndmain);
		break;
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
	char lp_act[NAME_SIZE] = "?";
	char lp_caption[NAME_SIZE] = "?";

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
	GetWindowText(hwndTop, lp_caption, NAME_SIZE);

	DWORD pid;
	GetWindowThreadProcessId(hwndTop, &pid);
	
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	PROCESSENTRY32 entry32;
	entry32.dwSize = sizeof( PROCESSENTRY32 );

	Process32First(hSnap, &entry32);
	do{
		if(entry32.th32ProcessID == pid)	// ��Ʈ������ ���α׷� ã������ ���μ����̸��� �����;���
		{
			_snprintf(lp_act, NAME_SIZE, "[Trace] %s > %s", entry32.szExeFile, lp_caption);

			if( !strcmp(lp_act, pDat[pLoghead->dwNumberofentity].act) ){
				pDat[pLoghead->dwNumberofentity].inc+=1;

				strncat(lp_act, " [INC+1]\n", 1024);
				OutputDebugString(lp_act);
			}
			else{
				pLoghead->dwNumberofentity+=1;

				GetLocalTime(&pDat[pLoghead->dwNumberofentity].time);			
				_snprintf(pDat[pLoghead->dwNumberofentity].act, 128, "%s", lp_act);
				pDat[pLoghead->dwNumberofentity].inc = 1;

				strncat(lp_act, " [New 1]\n", 1024);
				OutputDebugString(lp_act);
			}
			
			//OutputDebugString(lp_act);		
		}
	}while(Process32Next(hSnap, &entry32));

}



void Error(char *msg)
{
	char outstr[128];
	_snprintf(outstr,128, "%s, ErrorCode: 0x%08x\n", msg, GetLastError());
	OutputDebugString( outstr );
	SendMessage(hWndmain, WM_CLOSE, 0, 0);
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
	_snprintf(logfilename,32, "%04d%02d%02d.dat", localtime.wYear, localtime.wMonth, localtime.wDay);
	
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

void ReportResult(void)
{
	char report[4096]={0,};

	SYSTEMTIME loctime;
	GetLocalTime(&loctime);

	_snprintf(report,4096, "## REPORT %04d-%02d-%02d ##\n", loctime.wYear, loctime.wMonth, loctime.wDay);

	_snprintf(report,4096, "%s Total action : %d\n", report, pLoghead->dwNumberofentity-1);

	TRACELOGDATA trRank[1024]={0,};
	DWORD k=0, totalUse = 0;
	for(int i=1; i < pLoghead->dwNumberofentity; i++) // ���ߴ��� �����ϴ� ��
	{	// Dat�� ���� �ִ��� ó������ �����ϸ鼭
		BOOL bExist = FALSE;
		for(int j=0; j < k; j++){ // rank�� 0~k ���� �����ؼ�
			if( !strcmp( trRank[j].act, pDat[i].act) ) 
			{	// ���� rank �� ���� dat�� ���� act�� ������ 
				// rank�� �߰����� �ʰ� sec�� ��ģ��.
				bExist = TRUE;
				trRank[j].inc += pDat[i].inc;
			}
		}
		if(bExist == FALSE){
			memcpy(&trRank[k], &pDat[i], sizeof(TRACELOGDATA));
			k++;
		}
		totalUse += pDat[i].inc;
		//_snprintf(report,2048, "%s %s (%d sec)\n", report, pDat[i].act, pDat[i].time.wSecond);	// Debugging ��
	}
	//OutputDebugString(report); // Debugging ��

	memset(report, 0, sizeof(report));
	_snprintf(report,4096, "================ Rank ================\n");
	_snprintf(report,4096, "�� %d�� ���\n", totalUse/60);

	DWORD rankIndex[1000];	// 1���� 1000������� �ε����� �迭�� ����
	DWORD temp=0;
	for(int i=0; i<1000; i++){ rankIndex[i] = i; } // �ʱ�ȭ

	for(int i=0; i < k-1 ; i++) 
	{	//  �ð��� ū ������ ���� �Ѵ�.
		for(int j=i+1; j<k; j++)
		{
			if( trRank[rankIndex[i]].inc < trRank[rankIndex[j]].inc )
			{
				temp		 = rankIndex[i];
				rankIndex[i] = rankIndex[j];
				rankIndex[j] = temp;
			}
		}
	}

	for(int i=0; i < 15 ; i++)	// 15������� �����Ѵ�.
	{
		_snprintf(report, 4096, "%s Rank %d --> %s (%d sec)\n", 
			report, i+1,
			trRank[rankIndex[i]].act, 
			trRank[rankIndex[i]].inc);
	}

	MessageBox(NULL, report, "���", MB_OK);
	
}