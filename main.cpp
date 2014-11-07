

// 1. 보고서 멋지게 나오게 하기 그래프
// 2. 트레이 아이콘 팝업 메뉴 통해서 일별로 볼수 있게 하기
// 3. 어플리케이션 별로 그룹화한뒤 탭별로 순위로 보여주기


#include <Windows.h>
#include <stdio.h>
#include <TlHelp32.h>
#include "resource.h"

/* 실행되는 프로그램에 대한 데이터를 저장하기 위한 구조체 */
typedef struct _TRACELOGDATA{
	SYSTEMTIME time;
	DWORD inc;
	BOOL bIncludedInSum;
	char szExe[256];
	char szTitle[256];
}TRACELOGDATA;

/*	파일에 저장되는 데이터의 전체적인 내용에 대해 관리하기 위한 구조체 */
typedef struct _TRACELOGHEAD{
	DWORD dwNumberofentity;
	DWORD reserv[3];
}TRACELOGHEAD;


/* 세부통계를 작성할때 사용되는 구조체 */
typedef struct _USAGE_DETAIL_SUMMARY{
	char szTitle[256];
	DWORD inc;
}USAGE_DETAIL_SUMMARY;


/* 통계 보고를 위해 사용된 프로그램을 하나로 통합해서 표현하기 위함 
    (세부 통계를 위해 캡션이 다르면 다른 데이터로 저장하기 때문에 통합을 따로함) */
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
		// 트레이아이콘의 이벤트를 전송받을 메시지 타입정의
		nid.uCallbackMessage = TRAY_NOTIFY;
		nid.hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_ICON1));
		strncpy_s(nid.szTip,"먹이를 많이 준 개가 크죠.", 256);
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
			TrackPopupMenu(hPopupMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, 
									pt.x, pt.y, 0, hWnd, NULL);
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
		iMbRet = MessageBox(g_hWndmain, "종료할까요?", "묻기", MB_YESNO);
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
	GetWindowText(hwndTop, lp_title, NAME_SIZE);	// 타이틀바 읽음

	DWORD pid;
	GetWindowThreadProcessId(hwndTop, &pid);
	
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	PROCESSENTRY32 entry32;
	entry32.dwSize = sizeof( PROCESSENTRY32 );

	DWORD dwCurEndp = pLoghead->dwNumberofentity;

	Process32First(hSnap, &entry32);
	do{
		if(entry32.th32ProcessID == pid)	// 엔트리에서 프로그램 찾았으면 프로세스이름을 가져와야지
		{
			BOOL bFoundEqual = FALSE;
			for(DWORD dwIndexSearchEqual=0; 
					dwIndexSearchEqual < dwCurEndp;
					dwIndexSearchEqual++)
			{
				if(    !strcmp(entry32.szExeFile,   pDat[dwIndexSearchEqual].szExe  )     // 실행파일 이름이 같고
				    && !strcmp(			lp_title,	pDat[dwIndexSearchEqual].szTitle)  )  // 타이틀 바가 같으면
				{
					pDat[dwIndexSearchEqual].inc+=1;
					bFoundEqual = TRUE;
					break;
				}
			}
			if(bFoundEqual == FALSE)
			{
				dwCurEndp = pLoghead->dwNumberofentity;

				GetLocalTime( &pDat[dwCurEndp].time);									// 현재 시간	
				_snprintf_s( pDat[dwCurEndp].szExe,   256, "%s", entry32.szExeFile);	// exe
				_snprintf_s( pDat[dwCurEndp].szTitle, 256, "%s", lp_title);			// title
				  		     pDat[dwCurEndp].inc = 1;	  								// inc 초기값

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
	
	
	/* 몇개의 프로그램이 실행되었는지 취합 */
	for(DWORD curDataIndex = 0; 
			curDataIndex < pLoghead->dwNumberofentity; 
			curDataIndex++)															// 뭐했는지 취합하는 곳
	{																				// Dat에 뭐가 있는지 처음부터 조사하면서
		BOOL bIsNew = FALSE;
		for(DWORD cmpDataIndex = 0; 
				cmpDataIndex < usedAppEndp; 
				cmpDataIndex++)														// rank의 0~k 까지 조사해서
		{
			if( !strcmp( arUsedAppSum[cmpDataIndex].szExe, pDat[curDataIndex].szExe) ) 
			{																		// 만약 rank 에 현재 dat와 같은 exe가 있으면 
				bIsNew = TRUE;														// rank에 추가하지 않고 sec를 합친다.
				arUsedAppSum[cmpDataIndex].inc += pDat[curDataIndex].inc;
			}
		}
		if(bIsNew == FALSE)
		{
			strncpy_s(arUsedAppSum[usedAppEndp].szExe, pDat[curDataIndex].szExe, 256);
			arUsedAppSum[usedAppEndp].inc = pDat[curDataIndex].inc;					// time 은 따로 집계하기 힘듬..
			usedAppEndp++;
		}
		dwTotalUsageSec += pDat[curDataIndex].inc;
	}


	/* 세부 통계 조사함 */
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

		// 타이틀들을 멤버구조체에 저장하기 위해 동적할당
		arUsedAppSum[app_iter].arDetail = 
				(USAGE_DETAIL_SUMMARY*)malloc(sizeof(USAGE_DETAIL_SUMMARY) * dwDiffTitleCnt);
		memset(arUsedAppSum[app_iter].arDetail, 0, sizeof(USAGE_DETAIL_SUMMARY) * dwDiffTitleCnt );

		arUsedAppSum[app_iter].dwDetailCnt = dwDiffTitleCnt;


		// 타이틀들을 복사하기 위한 루프.
		DWORD dwCopyIndex = 0;
		for(DWORD title_iter = 0 ;
				  title_iter < pLoghead->dwNumberofentity;
				  title_iter ++)
		{
			if( !strcmp(arUsedAppSum[app_iter].szExe, pDat[title_iter].szExe) )
			{
				// 빈 타이틀은 따로
				if( !strncmp(pDat[title_iter].szTitle, "", 256) ) 
				{
					strncpy_s(
							arUsedAppSum[app_iter].arDetail[dwCopyIndex].szTitle,
							"--none title", 
							256);
				}				
				else// 내용 복사
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
			

		/* 사용량 많은 순으로 정렬 */

	}






	memset(report_msg, 0, sizeof(report_msg));
	_snprintf_s(report_msg,4096, "================ Rank ================\n");

	if( dwTotalUsageSec >= SEC_OF_MIN)
	{
		_snprintf_s(report_msg,4096, "%s총 %d분 %d초 사용\n",
					report_msg, dwTotalUsageSec/SEC_OF_MIN, dwTotalUsageSec%SEC_OF_MIN);
	}
	else
	{
		_snprintf_s(report_msg,4096, "%s총 %d초 사용\n",report_msg, dwTotalUsageSec);
	}

	for(DWORD app_iter=0; app_iter < usedAppEndp; app_iter++)
	{
		if( arUsedAppSum[app_iter].inc >= SEC_OF_MIN )
		{
			_snprintf_s(report_msg, 4096, "%s   %s (%d분 %d초)\n", 
					report_msg,  
					arUsedAppSum[app_iter].szExe,
					arUsedAppSum[app_iter].inc/SEC_OF_MIN, 
					arUsedAppSum[app_iter].inc%SEC_OF_MIN);
		}
		else
		{
			_snprintf_s(report_msg, 4096, "%s   %s (%d초)\n", 
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
				_snprintf_s(report_msg, 4096, "%s        %s (%d분 %d초)\n", 
										report_msg, 
										arUsedAppSum[app_iter].arDetail[title_iter].szTitle,
										arUsedAppSum[app_iter].arDetail[title_iter].inc/SEC_OF_MIN,
										arUsedAppSum[app_iter].arDetail[title_iter].inc%SEC_OF_MIN);
			}
			else
			{
				_snprintf_s(report_msg, 4096, "%s        %s (%d초)\n", 
										report_msg, 
										arUsedAppSum[app_iter].arDetail[title_iter].szTitle,
										arUsedAppSum[app_iter].arDetail[title_iter].inc);
			}
		}
	}

	MessageBox(NULL, report_msg, "결과", MB_OK);
	
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



