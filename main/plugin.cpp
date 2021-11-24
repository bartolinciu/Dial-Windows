#include "plugin_requirements.h"
#include <windows.h>
#include <vector>


std::string spAppYouTube = "YouTube na telewizory";
std::string spAppYouTubeExecutable = "C:\\PROGRA~2\\Google\\Chrome\\Application\\chrome.exe";

struct WND{
	HWND handle;
	unsigned long pid;
	operator bool(){return handle!=NULL && pid!=0;} 
} spAppYouTubeWindow;std::vector<WND> wnds;

BOOL CALLBACK _getWindowSnap( HWND handle, LPARAM lParam ){
	char clas[20];
	GetClassName(handle, clas, 20);
	if(std::string(clas) == "Chrome_WidgetWin_1"){
		int len = SendMessage( handle, WM_GETTEXTLENGTH, 0, 0);
		LPSTR text = ( LPSTR ) GlobalAlloc(GPTR, len + 1);
		int cnt = SendMessage( handle, WM_GETTEXT, (LPARAM) len + 1, (WPARAM) text);
		if( cnt != 0 && cnt == len ){
			if(std::string(text) == spAppYouTube){
				printf("%s\n",clas); 
				unsigned long pid;
				GetWindowThreadProcessId( handle, &pid );//Chrome_WidgetWin_1
				wnds.push_back(WND{handle, pid});
			}
		}
		GlobalFree((HGLOBAL)text);
	}
	return TRUE;
}

void CreateWindowsSnapshot(){
	EnumWindows(_getWindowSnap, 0);
}

BOOL CALLBACK _idetifyNewWindow( HWND handle, LPARAM lParam ){
	char clas[20];
	GetClassName(handle, clas, 20);
	if(std::string(clas) == "Chrome_WidgetWin_1"){
		int len = SendMessage( handle, WM_GETTEXTLENGTH, 0, 0);
		LPSTR text = ( LPSTR ) GlobalAlloc(GPTR, len + 1);
		int cnt = SendMessage( handle, WM_GETTEXT, (LPARAM) len + 1, (WPARAM) text);
		if( cnt != 0 && cnt == len ){
			
			if(std::string(text) == spAppYouTube){
				for(size_t i=0; i < wnds.size(); i++){
					if(wnds[i].handle == handle ){
						GlobalFree((HGLOBAL)text);
						return TRUE;
					}
				}
				unsigned long pid;
				GetWindowThreadProcessId( handle, &pid );
				spAppYouTubeWindow = WND{handle, pid};
				GlobalFree((HGLOBAL)text);
				return FALSE;
			}
		}
		GlobalFree((HGLOBAL)text);
	}
	return TRUE;
}

void identifyNewWindow(){
	EnumWindows(_idetifyNewWindow, 0);
};

BOOL CALLBACK _isWndRunning( HWND handle, LPARAM lParam ){
	WND* out = (WND*) lParam;
	
	if(handle == spAppYouTubeWindow.handle){
		*out = spAppYouTubeWindow;
		return FALSE;
	}
	
	return TRUE;
}

WND isWndRunning(){
	WND out = {NULL, 0};
	EnumWindows(_isWndRunning, (LPARAM) &out);
	return out;
}

void setWndFulscreen(){
	HWND hwnd = spAppYouTubeWindow.handle;
	DWORD dwStyle = GetWindowLong( hwnd, GWL_STYLE );
	if( dwStyle &WS_OVERLAPPEDWINDOW ){
		MONITORINFO mi = {sizeof(mi)};
		if(GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)){
			SetWindowLong(hwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
			SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		}
	}
}

DIALStatus youtube_start(const char *appname, const char * payload, const char * additionalDataUrl, DIAL_run_t *run_id, void *callback_data){
	printf("\n\n ** LAUNCH YouTube ** with payload %s\n\n", payload);
	std::string url;
	if(payload != "" && additionalDataUrl !="")
		url = "https://www.youtube.com/tv?" + std::string(payload) + "&" + std::string(additionalDataUrl);
	else if(payload != "")
		url = "https://www.youtube.com/tv?" + std::string(payload);
	else
		url = "https://www.youtube.com/tv";
	CreateWindowsSnapshot();
	system(("start " + spAppYouTubeExecutable + " --app="+url).c_str());
	Sleep(4000);
	identifyNewWindow();
	if(isWndRunning){
		setWndFulscreen();
	}
	return kDIALStatusRunning;
}

DIALStatus youtube_hide(const char *appname, DIAL_run_t *run_id, void *callback_data){
	return (isWndRunning()) ? kDIALStatusRunning : kDIALStatusStopped;
}

DIALStatus youtube_status(const char *appname,
                                 DIAL_run_t run_id, int *pCanStop, void *callback_data){
	*pCanStop = 1;
	return(isWndRunning()) ? kDIALStatusRunning : kDIALStatusStopped;
}

void youtube_stop(const char *appname, DIAL_run_t run_id, void *callback_data){
	printf("Stopping YouTube\n");
	WND pid = isWndRunning();
	SendMessage( pid.handle, 2, 0, 0 );
}

static DIALAppCallbacks Callbacks = {
	youtube_start,
	youtube_hide,
	youtube_stop,
	youtube_status
};


extern "C"{
	bool ParseConfig( std::string key, std::string value ){
		if(key == "chrome path"){
			spAppYouTubeExecutable = value;
		}
		else
			return false;
	}
}

static serviceData data = {
	ParseConfig,
	"YouTube",
	NULL,
	Callbacks,
	1,
	".youtube.com"
};

extern "C" {
	serviceData GetServiceData(){
		return data;
	}
}