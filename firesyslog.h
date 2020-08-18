#pragma once
#pragma warning(disable : 4996)

#include "resource.h"

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commctrl.h>
#include <winsock.h>
#include <stdio.h>
#include <process.h>
#include <direct.h>								/* for mkdir() */

#define LOGFILE_SIZE 1048576
#define SYSLOG_PORT 514
#define TRAYICON_FLASHTIME		500				/* Flash time in miliseconds */
#define MAX_CUSTOMCOMMANDS	20					/* Maximum amt of custom commands */

#define MENUID_SYSTRAY_RESTORE		(WM_APP + 1)
#define MENUID_SYSTRAY_EXIT			(WM_APP + 2)
#define MENUID_LISTVIEW_COPYITEM	(WM_APP + 3)
#define MENUID_LISTVIEW_COPYALL		(WM_APP + 4)
#define MENUID_LISTVIEW_CLEAR		(WM_APP + 5)
#define MENUID_LISTVIEW_CUSTOMADD	(WM_APP + 6)
#define MENUID_LISTVIEW_GOOGLEPORT	(WM_APP + 7)

/* Main menu options */
//#define MENUID_FILE_SETTINGS		51
//#define MENUID_FILE_EXIT			52
//#define MENUID_VIEW_SYSLOG			53
//#define MENUID_VIEW_FIREWALL		54
//#define MENUID_VIEW_LOGDIR			55

#define WM_RECVDATA			(WM_APP + 51)
#define WM_TRAYICON			(WM_APP + 52)
#define WM_TRAYFLASH		(WM_APP + 53)
#define WM_LOGCHANGE		(WM_APP + 54)
#define WM_ESCPRESSED		(WM_APP + 55)
#define WM_CUSTOMMENUBASE	(WM_APP + 100)

/* Syslog protocol codes */
#define LEVEL_EMERGENCY		0
#define LEVEL_ALERT			1
#define LEVEL_CRITICAL		2
#define LEVEL_ERROR			3
#define LEVEL_WARNING		4
#define LEVEL_NOTICE		5
#define LEVEL_INFORMATIONAL	6
#define LEVEL_DEBUG			7

#define FACILITY_KERNEL		0
#define FACILITY_USER		1
#define FACILITY_MAIL		2
#define FACILITY_SYSTEM		3
#define FACILITY_AUTH1		4
#define FACILITY_SYSLOGD	5
#define FACILITY_LINE		6
#define FACILITY_NEWS		7
#define FACILITY_UUCP		8
#define FACILITY_CLOCK1		9
#define FACILITY_AUTH2		10
#define FACILITY_FTP		11
#define FACILITY_NTP		12
#define FACILITY_LOGAUDIT	13
#define FACILITY_LOGALERT	14

typedef struct PROGRAM_OPTIONS
{
	char logdir[2048];
	BOOL bSystrayStart;
	BOOL bWindowsStart;

} PROGRAM_OPTIONS;
typedef struct SYSTRAYICONDATA
{
	NOTIFYICONDATA nid;
	DWORD dwIconFlash;
	HICON hIconActivity;
	HICON hIconIdle;
} SYSTRAYICONDATA;
typedef struct SYSLOGPACKET
{
	BYTE bFacility;
	BYTE bLevel;
	char szHostFrom[128];
	unsigned long uTime;
	char szMessage[1024];
} SYSLOGPACKET;

typedef struct ITCSTRUCT
{
	char szBuffer[65535];
	BOOL bNewData;
	SOCKET sSyslog;
	struct sockaddr_in sin;

	HANDLE hThread;
	int nThreadID;
} ITCSTRUCT;

typedef struct LOGFILESTRUCT
{
	int			nSize;
	int			nCurrent;
	char		szDirectory[1024];
	char		szFullFile[2048];
	char		szSuffix[32];
	FILE		*fp;
	//HANDLE		hFile;
	//HANDLE	hFileMap;
	//PVOID		pvFile;
	PVOID		pvPosition;
	//PVOID		pvLLI[30000];
} LOGFILESTRUCT;

typedef struct LOGFILETIMER
{
	SYSTEMTIME stLastSet;
} LOGFILETIMER;

typedef struct CMD32PROCESS
{
	SECURITY_ATTRIBUTES sa;
	SECURITY_DESCRIPTOR sd;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	DWORD dwCommunicationsThreadID;

	HANDLE hCommunicationsThread;

	HANDLE hPipeStdInputW;
	HANDLE hPipeStdOutputW;
	HANDLE hPipeStdErrorW;

	HANDLE hPipeStdInputR;
	HANDLE hPipeStdOutputR;
	HANDLE hPipeStdErrorR;

	HWND hWndDialog;
	HWND hWndOutput;
	HWND hWndInput;

	char szData[1024];

} CMD32PROCESS;

#define CMIL_IsEmpty(cmil)						(cmil.szName[0] == 0 ? TRUE : FALSE)
typedef struct CUSTOMMENUITEMLIST
{
	char szName[MAX_PATH];
	char szCmdline[MAX_PATH];
	DWORD dwMenuID;

} CUSTOMMENUITEMLIST;

/* Forward declarations */
LRESULT CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);
DLGPROC SettingsProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK CMD32Proc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK CustomCMDProc(HWND, UINT, WPARAM, LPARAM);
void ListView_AddColumn(HWND, int, char*, int);
unsigned __stdcall UDPRead(void*);
void GetSyslogPacket(char*, SYSLOGPACKET*);
void GetFacilityAndLevel(BYTE, BYTE*, BYTE*);
int InitLogFile(LOGFILESTRUCT*);
int Log_AddSyslogEntry(LOGFILESTRUCT*, SYSLOGPACKET*, char *buf);
int ParseEquals(const char*, const char*, char*);
void UpdateSettings();
void CreateMenus();
void UpdateLogFiles();
void CreateCMD32Process(CMD32PROCESS*);
void CreateCMD32Window(CMD32PROCESS*);
unsigned __stdcall CMD32Read(void*);
int AddTextToEditControl(HWND, char *);
int GetCurrentBrowser(char*);
int GetCMILSettings(char*, unsigned int, CUSTOMMENUITEMLIST*);
int RefreshCustomCMDList(HWND, HWND, HWND);
int RefreshCustomMenu();
DWORD CALLBACK FilterFunc(int, WPARAM, LPARAM);
LRESULT CALLBACK DlgSubclassProc(HWND, UINT, WPARAM, LPARAM);