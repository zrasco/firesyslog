// firesyslog.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "firesyslog.h"

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;								// current instance
HWND hMain;										// Main window
HWND hTabRef[5][20];							// Tab references
HWND hCopyList;									// List with data to be copied
HMENU hMenuSystray;
HMENU hMenuList;
HMENU hMenuCustom;
DLGPROC OldDlgProc;
int	nCopyColumn = 0;							// Column to be copied
char g_szGoogleString[1024];					// Google search method

CUSTOMMENUITEMLIST cmil[MAX_CUSTOMCOMMANDS];	// Custom command table
CUSTOMMENUITEMLIST cmilcopy[MAX_CUSTOMCOMMANDS];// Local copy stored for dialog list
CMD32PROCESS cmd32p;							// CMD32.EXE process info
PROGRAM_OPTIONS POpt;							// Program options
SYSTRAYICONDATA sid;							// Systray icon data 
ITCSTRUCT itc;									// Interthread communication
LOGFILESTRUCT LFS_Syslog;						// Syslog information
LOGFILESTRUCT LFS_Firewall;						// Firewall information
LOGFILETIMER LFT;								// Logfile timer(changes at midnight)

#ifdef _DEBUG
char dbgtext[65535 * 8];
#endif /* _DEBUG */

char *szSeverity[] =
{
	"Emergency",
	"Alert",
	"Critical",
	"Error",
	"Warning",
	"Notice",
	"Informational",
	"Debug",
	NULL
};

char *szFacility[] =
{
	"Kernel",
	"User",
	"Mail",
	"System",
	"Auth",
	"Syslogd",
	"Line",
	"News",
	"UUCP",
	"Clock",
	"Security",
	"FTP",
	"NTP",
	"LogAudit",
	"LogAlert",
	"Clock",
	"Local0",
	"Local1",
	"Local2",
	"Local3",
	"Local4",
	"Local5",
	"Local6",
	"Local7",
	NULL
};

// Forward declarations of functions included in this code module:
BOOL				InitInstance(HINSTANCE, int);
bool				GetLastErrorReadable(char*, int);

// Source: https://www.codeproject.com/Tips/479880/GetLastError-as-std-string
// Credit: Orjan Westin
bool GetLastErrorReadable(char *buffer, int inputLength)
{
	DWORD error = GetLastError();
	if (error)
	{
		LPVOID lpMsgBuf;
		DWORD bufLen = FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			error,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&lpMsgBuf,
			0, NULL);

		if (bufLen && bufLen <= inputLength)
		{
			LPCSTR lpMsgStr = (LPCSTR)lpMsgBuf;

			sprintf(buffer, "%s", lpMsgStr);

			LocalFree(lpMsgBuf);

			return true;
		}
	}

	return false;
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
 	// TODO: Place code here.
	DWORD dwStyle;
	MSG msg;
	INITCOMMONCONTROLSEX icc;
	char szFullIniFile[1024], szCurrentDirectory[1024], 
		szDefaultLogDir[1024], buf[1024], inifile[256];
	FILE *fpIni;

	// Perform application initialization:
	icc.dwICC = ICC_TREEVIEW_CLASSES | ICC_TAB_CLASSES;
	icc.dwSize = sizeof(icc);
	InitCommonControlsEx(&icc);
	WSADATA wsaData;

	/* Start application */
	if (!InitInstance (hInstance, nCmdShow)) 
	{
		return FALSE;
	}


	/* Load program options from .ini file */
	memset(&POpt,0,sizeof(POpt));
	memset(&cmil,0,sizeof(cmil));
	memset(&cmilcopy,0,sizeof(cmilcopy));
	GetModuleFileName(NULL,buf,sizeof(buf));
	strcpy(inifile,strrchr(buf,92));

	/* Strip preceding \ and extension */
	strcpy(inifile,&inifile[1]);
	(strrchr(inifile,'.'))[0] = 0;
	strcat(inifile,".ini");

	/* Load options */
	GetCurrentDirectory(sizeof(szCurrentDirectory),szCurrentDirectory);
	strcpy(szDefaultLogDir,szCurrentDirectory);
	strcat(szDefaultLogDir,"\\logs");
	sprintf(szFullIniFile,"%s\\%s",szCurrentDirectory,inifile);

	fpIni = fopen(szFullIniFile,"r");

	if (!fpIni)
	/* Ini file doesn't exist */
	{
		WritePrivateProfileString("options","logdirectory",szDefaultLogDir,szFullIniFile);
		strcpy(POpt.logdir,szDefaultLogDir);
		WritePrivateProfileString("options","systray","0",szFullIniFile);
		WritePrivateProfileString("options","systemstartup","0",szFullIniFile);
	}
	else
	/* Read options from file */
	{
		/* TODO: Insert error checking for missing keys */
		int len, count;

		GetPrivateProfileString("options","logdirectory",szDefaultLogDir,POpt.logdir,sizeof(POpt.logdir),szFullIniFile);
		len = (int)strlen(POpt.logdir);

		/* Cut off trailing '\' */
		if (POpt.logdir[len - 1] == '\\')
			POpt.logdir[len - 1] = 0;

		GetPrivateProfileString("options","systray","0",buf,sizeof(buf),szFullIniFile);

		if (buf[0] != '0')
			POpt.bSystrayStart = TRUE;

		GetPrivateProfileString("options","systemstartup","0",buf,sizeof(buf),szFullIniFile);

		if (buf[0] != '0')
			POpt.bWindowsStart = TRUE;

		/* Load the custom menu item list */
		for (count = 0; count < MAX_CUSTOMCOMMANDS; count++)
		{
			if (GetCMILSettings(szFullIniFile,(unsigned int)count,&cmil[count]) == -1)
				break;
		}
		fclose(fpIni);

	}

	/* Create menus & clipboard data */
	CreateMenus();

	/* Initialize logfiles */
	memset(&LFS_Syslog,0,sizeof(LFS_Syslog));
	memset(&LFS_Firewall,0,sizeof(LFS_Firewall));

	strcpy(LFS_Syslog.szDirectory,POpt.logdir);
	strcpy(LFS_Firewall.szDirectory,POpt.logdir);

	strcpy(LFS_Syslog.szSuffix,"Syslog");
	strcpy(LFS_Firewall.szSuffix,"Firewall");

	/* Set midnight catch timer */
	memset(&LFT,0,sizeof(LFT));
	GetLocalTime(&LFT.stLastSet);
	SetTimer(hMain,WM_LOGCHANGE,60000,NULL);

	InitLogFile(&LFS_Syslog);
	InitLogFile(&LFS_Firewall);

	/* Set list views to "Full row select", and fix Z-Order */
	dwStyle = (DWORD)SendMessage(GetDlgItem(hMain,IDC_LVSYSLOG),LVM_GETEXTENDEDLISTVIEWSTYLE,0,0);
	dwStyle |= LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES;
	PostMessage(GetDlgItem(hMain,IDC_LVSYSLOG),LVM_SETEXTENDEDLISTVIEWSTYLE,0,dwStyle);
	PostMessage(GetDlgItem(hMain,IDC_LVFIREWALL),LVM_SETEXTENDEDLISTVIEWSTYLE,0,dwStyle);
	SetWindowPos(GetDlgItem(hMain,IDC_TABMAIN),HWND_BOTTOM,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE);
	//SetWindowPos(GetDlgItem(hMain,IDC_LVFIREWALL),GetDlgItem(hMain,IDC_TABMAIN),0,0,0,0,SWP_NOMOVE | SWP_NOSIZE);
	//SetWindowPos(GetDlgItem(hMain,IDC_LVSYSLOG),GetDlgItem(hMain,IDC_TABMAIN),0,0,0,0,SWP_NOMOVE | SWP_NOSIZE);
	//SetWindowPos(hTabRef[2][0],HWND_TOPMOST,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE);
	//SetWindowPos(hTabRef[2][1],HWND_TOPMOST,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE);
	//SetWindowPos(hTabRef[2][2],HWND_TOPMOST,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE);
	//SetWindowPos(hTabRef[2][3],HWND_TOPMOST,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE);
	//SetWindowPos(hTabRef[2][4],HWND_TOPMOST,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE);

	//SetWindowPos(GetDlgItem(hMain,IDC_LVFIREWALL),HWND_TOP,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE);
	//SetWindowPos(GetDlgItem(hMain,IDC_LVSYSLOG),HWND_TOP,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE);

	SetWindowPos(hTabRef[2][0],HWND_TOPMOST,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE);
	SetWindowPos(hTabRef[2][1],HWND_TOPMOST,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE);
	SetWindowPos(hTabRef[2][2],HWND_TOPMOST,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE);
	SetWindowPos(hTabRef[2][3],HWND_TOPMOST,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE);
	SetWindowPos(hTabRef[2][4],HWND_TOPMOST,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE);

	/* Set up system tray stuff */
	sid.hIconActivity = LoadIcon(hInst,MAKEINTRESOURCE(IDI_FIRESYSLOGR));
	sid.hIconIdle = LoadIcon(hInst,MAKEINTRESOURCE(IDI_FIRESYSLOG));

	sid.nid.cbSize = sizeof(sid.nid);
	sid.nid.hWnd = hMain;
	sid.nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
	sid.nid.uCallbackMessage = WM_TRAYICON;
	sid.nid.hIcon = sid.hIconIdle;
	sid.nid.uID = IDI_FIRESYSLOG;
	strcpy(sid.nid.szTip,"Firewall/Syslog parser");

	/* Start in systray */
	if (POpt.bSystrayStart)
		SendMessage(hMain,WM_CLOSE,0,0);
	else
	{
		ShowWindow(hMain, nCmdShow);
		UpdateWindow(hMain);
	}

	/* Start winsock */
	WSAStartup(MAKEWORD(1,1),&wsaData);

	itc.sSyslog = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);

	if (itc.sSyslog == INVALID_SOCKET)
	{
		MessageBox(hMain,"Failure at socket()","Fatal Error",MB_OK | MB_ICONERROR);
		WSACleanup();
		exit(0);
	}

	/* Set up server info */
	memset(&itc.sin,0,sizeof(itc.sin));

#ifdef _DEBUG
	memset(&dbgtext,0,sizeof(dbgtext));
#endif /* _DEBUG */

	itc.sin.sin_family = AF_INET;
	itc.sin.sin_port = htons(SYSLOG_PORT);
	itc.sin.sin_addr.s_addr = INADDR_ANY;

	/* Listen on the UDP port */
	if (bind(itc.sSyslog,(struct sockaddr*)&itc.sin,sizeof(struct sockaddr)) ==
		SOCKET_ERROR)
	{
		sprintf(buf,"Unable to bind to UDP port %d\nError: %d",SYSLOG_PORT,WSAGetLastError());

		MessageBox(hMain,buf,"Fatal Error",MB_OK | MB_ICONERROR);
		WSACleanup();
		exit(0);
	}

	/* Begin UDP listening thread */
	itc.hThread = (HANDLE)_beginthreadex(NULL,0,UDPRead,NULL,0,(unsigned int*)&itc.nThreadID);

	/* Spawn background cmd32.exe process */
	memset(&cmd32p,0,sizeof(CMD32PROCESS));
	CreateCMD32Process(&cmd32p);
	CreateCMD32Window(&cmd32p);

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		if (!IsDialogMessage(hMain,&msg))
		{
			if (!IsDialogMessage(cmd32p.hWndDialog,&msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}

	return (int) msg.wParam;
}

//
//   FUNCTION: InitInstance(HANDLE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	HMENU hMenu;
	HWND hTab, hCurr;
	TC_ITEM tcItem;

	hInst = hInstance; // Store instance handle in our global variable

	//hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
	//  CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

	hMain = CreateDialog(hInst,MAKEINTRESOURCE(IDD_MAIN),NULL,(DLGPROC)DlgProc);

	if (!hMain)
	{
		return FALSE;
	}

	/* Add Menu */
	hMenu = CreateMenu();

	/* Initialize dialog items */
	hTab = GetDlgItem(hMain,IDC_TABMAIN);

	tcItem.mask = TCIF_TEXT;

	tcItem.pszText = "Syslog";
	TabCtrl_InsertItem(hTab,0,&tcItem);

	tcItem.pszText = "Firewall";
	TabCtrl_InsertItem(hTab,1,&tcItem);

	tcItem.pszText = "Shell";
	TabCtrl_InsertItem(hTab,2,&tcItem);

#ifdef _DEBUG
	tcItem.pszText = "Debug";
	TabCtrl_InsertItem(hTab,3,&tcItem);
#endif /* _DEBUG */

	/* Set up list views */
	hCurr = GetDlgItem(hMain,IDC_LVSYSLOG);
	ListView_AddColumn(hCurr,0,"Date/time",120);
	ListView_AddColumn(hCurr,1,"Facility.Level",100);
	ListView_AddColumn(hCurr,2,"Hostname",100);
	ListView_AddColumn(hCurr,3,"Data",200);

	hCurr = GetDlgItem(hMain,IDC_LVFIREWALL);
	ListView_AddColumn(hCurr,0,"Date/time",120);
	ListView_AddColumn(hCurr,1,"Action",50);
	ListView_AddColumn(hCurr,2,"Source IP",70);
	ListView_AddColumn(hCurr,3,"Dest IP",70);
	ListView_AddColumn(hCurr,4,"Type",60);
	ListView_AddColumn(hCurr,5,"Source Port",70);
	ListView_AddColumn(hCurr,6,"Dest Port",60);
	ListView_AddColumn(hCurr,7,"Size",35);

	/* Set up tab references */
	memset(&hTabRef,0,sizeof(hTabRef));

	/* Syslog items */
	hTabRef[0][0] = GetDlgItem(hMain,IDC_LVSYSLOG);

	/* Firewall items */
	hTabRef[1][0] = GetDlgItem(hMain,IDC_LVFIREWALL);

	hTabRef[2][0] = GetDlgItem(hMain,IDC_BOXINPUT);
	hTabRef[2][1] = GetDlgItem(hMain,IDC_BOXOUTPUT);
	//hTabRef[2][2] = GetDlgItem(hMain,IDC_BTNCMDSEND);
	hTabRef[2][2] = GetDlgItem(hMain,IDC_EDITCMDOUTPUT);
	hTabRef[2][3] = GetDlgItem(hMain,IDC_EDITCMDINPUT);

#ifdef _DEBUG
	/* Debug box */
	hTabRef[3][0] = GetDlgItem(hMain,IDC_EDITDEBUG);
#endif /* _DEBUG */

	return TRUE;
}

//
//  FUNCTION: DlgProc(HWND, unsigned, WORD, LONG)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK DlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	SYSLOGPACKET syslogpkt;
	char buf[1024], szMsgSource[64], szAction[16], szSrcIP[32], szDestIP[32],
		szProtocol[32], szSrcPort[16], szDestPort[16],
		szICMPType[16], szLength[16];

	switch (message) 
	{
		case WM_INITDIALOG:
		{
			HWND hWndEdit = GetDlgItem(hWnd,IDC_EDITCMDINPUT);

			/* Set icons, default ID's */
			SendMessage(hWnd,DM_SETDEFID,IDC_BTNCMDSEND,0);
			SendMessage(hWnd,WM_SETICON,ICON_BIG,(LPARAM)LoadIcon(hInst, MAKEINTRESOURCE(IDI_FIRESYSLOG)));
			SendMessage(hWnd,WM_SETICON,ICON_SMALL,(LPARAM)LoadIcon(hInst, MAKEINTRESOURCE(IDI_FIRESYSLOG)));

			/* Subclass the dialog(to capture escape, ctrl+break, etc...) */
			OldDlgProc = (DLGPROC) SetWindowLong(hWndEdit,GWL_WNDPROC,(LONG)DlgSubclassProc);

			//LRESULT CALLBACK CMD32EditProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)

			return TRUE;
		}
		break;
		case WM_GETMINMAXINFO:
		/* Window resizing rules */
		{
			MINMAXINFO *pmmi = (MINMAXINFO*)(lParam);

			pmmi->ptMinTrackSize.x = 200;
			pmmi->ptMinTrackSize.y = 200;
		}
		break;
		case WM_RECVDATA:
		{
			BOOL bFirewallData = FALSE;
			LV_ITEM lvitem;
			HWND hLV;
			char *pszInfo = NULL;
			char szDateTime[32], szDate[32], szTime[32], szFacilityLevel[128],
				szHostname[64], szData[1024];
			int count, index = 0, baseindex = 0;

#ifdef _DEBUG
			strcat(dbgtext,itc.szBuffer);
			SetDlgItemText(hWnd,IDC_EDITDEBUG,dbgtext);
#endif /* _DEBUG */
	
			/* Flash our systray icon */
			sid.dwIconFlash = GetTickCount() + TRAYICON_FLASHTIME;
			sid.nid.hIcon = sid.hIconActivity;
			Shell_NotifyIcon(NIM_MODIFY,&sid.nid);
			PostMessage(hWnd,WM_TRAYFLASH,0,0);

			lvitem.mask = LVIF_TEXT;

			GetSyslogPacket(itc.szBuffer,&syslogpkt);

			if (syslogpkt.bFacility == FACILITY_USER &&
				syslogpkt.bLevel == LEVEL_WARNING)
			/* Possible firewall data */
			{
// TCP: <12>kernel: IN=vlan1 OUT= MAC=00:14:bf:f8:46:49:00:30:b8:c3:87:00:08:00:45:00:00:28 SRC=211.234.112.71 DST=68.109.167.45 LEN=40 TOS=0x00 PREC=0x00 TTL=104 ID=62031 PROTO=TCP SPT=6000 DPT=7212 WINDOW=16384 RES=0x00 SYN URGP=0 
// UDP: <12>kernel: IN=vlan1 OUT= MAC=00:14:bf:f8:46:49:00:30:b8:c3:87:00:08:00:45:00:01:41 SRC=68.2.16.25 DST=68.109.167.45 LEN=321 TOS=0x00 PREC=0x00 TTL=59 ID=33187 PROTO=UDP SPT=53 DPT=2049 LEN=301 
// ICMP: <12>kernel: IN=vlan1 OUT= MAC=00:14:bf:f8:46:49:00:30:b8:c3:87:00:08:00:45:00:00:3c SRC=24.36.49.89 DST=192.168.1.1 LEN=60 TOS=0x00 PREC=0x00 TTL=111 ID=24831 PROTO=ICMP TYPE=8 CODE=0 ID=512 SEQ=1024 

				/*
				char buf[256], szMsgSource[64], szAction[16], szSrcIP[32], szDestIP[32],
					szProtocol[32], szSrcPort[16], szDestPort[16],
					szICMPType[16], szLength[16];
					*/
				int nSrcLen;
				BOOL bAction = FALSE;
				char szMonth[128], szDate[128], szTime[128];

				sscanf(syslogpkt.szMessage,"%s %s %s %s %s %s\r\n",szMonth, szDate, szTime, szMsgSource,szAction,buf);

				nSrcLen = (int)strlen(szMsgSource);
				if (szMsgSource[nSrcLen - 1] == ':')
					szMsgSource[nSrcLen - 1] = 0;

				if (strcmp(szMsgSource,"kernel") == 0 &&
					(bAction = (szAction[0] == 'I' && szAction[1] == 'N' && szAction[2] == '=') 
					|| (buf[0] == 'I' && buf[1] == 'N' && buf[2] == '=')))
				/* Firewall packet */
				{
					bFirewallData = TRUE;

					ParseEquals(syslogpkt.szMessage," SRC=",szSrcIP);
					ParseEquals(syslogpkt.szMessage," PROTO=",szProtocol);
					ParseEquals(syslogpkt.szMessage," DST=",szDestIP);
					ParseEquals(syslogpkt.szMessage," LEN=",szLength);

					if (ParseEquals(syslogpkt.szMessage,"TYPE=",szICMPType) != -1)
					/* ICMP packet */
					{
						strcat(szProtocol,"-");
						strcat(szProtocol,szICMPType);
						strcpy(szSrcPort,"N/A");
						strcpy(szDestPort,"N/A");
					}
					else
					/* Copy port information */
					{
						ParseEquals(syslogpkt.szMessage,"SPT=",szSrcPort);
						ParseEquals(syslogpkt.szMessage,"DPT=",szDestPort);						
					}

					if (!bAction)
					/* Default action = DROP */
						strcpy(szAction,"DROP");

					/* Output the packet to the log */
					lvitem.iItem = LFS_Firewall.nCurrent;

					Log_AddSyslogEntry(&LFS_Firewall,&syslogpkt,buf);
					sscanf(buf,"%s %s",szDate,szTime);
					sprintf(szDateTime,"%s %s",szDate,szTime);

					/*
					** #1 Action
					** #2 Source IP
					** #3 Destination IP
					** #4 Protocol
					** #5 Source port
					** #6 Destination port
					** #7 Size
					*/

					hLV = GetDlgItem(hWnd,IDC_LVFIREWALL);

					lvitem.iSubItem = 0;
					lvitem.pszText = szDateTime;
					ListView_InsertItem(hLV,&lvitem);

					ListView_SetItemText(hLV,lvitem.iItem,1,szAction);
					ListView_SetItemText(hLV,lvitem.iItem,2,szSrcIP);
					ListView_SetItemText(hLV,lvitem.iItem,3,szDestIP);
					ListView_SetItemText(hLV,lvitem.iItem,4,szProtocol);
					ListView_SetItemText(hLV,lvitem.iItem,5,szSrcPort);
					ListView_SetItemText(hLV,lvitem.iItem,6,szDestPort);
					ListView_SetItemText(hLV,lvitem.iItem,7,szLength);
				}
			}
			if (!bFirewallData)
			/* Add to syslog log and list */
			{
				lvitem.iItem = LFS_Syslog.nCurrent;

				Log_AddSyslogEntry(&LFS_Syslog,&syslogpkt,buf);
				pszInfo = buf;

				for (count = 0; pszInfo[count] != '\r'; count++)
				{
					if (pszInfo[count] == 32 && index < 4)
					{
						index++;

						if (index == 1)
						{
							szDateTime[baseindex] = pszInfo[count];
							baseindex++;
						}
						else if (index == 2)
						/* End of Date/time */
						{
							szDateTime[baseindex] = 0;
							baseindex = 0;
						}
						else if (index == 3)
						/* End of Facility.Level */
						{
							szFacilityLevel[baseindex] = 0;
							baseindex = 0;
						}
						else if (index == 4)
						/* End of hostname */
						{
							szHostname[baseindex] = 0;
							baseindex = 0;
						}


						continue;
					}
					else
					{
						if (index < 2)
						/* Still on date */
							szDateTime[baseindex] = pszInfo[count];
						else if (index == 2)
						/* On Facily.Level */
							szFacilityLevel[baseindex] = pszInfo[count];
						else if (index == 3)
						/* On hostname */
							szHostname[baseindex] = pszInfo[count];
						else
						/* On data */
							szData[baseindex] = pszInfo[count];

						baseindex++;
					}
				}

				szData[baseindex] = 0;

				/* 
				** Column #1 = Date/time, Column #2 = Facility.Level
				** Coulmn #3 = Hostname, Column #4 = Data
				*/

				hLV = GetDlgItem(hWnd,IDC_LVSYSLOG);

                lvitem.iSubItem = 0;
				lvitem.pszText = szDateTime;
				ListView_InsertItem(hLV,&lvitem);

				ListView_SetItemText(hLV,lvitem.iItem,1,szFacilityLevel);
				ListView_SetItemText(hLV,lvitem.iItem,2,szHostname);
				ListView_SetItemText(hLV,lvitem.iItem,3,szData);
			}

			itc.bNewData = FALSE;
		}
		break;
		case WM_SIZE:
		{
			int nWidth = LOWORD(lParam), nHeight = HIWORD(lParam);
			HWND hTabMain, hLVSyslog, hLVFirewall;

#ifdef _DEBUG
			HWND hEditDbg;
#endif /* _DEBUG */

			hTabMain = GetDlgItem(hWnd,IDC_TABMAIN);
			hLVSyslog = GetDlgItem(hWnd,IDC_LVSYSLOG);
			hLVFirewall = GetDlgItem(hWnd,IDC_LVFIREWALL);

#ifdef _DEBUG
			hEditDbg = GetDlgItem(hWnd,IDC_EDITDEBUG);
#endif /* _DEBUG */

			MoveWindow(hTabMain,0,1,nWidth - 1,nHeight - 1,TRUE);
			MoveWindow(hLVSyslog,11,24,nWidth - 20 ,nHeight - 30,TRUE);
			MoveWindow(hLVFirewall,11,24,nWidth - 20 ,nHeight - 30,TRUE);

			/* Resize the shell controls */		
			MoveWindow(GetDlgItem(hMain,IDC_BOXINPUT),8,nHeight - 50,nWidth - 21, 46 ,TRUE);
			MoveWindow(GetDlgItem(hMain,IDC_BOXOUTPUT),8,25,nWidth - 21, nHeight - 75,TRUE);
			MoveWindow(GetDlgItem(hMain,IDC_EDITCMDOUTPUT),15,40,nWidth - 35,nHeight - 95,TRUE);
			MoveWindow(GetDlgItem(hMain,IDC_EDITCMDINPUT),15,nHeight - 34,nWidth - 35, 20,TRUE);

#ifdef _DEBUG
			MoveWindow(hEditDbg,11,24,nWidth - 20,nHeight - 30,TRUE);
#endif /* _DEBUG */
		}
		break;
		case WM_NOTIFY:
		{
			int idCtrl = (int) wParam;
			LPNMHDR pnmh = (LPNMHDR) lParam;

			switch (pnmh->idFrom)
			{
				case IDC_TABMAIN:
				{
					int tab = TabCtrl_GetCurSel(pnmh->hwndFrom);
					
					if (pnmh->code == TCN_SELCHANGE)
					/* Selection is changing */
					{
						int count, count2;

						//SetWindowPos(GetDlgItem(hMain,IDC_TABMAIN),HWND_BOTTOM,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE);

						/* Place Z-Order of tab in the middle(HWND_TOP) */
						SetWindowPos(GetDlgItem(hMain,IDC_TABMAIN),HWND_TOPMOST,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE);

						for (count = 0; count < 5; count++)
						{
							/* test */
							//if (count == 2)
								//SetWindowPos(GetDlgItem(hMain,IDC_TABMAIN),HWND_TOPMOST,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE);
							for (count2 = 0; hTabRef[count][count2] != NULL; count2++)
							/* Show the correct windows */
							{
								if (count == tab)
								{
									ShowWindow(hTabRef[count][count2],SW_SHOW);

									/* Z-Order of the new ones are on the top */
									SetWindowPos(hTabRef[count][count2],HWND_TOPMOST,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE);
								}
								else
								{
									ShowWindow(hTabRef[count][count2],SW_HIDE);

									/* Place Z-Order of these on the bottom */
									SetWindowPos(hTabRef[count][count2],HWND_BOTTOM,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE);
								}
							}
						}

						//SetWindowPos(GetDlgItem(hMain,IDC_TABMAIN),HWND_TOPMOST,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE);

						if (tab == 2)
						/* Command prompt tab */
						{
							SetWindowPos(GetDlgItem(hMain,IDC_TABMAIN),HWND_BOTTOM,0,0,0,0,SWP_NOREDRAW | SWP_NOMOVE | SWP_NOSIZE);
							SetFocus(cmd32p.hWndInput);
						}
						else
							SetWindowPos(GetDlgItem(hMain,IDC_TABMAIN),HWND_BOTTOM,0,0,0,0,SWP_NOREDRAW | SWP_NOMOVE | SWP_NOSIZE);
					}
				}
				break;
				case IDC_LVFIREWALL:
				case IDC_LVSYSLOG:
				{
					if (((LPNMHDR)lParam)->code == NM_RCLICK)
					/* Right-clicked the listview */
					{
						int state = -1, total = ListView_GetSelectedCount(pnmh->hwndFrom), items = 0;
						char buf[256], szMenuText[256];
						LVHITTESTINFO lvhittest;
						MENUITEMINFO mii;
						LV_COLUMN lvcolumn;
						RECT rect;
						POINT ptOriginal;

						/* Pop up the menu */
						GetCursorPos(&ptOriginal);
						lvhittest.pt = ptOriginal;
						GetWindowRect(pnmh->hwndFrom,&rect);
						lvhittest.pt.x -= rect.left; lvhittest.pt.y -= rect.top;
                        
						ListView_SubItemHitTest(pnmh->hwndFrom,&lvhittest);

						if (lvhittest.iSubItem != -1)
						{
							BOOL bDelMenu = FALSE;

							hCopyList = pnmh->hwndFrom;
							nCopyColumn = lvhittest.iSubItem;

							/* Get column name */
							lvcolumn.mask = LVCF_TEXT;
							lvcolumn.pszText = buf;
							lvcolumn.cchTextMax = sizeof(buf);

							ListView_GetColumn(pnmh->hwndFrom,lvhittest.iSubItem,&lvcolumn);

							/* Change the first menu item to "Copy <Column name>" */
							sprintf(szMenuText,"Copy %s",lvcolumn.pszText);

							if (ListView_GetSelectedCount(pnmh->hwndFrom) > 1)
								strcat(szMenuText,"(s)");

							mii.cbSize = sizeof(mii);
							mii.fMask = MIIM_TYPE;
							mii.fType = MFT_STRING;
							mii.dwTypeData = szMenuText;
							SetMenuItemInfo(hMenuList,MENUID_LISTVIEW_COPYITEM,FALSE,&mii);

							/* Delete the old menu items */
							RemoveMenu(hMenuList,5,MF_BYPOSITION);
							RemoveMenu(hMenuList,4,MF_BYPOSITION);

							if (pnmh->idFrom == IDC_LVFIREWALL &&
								(lvhittest.iSubItem == 2 ||				/* Source IP */
								lvhittest.iSubItem == 3))				/* Dest IP */
							{
								char text[256];
								int nTextSize = 256, sel = ListView_GetNextItem(pnmh->hwndFrom,-1,LVIS_SELECTED);

								/* Get the IP address for later */
								ListView_GetItemText(pnmh->hwndFrom,
									sel
									,lvhittest.iSubItem,
									text,
									nTextSize);

								if (sel != -1)
								/* Only show the menu if there are selections present */
								{
									strcpy(cmd32p.szData,text);

									bDelMenu = TRUE;
									AppendMenu(hMenuList,MF_SEPARATOR,NULL,NULL);
									AppendMenu(hMenuList,MF_POPUP,(UINT_PTR)hMenuCustom,"C&ustom");
								}
							}
							else if (pnmh->idFrom == IDC_LVFIREWALL &&
								(lvhittest.iSubItem == 5 ||
								lvhittest.iSubItem == 6))
							/* Google source/destination port */
							{
								int sel = ListView_GetNextItem(pnmh->hwndFrom,-1,LVIS_SELECTED);

								if (sel != -1)
								{
									char szPortBuf[256], szPort[32], szProto[32];

									bDelMenu = TRUE;									

									/* Get the port number */
									ListView_GetItemText(pnmh->hwndFrom,
										sel,
										lvhittest.iSubItem,
										szPort,
										sizeof(szPort));

									/* Get the protocol type */
									ListView_GetItemText(pnmh->hwndFrom,
										sel,
										4,
										szProto,
										sizeof(szProto));

									if (szPort[0] != 'N')
									/* Don't google for N/A protocols! */
									{
										sprintf(szPortBuf,"Google %s port %s...",szProto,szPort);
										AppendMenu(hMenuList,MF_SEPARATOR,NULL,NULL);
										AppendMenu(hMenuList,MF_STRING,MENUID_LISTVIEW_GOOGLEPORT,szPortBuf);

										sprintf(g_szGoogleString,"http://www.google.com/search?hl=en&q=%s+port+%s",szProto,szPort);
									}
								}
							}

							SetForegroundWindow(hWnd);
							TrackPopupMenu(hMenuList,TPM_LEFTALIGN | TPM_RIGHTBUTTON,ptOriginal.x,ptOriginal.y,0,hWnd,NULL);
							PostMessage(hWnd,WM_NULL,0,0);

						}
					}
				}
				break;
			}
		}
		break;
		case WM_TRAYICON:
		{
			switch (LOWORD(lParam))
			{
				case WM_LBUTTONDBLCLK:
				{
					ShowWindow(hWnd,SW_NORMAL);
					KillTimer(hWnd,WM_TRAYFLASH);
					Shell_NotifyIcon(NIM_DELETE,&sid.nid);
					SetForegroundWindow(hWnd);
					SetActiveWindow(hWnd);
				}
				break;
				case WM_RBUTTONUP:
				{
					POINT pt;

					GetCursorPos(&pt);
					
					SetForegroundWindow(hWnd);
					TrackPopupMenu(hMenuSystray,TPM_RIGHTALIGN | TPM_RIGHTBUTTON,pt.x,pt.y,0,hWnd,NULL);
					PostMessage(hWnd,WM_NULL,0,0);
				}
				break;
			}
		}
		break;
		case WM_MENUCOMMAND:
		case WM_COMMAND:
		{
			int id = LOWORD(wParam);

			switch (id)
			{
				case IDCANCEL:
					SendMessage(hWnd,WM_CLOSE,0,0);
				break;
				/* Main menu items */
				case MENUID_FILE_SETTINGS:
				{
					DialogBox(hInst,MAKEINTRESOURCE(IDD_OPTIONS),hWnd,(DLGPROC)SettingsProc);
				}
				break;
				case MENUID_FILE_EXIT:
					DestroyWindow(hWnd);
				break;
				case MENUID_VIEW_SYSLOG:
					ShellExecute(hWnd,"open",LFS_Syslog.szFullFile,NULL,POpt.logdir,SW_SHOW);
				break;
				case MENUID_VIEW_FIREWALL:
					ShellExecute(hWnd,"open",LFS_Firewall.szFullFile,NULL,POpt.logdir,SW_SHOW);
				break;
				case MENUID_VIEW_LOGDIR:
					ShellExecute(hWnd,"open",POpt.logdir,NULL,POpt.logdir,SW_SHOW);
				break;
				case MENUID_VIEW_CUSTOMCOMMANDS:
					DialogBox(hInst,MAKEINTRESOURCE(IDD_CUSTOMCMDOPTS),hWnd,(DLGPROC)CustomCMDProc);
				break;

				/* Systray menu items */
				case MENUID_SYSTRAY_RESTORE:
				{
					ShowWindow(hWnd,SW_NORMAL);
					KillTimer(hWnd,WM_TRAYFLASH);
					Shell_NotifyIcon(NIM_DELETE,&sid.nid);
					SetForegroundWindow(hWnd);
					SetActiveWindow(hWnd);
				}
				break;
				case MENUID_SYSTRAY_EXIT:
					DestroyWindow(hWnd);
				break;

				/* Listview menu items */
				case MENUID_LISTVIEW_COPYITEM:
				case MENUID_LISTVIEW_COPYALL:
				{
					int total = ListView_GetSelectedCount(hCopyList);
					int state = -1, count, count2, nTextSize, nTotalColumns = 0, nLength;
					char *pszBuf, text[256];
					HWND hLVHeader;
					HGLOBAL hMemClipboard = GlobalAlloc(GMEM_MOVEABLE,8912);

					if (!total)
					/* In case the doofus clicks it with nothing selected! */
						break;

					/* Get number of columns */
					hLVHeader = ListView_GetHeader(hCopyList);
					nTotalColumns = Header_GetItemCount(hLVHeader);

					OpenClipboard(hWnd);
					EmptyClipboard();

					nTextSize = sizeof(text);

					pszBuf = (char*)GlobalLock(hMemClipboard);
					pszBuf[0] = 0;

					for (count = 0; count < total; count++)
					{
						state = ListView_GetNextItem(hCopyList,state,LVIS_SELECTED);

						if (state != -1)
						/* Copy next item */
						{
							if (id == MENUID_LISTVIEW_COPYITEM)
							/* Copy only one column */
							{
								ListView_GetItemText(hCopyList,state,nCopyColumn,text,nTextSize);
								strcat(pszBuf,text);
								strcat(pszBuf,"\r\n");
							}
							else
							/* Copy all data */
							{
								for (count2 = 0; count2 <= nTotalColumns; count2++)
								/* Copy each element */
								{
									if (count2 < nTotalColumns)
									{
										ListView_GetItemText(hCopyList,state,count2,text,nTextSize);
										strcat(pszBuf,text);
										
										if (count2 < nTotalColumns - 1)
											strcat(pszBuf," ");
									}
									else
									{
										strcat(pszBuf,"\r\n");
									}
								}
							}
						}
						else
							break;
					}

					/* Take out last CRLF */
					nLength = (int)strlen(pszBuf);
					pszBuf[nLength - 1] = 0;
					pszBuf[nLength - 2] = 0;

					GlobalUnlock(hMemClipboard);

					SetClipboardData(CF_TEXT,hMemClipboard);
					CloseClipboard();

				}
				break;
				case MENUID_LISTVIEW_CLEAR:
				/* User hit "Clear" */
				{
					ListView_DeleteAllItems(hCopyList);

					if (GetDlgItem(hWnd,IDC_LVSYSLOG) == hCopyList)
						LFS_Syslog.nCurrent = 0;
					else
						LFS_Firewall.nCurrent = 0;
				}
				break;
				case MENUID_LISTVIEW_CUSTOMADD:
				/* Open the command dialog*/
					DialogBox(hInst,MAKEINTRESOURCE(IDD_CUSTOMCMDOPTS),hWnd,(DLGPROC)CustomCMDProc);
				break;
				case MENUID_LISTVIEW_GOOGLEPORT:
				{
					BOOL bQuoteYet = FALSE;
					int count;
					char ch;
					char szBrowserTotal[1024], szBrowserEXE[1024], szParameter[2048];

					GetCurrentBrowser(szBrowserTotal);

					/* Skip past first double-quote and search for second */
					for (count = 1; szBrowserTotal[count]; count++)
					{
						ch = szBrowserTotal[count];

						if (ch == '"')
						/* Seperate the browser image from the parameters(if any) */
						{
							szBrowserTotal[count] = 0;
							strcpy(szBrowserEXE,szBrowserTotal);
							szBrowserEXE[count] = szBrowserTotal[count] = ch;
							szBrowserEXE[count + 1] = 0;

							strcpy(szParameter,&szBrowserTotal[count + 2]);
						}
					}

					/* Append the google string to the browser launch parameters */
					strcat(szParameter," ");
					strcat(szParameter,g_szGoogleString);

					ShellExecute(NULL,
						"open",
						szBrowserEXE,
						szParameter,
						NULL,
						SW_SHOW);
				}
				break;
				case IDC_BTNCMDSEND:
				/* User hit enter to send command */
				{
					char chbuf[1024];
					DWORD dwWritten;
					int len, sz = sizeof(chbuf), tab = tab = TabCtrl_GetCurSel(GetDlgItem(hWnd,IDC_TABMAIN));

					if (tab == 2)
					{
						/* Get the command */
						GetDlgItemText(cmd32p.hWndDialog,IDC_EDITCMDINPUT,chbuf,sz);
						strcat(chbuf,"\r\n");

						//strcpy(chbuf,"test\r\n");
						len = (int)strlen(chbuf);

						/* Send the command thru the pipe and re-focus the input edit box */
						WriteFile(cmd32p.hPipeStdInputW,chbuf,len,&dwWritten,NULL);
						SetWindowText(cmd32p.hWndInput,"");
						SetFocus(cmd32p.hWndInput);
					}
				}
				default:
				/* Check for custom menu commands */
				{
					int count, nCMDID = (int)wParam;

					for (count = 0; cmil[count].dwMenuID ; count++)
					{
						if (cmil[count].dwMenuID == nCMDID)
						/* Got one of the items! */
						{
							char szCmdline[1024];
							int len;
							DWORD dwWritten;

							sprintf(szCmdline,cmil[count].szCmdline,cmd32p.szData);

							/* Execute the specified command in the shell tab */
							TabCtrl_SetCurFocus(GetDlgItem(hMain,IDC_TABMAIN),2);

							strcat(szCmdline,"\r\n");

							//strcpy(chbuf,"test\r\n");
							len = (int)strlen(szCmdline);

							/* Send the command thru the pipe and re-focus the input edit box */
							WriteFile(cmd32p.hPipeStdInputW,szCmdline,len,&dwWritten,NULL);
							SetWindowText(cmd32p.hWndInput,"");
							SetFocus(cmd32p.hWndInput);
						}
					}
				}
				break;
			}
		}
		break;
		case WM_TIMER:
		{
			int nTimerID = (int)wParam;

			if (nTimerID == WM_TRAYFLASH && 
				sid.dwIconFlash && 
				GetTickCount() > sid.dwIconFlash)
			/* Systray flash */
			{
				sid.nid.hIcon = sid.hIconIdle;
				sid.dwIconFlash = 0;
				Shell_NotifyIcon(NIM_MODIFY,&sid.nid);
			}
			else if (nTimerID == WM_LOGCHANGE)
			/* Midnight log change timer */
			{
				SYSTEMTIME stCheck;

				GetLocalTime(&stCheck);

				if (stCheck.wDay != LFT.stLastSet.wDay)
				/* New date, new logs */
				{
					GetLocalTime(&LFT.stLastSet);
					UpdateLogFiles();
				}
			}
		}
		break;
		case WM_CLOSE:
			{
				SetTimer(hWnd,WM_TRAYFLASH,100,NULL);
				Shell_NotifyIcon(NIM_ADD,&sid.nid);
				ShowWindow(hWnd,SW_HIDE);

				//DestroyWindow(hWnd);
			}
		break;
		case WM_DESTROY:
		{
			KillTimer(hWnd,WM_TRAYFLASH);
			KillTimer(hWnd,WM_LOGCHANGE);
			Shell_NotifyIcon(NIM_DELETE,&sid.nid);
			PostQuitMessage(0);
		}
		break;
		default:
			return FALSE;
	}

	return TRUE;
}

DLGPROC SettingsProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
/*
** SettingsProc()
** Callback for options/settings dialog
*/
{
	char buf[2048];

	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			SetDlgItemText(hDlg,IDC_EDITLOGDIR,POpt.logdir);
			
			if (POpt.bSystrayStart)
				SendDlgItemMessage(hDlg,IDC_CHKSYSTRAY,BM_SETCHECK,BST_CHECKED,0);
			if (POpt.bWindowsStart)
				SendDlgItemMessage(hDlg,IDC_CHKWINSTART,BM_SETCHECK,BST_CHECKED,0);
		}
		break;
		case WM_CLOSE:
			EndDialog(hDlg,NULL);
		break;
		case WM_COMMAND:
		{
			int wID = GET_WM_COMMAND_ID(wParam,lParam);

			switch (wID)
			{
				case ID_BTNOK:
				{
					GetDlgItemText(hDlg,IDC_EDITLOGDIR,buf,sizeof(buf));
					
					if (mkdir(buf) &&
						GetLastError() != ERROR_ALREADY_EXISTS)
					/* Unable to create directory */
					{
						char msgbuf[4096];
						char error[1028];

						FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
							NULL,
							GetLastError(),
							0,
							error,
							sizeof(error),
							NULL);

						sprintf(msgbuf,"Unable to create directory %s\r\n\r\nError: %s",buf,error);

						MessageBox(hDlg,msgbuf,"Fatal Error",MB_OK | MB_ICONERROR);
						break;
					}
					else
					{
						HKEY hKey;
						char szModule[1024];

						memset(&POpt,0,sizeof(POpt));

						/* Update our settings */
						if (SendDlgItemMessage(hDlg,IDC_CHKSYSTRAY,BM_GETCHECK,0,0) ==
							BST_CHECKED)
							POpt.bSystrayStart = TRUE;
						if (SendDlgItemMessage(hDlg,IDC_CHKWINSTART,BM_GETCHECK,0,0) ==
							BST_CHECKED)
							POpt.bWindowsStart = TRUE;

						GetDlgItemText(hDlg,IDC_EDITLOGDIR,POpt.logdir,sizeof(POpt.logdir));

						/* Update registry key */
						GetModuleFileName(NULL,szModule,sizeof(szModule));

						RegCreateKey(HKEY_CURRENT_USER,
							"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
							&hKey);

						if (POpt.bWindowsStart)
							RegSetValueEx(hKey,"firesyslog",NULL,REG_SZ,(const BYTE*)szModule,(DWORD)strlen(szModule) + 1 );
						else
							RegDeleteValue(hKey,"firesyslog");

						UpdateSettings();
						UpdateLogFiles();
					}
				}
				/* NO break statement */
				case ID_BTNCANCEL:
					EndDialog(hDlg,NULL);
			}
		}
		break;
	}

	return FALSE;
}

void ListView_AddColumn(HWND hwnd, int index, char* coltext, int width)
/*
** ListView_AddColumn()
** This function adds a column on the specified listview
*/
{
	int retval;
	LV_COLUMN lvcolumn;

	memset(&lvcolumn,0,sizeof(lvcolumn));

	lvcolumn.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
	lvcolumn.iSubItem = index;
	lvcolumn.cx = width;
	lvcolumn.pszText = coltext;

	retval = ListView_InsertColumn(hwnd,index,&lvcolumn);
	
}

unsigned __stdcall UDPRead(void *dummy)
/*
** UDPRead()
** Reads UDP packets from the syslog daemon
*/
{
	int retval = 0;
	int bufsize = sizeof(itc.szBuffer);
	char beforebuf[65535];

	while (TRUE)
	{
		strcpy(beforebuf,itc.szBuffer);
		/* Wait for data to come in */
		retval = recv(itc.sSyslog,itc.szBuffer,bufsize,0);

		itc.szBuffer[retval] = 0;
		strcat(itc.szBuffer,"\r\n");

		itc.bNewData = TRUE;

		PostMessage(hMain,WM_RECVDATA,0,0);

		while (itc.bNewData == TRUE)
			Sleep(10);
	}

	return 0;
}

void GetSyslogPacket(char *pszRawData, SYSLOGPACKET *syslgPkt)
/*
** GetSyslogPacket()
** Takes the input string(single syslog packet) and processes it
*/
{
	int nCode, count;
	char buf[4096];

	memset(syslgPkt,0,sizeof(SYSLOGPACKET));

	sscanf(pszRawData,"<%3d>",&nCode);

	for (count = 0; pszRawData[count] != '>'; count++); count++;

	strcpy(buf,&pszRawData[count]);
	
	/* We wouldnt want anyone causing a buffer overflow, now would we ;) */
	buf[1024] = 0;
	strcpy(syslgPkt->szMessage,buf);

	if (nCode > 255)
	/* Hmm, this is suspicious ;) */
		nCode = 123;

	/* Extract the facility codes */
	GetFacilityAndLevel((BYTE)nCode,&syslgPkt->bFacility,&syslgPkt->bLevel);

}

void GetFacilityAndLevel(BYTE bInput, BYTE *bFacility, BYTE *bLevel)
/*
** GetFacilityAndLevel()
** Extracts the facility and level codes from the specified 
*/
{
	BYTE bLevelMask = 7;	/* 0000 0111 */
	BYTE bFacilityMask = 31; /* 0001 1111 */


	/* Severity level is the first 3 LSB's */
	*bLevel = bInput & bLevelMask;
	*bFacility = (bInput >> 3) & bFacilityMask;
}

int InitLogFile(LOGFILESTRUCT *pLFS)
/*
** InitLogFile()
** Creates/opens the logfile specified and creates the file-mapping objects
*/
{
	SYSTEMTIME stTime;

	/* Get time */
	GetLocalTime(&stTime);

	/* Try to make the directory in case it doesnt exist */
	int mkdirResult = mkdir(pLFS->szDirectory);

	if (mkdirResult != 0)
	{
		const int BUFLEN = 256;
		int error = GetLastError();
		char errorMsg[BUFLEN];

		// Ignore error B7 - Directory already exists
		if (error && error != 0xB7 && GetLastErrorReadable(errorMsg, BUFLEN))
		{
			char msg[BUFLEN + 4096];
			sprintf(msg,"Unable to create logfile directory %s \n\nError: %s", pLFS->szDirectory, errorMsg);
			MessageBox(hMain, msg, "Fatal Error", MB_OK | MB_ICONSTOP);
			WSACleanup();
			exit(0);
		}
	}

	sprintf(pLFS->szFullFile,"%s\\%s[%d-%d-%d].log",pLFS->szDirectory,pLFS->szSuffix,
		stTime.wMonth,stTime.wDay,stTime.wYear);

	pLFS->fp = fopen(pLFS->szFullFile,"a+b");

	if (!pLFS->fp)
	{
		MessageBox(hMain,"Unable to open logfile!","Fatal Error",MB_OK | MB_ICONSTOP);
		WSACleanup();
		exit(0);
	}
	
	/* Open the logfile */
	//pLFS->hFile = CreateFile(szFullFile,GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
	//	NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	//if (pLFS->hFile == INVALID_HANDLE_VALUE)
	/* Unable to open logfile */
	//{
	//	MessageBox(hMain,"Unable to open logfile!","Fatal Error",MB_OK | MB_ICONSTOP);
	//	WSACleanup();
	//	exit(0);
	//}

    /* Open the file mapping object */
	/* Temp: Use a 1MB logfile */

	//pLFS->hFileMap = CreateFileMapping(pLFS->hFile,NULL,PAGE_READWRITE,0,LOGFILE_SIZE,NULL);

	//if (pLFS->hFileMap == NULL)
	/* Unable to create file mapping object */
	//{
	//	MessageBox(hMain,"Unable to create log file mapping object!","Fatal Error",MB_OK | MB_ICONSTOP);
	//	CloseHandle(pLFS->hFile);
	//	WSACleanup();
	//	exit(0);
	//}
    
	/* Map view of file to process memory */
	//pLFS->pvPosition = pLFS->pvFile = MapViewOfFile(pLFS->hFileMap,FILE_MAP_ALL_ACCESS,0,0,0);
	

	return 0;
}

int Log_AddSyslogEntry(LOGFILESTRUCT *pLFS, SYSLOGPACKET *pSyslgpkt, char *buf)
/*
** Log_AddSyslogEntry()
** This function adds an entry to the memory-mapped region of the syslog file
*/
{
	SYSTEMTIME stTime;
	char szFormattedDate[64], szFormattedTime[64];
	//PVOID pvCurr = pLFS->pvPosition;
	int buflen;

	/* Sample syslog entry: 8-4-06 16:15:03 User.Warning 192.168.1.1 :blahblahblahblah! */

	GetLocalTime(&stTime);
	GetDateFormat(LOCALE_USER_DEFAULT,NULL,&stTime,"M'-'d'-'yy",szFormattedDate,sizeof(szFormattedDate));
	GetTimeFormat(LOCALE_USER_DEFAULT,NULL,&stTime,"hh':'mm':'sstt",szFormattedTime,sizeof(szFormattedTime));
	strlwr(szFormattedTime);

	/* TEMP: Use 192.168.1.1 */

	/* Step #1, Format what need */
	buflen = sprintf(buf,"%s %s %s.%s 192.168.1.1 %s",szFormattedDate,szFormattedTime,
		szFacility[pSyslgpkt->bFacility],szSeverity[pSyslgpkt->bLevel],
		pSyslgpkt->szMessage
		);
	/*
	buflen = sprintf(buf,"%d-%d-%d %d:%02d:%02d %s.%s 192.168.1.1 %s",stTime.wMonth,stTime.wDay,stTime.wYear,
		stTime.wHour,stTime.wMinute,stTime.wSecond,
		szFacility[pSyslgpkt->bFacility],szSeverity[pSyslgpkt->bLevel],
		pSyslgpkt->szMessage
		);
	*/

	/* Step #2, Write to file */
	/* TODO: File write bug when dbgtext exceeds size */
	fprintf(pLFS->fp,buf);
	fflush(pLFS->fp);

	/* Step #2, Copy to memory-mapped region */
	//memmove(pvCurr,(const void*)buf,buflen);
	//pszPlace = (char*)pLFS->pvPosition;
    //pszPlace += buflen;
	//pLFS->pvPosition = (PVOID)pszPlace;

	/* Step #3, Update our internal counters & references */
	//pLFS->pvLLI[pLFS->nCurrent] = pvCurr;
	pLFS->nCurrent++;

	return 0;
}

int ParseEquals(const char *pszInputString, const char *pszParameter, char *pszValue)
/*
** ParseEquals()
** Extracts the value of a = string inside of a bigger string.
** Example: ParseEquals(pszInput,"SRC_IP=",pszIP);
** Returns 0 if sucessful, and the result in pszValue(must be an initialized string!)
*/
{
	int count;
	int len = (int)strlen(pszParameter);
	char *szPtr;

	szPtr = (char*)strstr(pszInputString,pszParameter);

	if (!szPtr)
		return -1;

	szPtr += len;

	for (count = 0; szPtr[count] && szPtr[count] != ' '; count++);

	strncpy(pszValue,szPtr,count);
	pszValue[count] = 0;

	return 0;
}

void UpdateSettings()
{
	char inifile[1024], buf[1024], szCurrentDirectory[1024], szFullIniFile[1024];
	char szSection[256];
	int count;

	GetModuleFileName(NULL,buf,sizeof(buf));
	strcpy(inifile,strrchr(buf,92));

	/* Strip preceding \ and extension */
	strcpy(inifile,&inifile[1]);
	(strrchr(inifile,'.'))[0] = 0;
	strcat(inifile,".ini");

	GetCurrentDirectory(sizeof(szCurrentDirectory),szCurrentDirectory);
	sprintf(szFullIniFile,"%s\\%s",szCurrentDirectory,inifile);

	/* Clear INI file before writing */
	DeleteFile(szFullIniFile);

	WritePrivateProfileString("options","logdirectory",POpt.logdir,szFullIniFile);

	sprintf(buf,"%d",POpt.bSystrayStart);
	WritePrivateProfileString("options","systray",buf,szFullIniFile);
	sprintf(buf,"%d",POpt.bWindowsStart);
	WritePrivateProfileString("options","systemstartup",buf,szFullIniFile);

	for (count = 0; cmil[count].szName[0] && count < MAX_CUSTOMCOMMANDS; count++)
	/* Write custom menu item list */
	{
		sprintf(szSection,"Program-%d",count);

		WritePrivateProfileString(szSection,"name",cmil[count].szName,szFullIniFile);
		WritePrivateProfileString(szSection,"cmdline",cmil[count].szCmdline,szFullIniFile);
	}
}

void UpdateLogFiles()
{
	SYSTEMTIME stTime;

	/* Modify log file information */
	strcpy(LFS_Syslog.szDirectory,POpt.logdir);
	strcpy(LFS_Firewall.szDirectory,POpt.logdir);

	/* Get time & current directory */
	GetLocalTime(&stTime);

	/* Try to make the directory in case it doesnt exist */
	mkdir(LFS_Syslog.szDirectory);

	/* Syslog file */
	sprintf(LFS_Syslog.szFullFile,"%s\\%s[%d-%d-%d].log",LFS_Syslog.szDirectory,LFS_Syslog.szSuffix,
		stTime.wMonth,stTime.wDay,stTime.wYear);

	fflush(LFS_Syslog.fp);
	fclose(LFS_Syslog.fp);
	LFS_Syslog.fp = fopen(LFS_Syslog.szFullFile,"a+b");

	if (!LFS_Syslog.fp)
	{
		MessageBox(hMain,"Unable to open syslog logfile!","Fatal Error",MB_OK | MB_ICONSTOP);
		WSACleanup();
		exit(0);
	}

	/* Firewall file */
	sprintf(LFS_Firewall.szFullFile,"%s\\%s[%d-%d-%d].log",LFS_Firewall.szDirectory,LFS_Firewall.szSuffix,
		stTime.wMonth,stTime.wDay,stTime.wYear);

	fflush(LFS_Firewall.fp);
	fclose(LFS_Firewall.fp);
	LFS_Firewall.fp = fopen(LFS_Firewall.szFullFile,"a+b");

	if (!LFS_Firewall.fp)
	{
		MessageBox(hMain,"Unable to open firewall logfile!","Fatal Error",MB_OK | MB_ICONSTOP);
		WSACleanup();
		exit(0);
	}

}

void CreateMenus()
{
	MENUINFO mi;

	hMenuSystray = CreatePopupMenu();
	hMenuList = CreatePopupMenu();
	hMenuCustom = CreatePopupMenu();

	/* Systray menu */
	AppendMenu(hMenuSystray,MF_STRING,MENUID_SYSTRAY_RESTORE,"&Restore");
	AppendMenu(hMenuSystray,MF_STRING,MENUID_SYSTRAY_EXIT,"&Exit");

	SetMenuDefaultItem(hMenuSystray,0,TRUE);

	/* Listview menu */
	AppendMenu(hMenuList,MF_STRING,MENUID_LISTVIEW_COPYITEM,"&Copy ");
	AppendMenu(hMenuList,MF_STRING,MENUID_LISTVIEW_COPYALL,"Copy &everything");
	AppendMenu(hMenuList,MF_SEPARATOR,NULL,NULL);
	AppendMenu(hMenuList,MF_STRING,MENUID_LISTVIEW_CLEAR,"C&lear");

	/* Custom command menu */
	mi.cbSize = sizeof(mi);
	mi.dwStyle = MNS_NOTIFYBYPOS;
	mi.fMask = MIM_APPLYTOSUBMENUS | MIM_STYLE;
	mi.cyMax = 0;
	mi.hbrBack = NULL;
	mi.dwContextHelpID = 0;
	mi.dwMenuData = 0;

	SetMenuInfo(hMenuCustom,&mi);

	if (CMIL_IsEmpty(cmil[0]))
		AppendMenu(hMenuCustom,MF_GRAYED | MF_STRING,NULL,"None available");
	else
	/* Commands available */
	{
		MENUITEMINFO mii;
		int count;

		mii.cbSize = sizeof(mii);
		mii.fMask = MIIM_DATA;

		for (count = 0; !CMIL_IsEmpty(cmil[count]); count++)
		{
			cmil[count].dwMenuID = WM_CUSTOMMENUBASE + count;
			AppendMenu(hMenuCustom,MF_STRING,cmil[count].dwMenuID,cmil[count].szName);
			mii.dwItemData = (ULONG_PTR)&cmil[count];

			SetMenuItemInfo(hMenuCustom,count,TRUE,&mii);
		}
	}

	AppendMenu(hMenuCustom,MF_SEPARATOR,NULL,NULL);
	AppendMenu(hMenuCustom,MF_STRING,MENUID_LISTVIEW_CUSTOMADD,"&Add command...");
}

void CreateCMD32Process(CMD32PROCESS *pCMD32)
/*
** CreateCMD32Process()
** Creates a child cmd32.exe process to be used with custom menu commands.
*/
{
	HANDLE hStdOut;
	BOOL bReturn;
	char szCmdline[1024];

	pCMD32->sa.bInheritHandle = TRUE;
	pCMD32->sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	pCMD32->sa.lpSecurityDescriptor = NULL;

	/* Get the handle for the current stdout */
	hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	
	/* Create a pipe for the child stdout */
	CreatePipe(&pCMD32->hPipeStdOutputR,&pCMD32->hPipeStdOutputW,&pCMD32->sa,0);

	/* Make sure read handle to the pipe for stdout is not inherited */
	SetHandleInformation(pCMD32->hPipeStdOutputR,HANDLE_FLAG_INHERIT,0);

	/* Create another pipe for child stdin */
	CreatePipe(&pCMD32->hPipeStdInputR,&pCMD32->hPipeStdInputW,&pCMD32->sa,0);

	/* Make sure write handle to the pipe for stdin is not inherited */
	SetHandleInformation(pCMD32->hPipeStdInputW,HANDLE_FLAG_INHERIT,0);
	
	/* Start up the process */
	pCMD32->si.cb = sizeof(STARTUPINFO);
	pCMD32->si.hStdError = pCMD32->hPipeStdOutputW;
	//pCMD32->si.hStdOutput = pCMD32->hPipeStdOutputW;
	pCMD32->si.hStdOutput = pCMD32->hPipeStdOutputW;
	pCMD32->si.hStdInput = pCMD32->hPipeStdInputR;
	pCMD32->si.dwFlags |= STARTF_USESTDHANDLES;

	memset(szCmdline,0,1024);
	GetSystemDirectory(szCmdline,1024);
	strcat(szCmdline,"\\cmd.exe");

	bReturn = CreateProcess(szCmdline,
		NULL,
		NULL,
		NULL,
		TRUE,
		CREATE_NO_WINDOW,
		NULL,
		NULL,
		&pCMD32->si,
		&pCMD32->pi);

	CloseHandle(pCMD32->pi.hProcess);
	CloseHandle(pCMD32->pi.hThread);

	/* Begin inter-process communication thread */
	pCMD32->hCommunicationsThread = (HANDLE)_beginthreadex(NULL,0,CMD32Read,(void*)pCMD32,0,(unsigned int*)&pCMD32->dwCommunicationsThreadID);	
}

void CreateCMD32Window(CMD32PROCESS *pCMD32)
/*
** CreateCMD32Window()
** Creates and initialzes the window for the command prompt
*/
{
	pCMD32->hWndDialog = hMain;
	pCMD32->hWndInput = GetDlgItem(pCMD32->hWndDialog,IDC_EDITCMDINPUT);
	pCMD32->hWndOutput = GetDlgItem(pCMD32->hWndDialog,IDC_EDITCMDOUTPUT);

	//ShowWindow(pCMD32->hWndDialog,SW_SHOW);
}

unsigned __stdcall CMD32Read(void *dummy)
/*
** CMD32Read()
** Sits and waits for input from the cmd.exe pipe
*/
{
	char chbuf[4096];
	DWORD dwRead;
	CMD32PROCESS *pCMD32 = (CMD32PROCESS*)dummy;


	for (;;)
	/* Continuously read data from the pipe */
	{
		int sz = sizeof(chbuf);

		memset(chbuf,0,sz);

		if (!ReadFile(pCMD32->hPipeStdOutputR,chbuf,sz,&dwRead,NULL) || dwRead == 0)
			break;

		/* Output the data from the pipe */
		AddTextToEditControl(pCMD32->hWndOutput,chbuf);

	}

	return 0;
}

int AddTextToEditControl(HWND hWndEdit, char *pszText)
/*
** AddTextToEditControl()
** Adds the specified text to the specified edit control
*/
{
	int nLength = GetWindowTextLength(hWndEdit);

	SendMessage(hWndEdit,EM_SETSEL,nLength,nLength);
	SendMessage(hWndEdit,EM_REPLACESEL,FALSE,(LPARAM)pszText);

	return 0;
}

LRESULT CALLBACK CustomCMDProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
/*
** CustomCMDProc()
** WM Handler for the custom command dialog box
*/
{
	HWND hListBoxCMD = GetDlgItem(hDlg,IDC_LISTCMDCOMMANDS),
		hEditName = GetDlgItem(hDlg,IDC_EDITCMDNAME),
		hEditCmdline = GetDlgItem(hDlg,IDC_EDITCMDCOMMAND);

	switch (uMsg)
	{
		case WM_INITDIALOG:
		/* Load entries from table */
		{
			int count;

			if (!cmil[0].szName[0])
			/* No entries */
			{
				ListBox_AddString(hListBoxCMD,"No entries");
				SetWindowText(hEditName,"Click Add to create a new command");
				EnableWindow(hEditName,FALSE);
				SetWindowText(hEditCmdline,"Click Add to create a new command");
				EnableWindow(hEditCmdline,FALSE);
				EnableWindow(hListBoxCMD,FALSE);
			}
			else
			/* Copy the entries to the temporary table */
			{
				for (count = 0; count < MAX_CUSTOMCOMMANDS; count++)
				{
					strcpy(cmilcopy[count].szCmdline,cmil[count].szCmdline);
					strcpy(cmilcopy[count].szName,cmil[count].szName);

					if (!CMIL_IsEmpty(cmil[count]))
					/* Copy the entry to the list box */
						ListBox_AddString(hListBoxCMD,cmilcopy[count].szName);
					else
						break;
				}

				/* Put the contents of the first item in the appropriate boxes */
				ListBox_SetCurSel(hListBoxCMD,0);
				SetWindowText(hEditName,cmilcopy[0].szName);
				SetWindowText(hEditCmdline,cmilcopy[0].szCmdline);

				SetFocus(hEditName);
				SendMessage(hEditName,EM_SETSEL,0,-1);
				SendMessage(hEditName,EM_SETSEL,-1,0);
			}

			return FALSE;
		}
		break;
		case WM_COMMAND:
		{
			int id = LOWORD(wParam);
			int nMessage = HIWORD(wParam);
			HWND hWndFrom = (HWND)lParam;

			if (id == IDC_BTNCMDCANCEL)
			/* User clicked Cancel, discard changes and close dialog box */
			{
				/* Erase copy of list */
				memset(&cmilcopy,0,sizeof(cmilcopy));

				SendMessage(hDlg,WM_CLOSE,0,0);
			}
			else if (id == IDC_BTNCMDOK)
			/* User clicked OK, save list */
			{
				int count;

				/* Erase old list */
				memset(&cmil,0,sizeof(cmil));

				for (count = 0; cmilcopy[count].szName[0]; count++)
				/* Copy from new list to old list */
				{
					strcpy(cmil[count].szName,cmilcopy[count].szName);
					strcpy(cmil[count].szCmdline,cmilcopy[count].szCmdline);
					cmil[count].dwMenuID = count;
				}

				RefreshCustomMenu();
				UpdateSettings();

				SendMessage(hDlg,WM_CLOSE,0,0);
			}
			else if (id == IDC_BTNCMDADD)
			/* User clicked Add, add blank entry */
			{
				int count;

				for (count = 0; cmilcopy[count].szName[0]; count++);

				if (!cmilcopy[0].szName[0])
				/* First command being added */
				{
					EnableWindow(hListBoxCMD,TRUE);
					SendMessage(hEditName,WM_CLEAR,0,0);
					EnableWindow(hEditName,TRUE);
					SendMessage(hEditName,WM_CLEAR,0,0);
					EnableWindow(hEditCmdline,TRUE);
				}

				strcpy(cmilcopy[count].szName,"Enter command name");
				strcpy(cmilcopy[count].szCmdline,"Enter command");

				RefreshCustomCMDList(hListBoxCMD,hEditName,hEditCmdline);

				ListBox_SetCurSel(hListBoxCMD,count);
				SetWindowText(hEditName,cmilcopy[count].szName);
				SetWindowText(hEditCmdline,cmilcopy[count].szCmdline);

				/* Highlight the edit box */
				SetFocus(hEditName);
				SendMessage(hEditName,EM_SETSEL,0,-1);

			}
			else if (id == IDC_BTNCMDDELETE)
			/* User clicked Delete, delete highlighted entry */
			{
				int nGoto, nResult = ListBox_GetCurSel(hListBoxCMD);

				if (nResult != LB_ERR)
				/* Delete from the CMIL copy */
				{
					int count;

					for (count = nResult; cmilcopy[count].szName[0] && count < MAX_CUSTOMCOMMANDS; count++)
					{
						strcpy(cmilcopy[count].szName,cmilcopy[count + 1].szName);
						strcpy(cmilcopy[count].szCmdline,cmilcopy[count + 1].szCmdline);
					}

					RefreshCustomCMDList(hListBoxCMD,hEditName,hEditCmdline);

					nGoto = cmilcopy[nResult].szName[0] ? nResult : nResult - 1;

					ListBox_SetCurSel(hListBoxCMD,nGoto);
					SetWindowText(hEditName,cmilcopy[nGoto].szName);
					SetWindowText(hEditCmdline,cmilcopy[nGoto].szCmdline);

					if (!cmilcopy[0].szName[0])
					/* No more entries */
					{
						ListBox_AddString(hListBoxCMD,"No entries");
						SetWindowText(hEditName,"Click Add to create a new command");
						EnableWindow(hEditName,FALSE);
						SetWindowText(hEditCmdline,"Click Add to create a new command");
						EnableWindow(hEditCmdline,FALSE);
						EnableWindow(hListBoxCMD,FALSE);
					}
					else
					{
						SetFocus(hEditName);
						SendMessage(hEditName,EM_SETSEL,0,-1);
						SendMessage(hEditName,EM_SETSEL,-1,0);
					}
				}

			}
			else if (id == IDC_LISTCMDCOMMANDS && nMessage == LBN_SELCHANGE)
			/* List box selection has changed */
			{
				int nSel = ListBox_GetCurSel(hWndFrom);

				SetWindowText(hEditName,cmilcopy[nSel].szName);
				SetWindowText(hEditCmdline,cmilcopy[nSel].szCmdline);

				SetFocus(hEditName);
				SendMessage(hEditName,EM_SETSEL,0,-1);
				SendMessage(hEditName,EM_SETSEL,-1,0);
			}
			else if (id == IDC_EDITCMDNAME && nMessage == EN_CHANGE)
			/* Text is changing in the name box */
			{
				int nSel = ListBox_GetCurSel(hListBoxCMD);

				if (nSel >= 0)
				/* Update the appropriate info */
				{
					GetWindowText(hEditName,cmilcopy[nSel].szName,sizeof(cmilcopy[nSel].szName));

					/* Insert new string */
					SendMessage (hListBoxCMD, LB_INSERTSTRING, 
								(WPARAM)(nSel), (LPARAM)cmilcopy[nSel].szName);
					/* Delete string at nSel+1 */
					SendMessage (hListBoxCMD, LB_DELETESTRING, 
								(WPARAM)(nSel+1), 0L);
					/* Restore the current selection */
					SendMessage (hListBoxCMD, LB_SETCURSEL, 
								(WPARAM)nSel, 0L);					
				}
			}
			else if (id == IDC_EDITCMDCOMMAND && nMessage == EN_CHANGE)
			/* Text is changing in the command line box */
			{
				int nSel = ListBox_GetCurSel(hListBoxCMD);

				if (nSel >= 0)
				/* Update the appropriate info */
					GetWindowText(hEditCmdline,cmilcopy[nSel].szCmdline,sizeof(cmilcopy[nSel].szCmdline));					
			}
		}
		break;
		case WM_CLOSE:
			EndDialog(hDlg,0);
		break;
		default:
			return FALSE;
		break;
	}

	return TRUE;
}

int RefreshCustomCMDList(HWND hListBox, HWND hEditName, HWND hEditCmdline)
/*
** RefreshCustomCMDList()
** Clears and enters the entries found in cmilcopy into the listbox
** Note: This is an internal function, using the global variable cmilcopy
*/
{
	int count;

	ListBox_ResetContent(hListBox);

	for (count = 0; cmilcopy[count].szName[0] && count < MAX_CUSTOMCOMMANDS; count++)
	{
		ListBox_AddString(hListBox,cmilcopy[count].szName);
	}
	return 0;
}

int GetCurrentBrowser(char *szInput)
/*
** GetCurrentBrowser()
** Outputs the default browser to the specified string
*/
{
	HKEY hKey = NULL;
	char buf[1024];

	memset(&buf[0],0,sizeof(buf));

	if (RegOpenKeyEx(HKEY_CLASSES_ROOT,"http\\shell\\open\\command", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	/* Open the default browser */
	{
		DWORD cbData = 1024;

		if (RegQueryValueEx(hKey,NULL,NULL,NULL,NULL,&cbData) == ERROR_SUCCESS)
		/* Get the size */
		{
			if (RegQueryValueEx(hKey,NULL,NULL,NULL,(LPBYTE)&buf[0],&cbData) == ERROR_SUCCESS)
			/* Got it! */
			{
				strcpy(szInput,buf);
			}
		}
	}

	return 0;
}

int GetCMILSettings(char *szIniFile, unsigned int nProgNum, CUSTOMMENUITEMLIST *pCMIL)
/*
** GetCMILSettings()
** Retrieves the custom menu item list settings for the specified program number
** Returns -1 if number is invalid
*/
{
	char szName[1024], szCmdline[1024], szSection[256];

	sprintf(szSection,"Program-%d",nProgNum);

	GetPrivateProfileString(szSection,"name",NULL,szName,sizeof(szName),szIniFile);
	GetPrivateProfileString(szSection,"cmdline",NULL,szCmdline,sizeof(szCmdline),szIniFile);
	
	if (szName[0])
	/* Copy the entries to the table */
	{
		strcpy(pCMIL->szName,szName);
		strcpy(pCMIL->szCmdline,szCmdline);

		return 0;
	}
	else
		return -1;
}

int RefreshCustomMenu()
/*
** Refreshes the custom command menu
*/
{
	int count, nItems;
	MENUINFO mi;

	/* Custom command menu */
	mi.cbSize = sizeof(mi);
	mi.dwStyle = MNS_NOTIFYBYPOS;
	mi.fMask = MIM_APPLYTOSUBMENUS | MIM_STYLE;
	mi.cyMax = 0;
	mi.hbrBack = NULL;
	mi.dwContextHelpID = 0;
	mi.dwMenuData = 0;

	SetMenuInfo(hMenuCustom,&mi);

	/* Delete the previous menu items */
	nItems = GetMenuItemCount(hMenuCustom);

	for (count = 0; count < nItems; count++)
		RemoveMenu(hMenuCustom,0,MF_BYPOSITION);

	/* Add new menu items */
	if (CMIL_IsEmpty(cmil[0]))
		AppendMenu(hMenuCustom,MF_GRAYED | MF_STRING,NULL,"None available");
	else
	/* Commands available */
	{
		MENUITEMINFO mii;
		int count;

		mii.cbSize = sizeof(mii);
		mii.fMask = MIIM_DATA;

		for (count = 0; !CMIL_IsEmpty(cmil[count]); count++)
		{
			cmil[count].dwMenuID = WM_CUSTOMMENUBASE + count;
			AppendMenu(hMenuCustom,MF_STRING,cmil[count].dwMenuID,cmil[count].szName);
			mii.dwItemData = (ULONG_PTR)&cmil[count];

			SetMenuItemInfo(hMenuCustom,count,TRUE,&mii);
		}
	}

	AppendMenu(hMenuCustom,MF_SEPARATOR,NULL,NULL);
	AppendMenu(hMenuCustom,MF_STRING,MENUID_LISTVIEW_CUSTOMADD,"&Edit commands...");

	return 0;
}

LRESULT CALLBACK DlgSubclassProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
/*
** CMD32EditProc()
** Subclass function for shell edit control
*/
{
	int lReturn = 0;

	switch (message)
	{
		case WM_GETDLGCODE:
		{
			MSG *pMSG = (MSG*)lParam;

			if (pMSG)
			{
				if (pMSG->message == WM_KEYDOWN && (pMSG->wParam == VK_ESCAPE))
				{
					DWORD dwWritten;
					int len = 1;
					char chbuf[] = { VK_ESCAPE, '\0' };
					
					/* Echo escape thru the cmd.exe pipe */
					/* NOTE: even sending all keys thru aint doin it */
					for (int count = 0; count < 256; chbuf[0] = count++)
						WriteFile(cmd32p.hPipeStdInputW,chbuf,len,&dwWritten,NULL);

					return DLGC_WANTALLKEYS;
				}
			}
			//MessageBox(hWnd,"Test!","Test",MB_OK);
		}
		break;
	}

	return CallWindowProc((WNDPROC)OldDlgProc,hWnd,message,wParam,lParam);
}