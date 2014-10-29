

// 1. 보고서 멋지게 나오게 하기 그래프
// 2. 트레이 아이콘 팝업 메뉴 통해서 일별로 볼수 있게 하기
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

	// 타이머를 설치 1초에 1회 동작
	// 타이머 프로시져에서는 활성화된 윈도우를 기록한다.
	// 초단위로 기록.
	// 같은동작 연속에 대한 처리는 나중에 생각하기로

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
		// 트레이아이콘의 이벤트를 전송받을 메시지 타입정의
		nid.uCallbackMessage = TRAY_NOTIFY;
		nid.hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_ICON1));
		strcpy(nid.szTip,"먹이를 많이 준개가 많이 크죠.");
		// 윈도우 쉘 관리자에게 nid의 정보를 가지는 트레이 아이콘을 등록함을 알린다.
		Shell_NotifyIcon(NIM_ADD, &nid);

		SetTimer(hWnd, IDT_TRACE_SEC, 1000, TimerFunc);
		InitHandle();
		return 0;
	case TRAY_NOTIFY:
		switch(lParam){
		case WM_RBUTTONDOWN:
			// 트레이 아이콘을 우클릭시 나타나는 메뉴를 생성하고 보여준다.   
			// 메뉴 로드하기
			hMenu = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_MENU1));

			// hMenu의 서브 메뉴들 중에서 첫번째 서브메뉴를 hPopupMenu로 등록
			hPopupMenu = GetSubMenu(hMenu, 0);

			// 마우스 위치 받아오기
			GetCursorPos(&pt);

			SetForegroundWindow(hWnd);
			TrackPopupMenu(hPopupMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
			SetForegroundWindow(hWnd);
                
			// 메뉴 삭제
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

	SYSTEMTIME loctime;		// 밤 열두시가 지나면 데이터 파일을 새로운 날짜로 바꿔야한다.
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
		if(entry32.th32ProcessID == pid)	// 엔트리에서 프로그램 찾았으면 프로세스이름을 가져와야지
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
	
	// 로그 파일 생성
	hLogfile = CreateFile(logfilename, GENERIC_READ | GENERIC_WRITE, 0, NULL,
			OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hLogfile == INVALID_HANDLE_VALUE){
		Error("Create File Error");
	}

	// 파일 맵핑 생성
	hLogmap = CreateFileMapping(hLogfile, NULL, PAGE_READWRITE, 0, MAPSIZE, NULL);
	if(hLogmap == INVALID_HANDLE_VALUE){
		Error("Create FileMapping Error");
	}

	// 맵 뷰 생성
	pView = (VOID*)MapViewOfFile(hLogmap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if( pView == NULL){
		Error("Mapping View Error");
	}
	//endoffile = GetFileSize(hLogfile, NULL);
	pLoghead =(TRACELOGHEAD*) pView;						// 헤드정보 가리킴
	pDat =(TRACELOGDATA*)((DWORD)pView+sizeof(TRACELOGHEAD));		// 데이터 가리킴

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
	for(int i=1; i < pLoghead->dwNumberofentity; i++) // 뭐했는지 취합하는 곳
	{	// Dat에 뭐가 있는지 처음부터 조사하면서
		BOOL bExist = FALSE;
		for(int j=0; j < k; j++){ // rank의 0~k 까지 조사해서
			if( !strcmp( trRank[j].act, pDat[i].act) ) 
			{	// 만약 rank 에 현재 dat와 같은 act가 있으면 
				// rank에 추가하지 않고 sec를 합친다.
				bExist = TRUE;
				trRank[j].inc += pDat[i].inc;
			}
		}
		if(bExist == FALSE){
			memcpy(&trRank[k], &pDat[i], sizeof(TRACELOGDATA));
			k++;
		}
		totalUse += pDat[i].inc;
		//_snprintf(report,2048, "%s %s (%d sec)\n", report, pDat[i].act, pDat[i].time.wSecond);	// Debugging 용
	}
	//OutputDebugString(report); // Debugging 용

	memset(report, 0, sizeof(report));
	_snprintf(report,4096, "================ Rank ================\n");
	_snprintf(report,4096, "총 %d분 사용\n", totalUse/60);

	DWORD rankIndex[1000];	// 1에서 1000등까지의 인덱스를 배열로 저장
	DWORD temp=0;
	for(int i=0; i<1000; i++){ rankIndex[i] = i; } // 초기화

	for(int i=0; i < k-1 ; i++) 
	{	//  시간이 큰 순서로 정렬 한다.
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

	for(int i=0; i < 15 ; i++)	// 15등까지만 보고한다.
	{
		_snprintf(report, 4096, "%s Rank %d --> %s (%d sec)\n", 
			report, i+1,
			trRank[rankIndex[i]].act, 
			trRank[rankIndex[i]].inc);
	}

	MessageBox(NULL, report, "결과", MB_OK);
	
}