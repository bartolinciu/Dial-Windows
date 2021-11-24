#include "url_lib.h"
#include "dial_server.h"
#include <windows.h>
#include <Winternl.h>
#include <string>
#include <fstream>
#include <iostream>
#include <experimental/filesystem>
#include "plugin_requirements.h"

void run_ssdp(int port, const char *pFriendlyName, const char * pModelName, const char *pUuid);

bool defined[2] = {0,0};

int gDialPort;
static char spModelName[] = "NOT A VALID MODEL NAME";

char spFriendlyName[MAX_COMPUTERNAME_LENGTH + 1];
char ip_addr[INET_ADDRSTRLEN];
static char spUuid[] = "deadbeef-dead-beef-dead-beefdeadbeef";


std::vector<serviceData> plugins;

INT_PTR CALLBACK DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam ){
	
	switch( message ){
		case WM_INITDIALOG:{ 
			HWND handle = GetDlgItem( hwnd, 1337 );
			SetWindowText( handle, spFriendlyName );
			return TRUE;
		}
		
		case WM_COMMAND:{
			int ctrl = LOWORD( wParam );
			int event = HIWORD( wParam );
			
			if( ctrl == IDOK && event == BN_CLICKED ){
				HWND handle = GetDlgItem( hwnd, 1337 );
				int n = GetWindowTextLength( handle );
				if( n > MAX_COMPUTERNAME_LENGTH ){
					MessageBox( NULL, "Entered name can't be longer than 15 characters!", "Error", MB_ICONERROR | MB_OK  );
					return TRUE;
				}
				GetWindowText( handle, spFriendlyName, n+1 );
				DestroyWindow(hwnd);
			}
			return TRUE;
				
		}
		
		
		case WM_CLOSE:
			DestroyWindow(hwnd);
			return TRUE;
			
		case WM_DESTROY:
			PostQuitMessage( 0 );
			return TRUE;
		
	}
	
	return FALSE;
}


void showNameSetDialog(){
	HWND dlg = CreateDialog( (HINSTANCE) GetModuleHandle( NULL ), "IDD_NMDLG", NULL, DialogProc );
	MSG msg;
	while ( GetMessage( & msg, 0, 0, 0 ) ) {
		if ( ! IsDialogMessage( dlg, & msg ) ) {
			TranslateMessage( & msg );
			DispatchMessage( & msg );
		}
	}
}

void initValues(){
	std::fstream configFile;
	configFile.open( "config.ini", std::ios::in );
	printf( "\nReading config file:\n" );
	if(!configFile.good()){
		printf( "\tConfig file not found\n" );
		configFile.close();
		//showInitDialog();
	}
	else{
		std::string key;
		std::string value;
		
		std::string line;
		while(getline(configFile, line)){
			key = line.substr(0, line.find(" = "));
			value = line.substr(line.find(" = ")+3, line.length()-1);
			if( key == "friendly name"){
				snprintf( spFriendlyName, MAX_COMPUTERNAME_LENGTH + 2, "%s",  value.c_str());
				defined[0] = true;
				printf( "\tFriendly name set to: %s\n", spFriendlyName );
			}
			else if( key == "ip address" ){
				snprintf( ip_addr, INET_ADDRSTRLEN, "%s",  value.c_str());
				defined[1] = true;
				printf( "\tIp address set to: %s\n", ip_addr );
			}
			else{
				bool parsed = false;
				//for( auto & p : plugins )
					//if( p.parseConfig( key, value ) )
						//parsed = true;
				
				if( !parsed )
					printf( "\tunknown key: %s\n", key.c_str() );
				
			}

		}
		printf("\nValidating invalid values:\n");
		if( !defined[0] ){
			DWORD len = MAX_COMPUTERNAME_LENGTH+1;
			GetComputerName(spFriendlyName, &len);
			if( MessageBox( NULL, "I haven't found your computer's name in config file. Do you want me to set it myself?", "Name not found", MB_YESNO | MB_ICONQUESTION ) == IDNO)
				showNameSetDialog();
			printf( "\tFriendly name set to: %s\n", spFriendlyName );
			std::string text = "Friendly name set to: ";
			text += spFriendlyName;
			MessageBox( NULL, text.c_str(), "Name set", MB_OK );
		}
		//if( !defined[1] || !is_ip_good( ip_addr ) )
			//showIpSetDialog();
		
		//for(auto & p : plugins)
			//p.validateValues();
				
	}
	
	
}


bool loadPlugin( std::string filename ){
	HANDLE h = LoadLibrary( filename.c_str() );
	if( h == NULL )
		return false;
	
	pGetServiceData GetServiceData =(pGetServiceData) GetProcAddress( (HMODULE)  h, "GetServiceData");
	if( GetServiceData )
		plugins.push_back( GetServiceData() );
	else
		return false;
	
	return true;
}  
	
void loadPlugins(){
	std::string path = ".\\plugins";
	printf("Loading plugins:\n");
	for (auto & p : std::experimental::filesystem::directory_iterator(path))
        if( is_regular_file( p ) && p.path().extension() == ".dll"){
			printf("\t%s: ", p.path().string().c_str());
			if( loadPlugin( p.path().string() ) )
				printf("success\n");
			else
				printf("fail\n");
		}
}
	
int main( int argc, char** argv ){
	
	loadPlugins();
	initValues();
	
	return 0;
	
	
	printf("spFriendlyName: %s\n", spFriendlyName);

	
	DIALServer ds;
	
	HANDLE h = LoadLibrary( "plugin.dll" );
	
	if( h != NULL ){
		pGetServiceData getServiceData = (pGetServiceData) GetProcAddress( (HMODULE) h, "GetServiceData" );
		if( getServiceData ){
			serviceData data = getServiceData();
			ds.register_app(data.name, &data.callbacks, data.user_data, data.useAdditionalData, data.corsAllowedOrigin);
		}
	}
	
	ds.start();
	gDialPort = ds.get_port();
	printf("launcher listening on gDialPort %d\n", gDialPort);
	run_ssdp(gDialPort, spFriendlyName, spModelName, spUuid);

	ds.stop();
	
	return 0;
}