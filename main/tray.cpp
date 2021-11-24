
#include "tray.h"

#define WM_USER_SHELLICON ( WM_USER + 1 )



static std::vector<Tray::TrayIcon*> icons;
static Tray::TrayIcon* current;
static TCHAR szWindowClass[] = "wndclass";
static TCHAR szTitle[] = "caption";
static TCHAR szApplicationToolTip[100];

LRESULT CALLBACK TrayWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

namespace Tray{

	void run( bool* ready, HWND* hWnd){
		MSG msg;
		*hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		  CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, NULL, NULL);
		
		*ready = true;
		if(*hWnd != NULL)
			while (GetMessage(&msg, NULL, 0, 0))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
	}
	
	TrayIcon::~TrayIcon(){
		if(this->dispatch.joinable())
			this->dispatch.detach();
	}
	
	bool TrayIcon::show(){
		if(!Shell_NotifyIcon(NIM_ADD, &this->nidApp)){
			this->lastError = "Couldn't create tray icon";
			return false;
		}
		return true;
	}
	
	bool TrayIcon::good(){
		return this->_good;
	}
	
	TrayIcon::TrayIcon( const char* hint, HICON icon, ButtonCallback lButton, ButtonCallback rButton ){
		this->lButton = lButton;
		this->rButton = rButton;
		
		this->_good = true;
		WNDCLASS wc;
		memset(&wc, 0, sizeof(wc));
		
		size_t id = icons.size();
		icons.push_back(this);

		if(!GetClassInfo(GetModuleHandle(NULL), szWindowClass, &wc)){
			wc.style			= CS_HREDRAW | CS_VREDRAW;
			wc.lpfnWndProc	= TrayWndProc;
			wc.lpszClassName	= szWindowClass;
			wc.hInstance = GetModuleHandle(NULL);

			if( !RegisterClass(&wc) ){
				this->_good = false;
				this->lastError = "Failed to register class";
				return;
			}
		}
		
		
		HWND hWnd = NULL;
		bool ready = false;
		
		this->dispatch = std::thread(run, &ready, &hWnd );
		
		while(!ready);

		if (!hWnd){
			this->_good = false;
			this->lastError = "Failed to create window";
			return;
		}
		
		this->hWnd = hWnd;
		
		nidApp.cbSize = sizeof(NOTIFYICONDATA); // sizeof the struct in bytes 
		nidApp.hWnd = (HWND) hWnd;              //handle of the window which will process this app. messages 
		nidApp.uID = 107 + id;           //ID of the icon that will appear in the system tray 
		nidApp.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; //ORing of all the flags 
		nidApp.hIcon =  icon;
		nidApp.uCallbackMessage = WM_USER_SHELLICON + id; 
		sprintf(nidApp.szTip, hint);
	}
	
	TrayIcon::TrayIcon( const char* hint, const char* iconPath, ButtonCallback lButton, ButtonCallback rButton) : TrayIcon::TrayIcon( hint, (HICON) LoadImage( NULL, iconPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE|LR_LOADTRANSPARENT ), lButton, rButton ){}
	
	std::string TrayIcon::getLastError(){
		return this->lastError;
	}
	
	void TrayIcon::parseMenuSelection( WPARAM id, LPARAM hMenu ){
		

		if(this->lButton.type == ButtonCallback::MENU){

			Menu* lButton = (Menu*) this->lButton.data.menu;
			if( lButton->getHandle() != (HMENU)hMenu ){
				for( auto i = lButton->descendants.begin(); i != lButton->descendants.end(); i++)
					if( (*i)->getHandle() == (HMENU)hMenu){
							(*(*i))[id].callback( (*i), id );
							return;
					}
			}
			else{
				(*lButton)[id].callback( lButton, id );
				return;
			}
			
		}
		
		if(this->rButton.type == ButtonCallback::MENU){
			Menu* rButton = (Menu*) this->rButton.data.menu;
			if( rButton->getHandle() != (HMENU) hMenu){
				for( auto i = rButton->descendants.begin(); i != rButton->descendants.end(); i++){

					if( (*i)->getHandle() == (HMENU)hMenu){
						(*(*i))[id].callback( (*i), id );
						return;
					}

				}

			}
			else
				(*rButton)[ id ].callback( rButton, id );
		}
	}
	
	void TrayIcon::Callback( short side ){
		
		ButtonCallback callback;
		switch(side){
			case WM_LBUTTONDOWN:
				callback = this->lButton;
				break;
			
			case WM_RBUTTONDOWN:
				callback = this->rButton;
				break;
			default:
				return;
		}
		
		switch(callback.type){		
			case ButtonCallback::FUNCTION:
				callback.data.callback();//if callback is function call it
				break;
			case ButtonCallback::MENU:
				callback.data.menu->show(this->hWnd);//if callback is pointer to a menu show this menu
				break;
			case ButtonCallback::_HWND:{
				ShowWindow( callback.data.hWnd, SW_SHOW );
			}
			break;
		}
		
	}
	
	ButtonCallback::ButtonCallback(){};
	
	ButtonCallback::ButtonCallback( const ButtonCallback& a ){
		this->type = a.type;
		this->data = a.data;
	}
		
	ButtonCallback::ButtonCallback( const Menu* a){
		this->type = MENU;
		this->data.menu = a;
	}
		
	ButtonCallback::ButtonCallback( const void* ){
		this->type = NONE;
	}
	
	ButtonCallback::ButtonCallback( const HWND a ){
		this->type = _HWND;
		this->data.hWnd = a;
	}
		
	ButtonCallback::ButtonCallback( SimpleCallback a){
		this->type = FUNCTION;
		this->data.callback = a;
	}
	
	Menu::Menu( MenuEntry* entries, size_t count ){
		this->handle = CreatePopupMenu();
		MENUINFO mi;
		mi.fMask = MIM_STYLE;
		mi.cbSize = sizeof(mi);
		GetMenuInfo( this->handle, &mi );
		mi.fMask = MIM_STYLE | MIM_APPLYTOSUBMENUS;
		mi.dwStyle |= MNS_NOTIFYBYPOS;
		SetMenuInfo( this->handle, &mi );
		
		
		for( size_t i=0; i< count; i++ ){
			
			switch( entries[i].type ){
				
				case MenuEntry::SEPARATOR:
					InsertMenu(this->handle, -1, MF_BYPOSITION | MF_SEPARATOR, i, NULL);
					this->entries.push_back( entries[i] );
					this->entries[i].id = i;
					this->entries[i].parent = this;
					break;
					
				case MenuEntry::BREAK:
					InsertMenu(this->handle, -1, MF_BYPOSITION | MF_MENUBREAK, i, NULL);
					entries[i].id = i;
					this->entries.push_back( entries[i] );
					this->entries[i].id = i;
					this->entries[i].parent = this;
					break;
					
				case MenuEntry::BARBREAK:
					InsertMenu(this->handle, -1, MF_BYPOSITION | MF_MENUBARBREAK, i, NULL);
					this->entries.push_back( entries[i] );
					this->entries[i].id = i;
					this->entries[i].parent = this;
					break;
				
				case MenuEntry::MENU:
					this->descendants.push_back(entries[i].menu);
					InsertMenu( this->handle, -1,
						MF_BYPOSITION | MF_POPUP | MF_ENABLED*entries[i].enabled |
						MF_GRAYED * entries[i].grayed | MF_CHECKED * entries[i].checked | MF_STRING,
						(UINT_PTR) entries[i].menu->getHandle(), entries[i].lpEntryItem );
					this->entries.push_back( entries[i] );
					this->entries[i].id = i;
					this->entries[i].parent = this;
					this->descendants.insert( this->descendants.end(), this->entries[i].menu->descendants.begin(), this->entries[i].menu->descendants.end() );
					this->entries[i].menu->descendants.clear();
					break;
				
				case MenuEntry::STRING:
					InsertMenu(this->handle, -1, MF_BYPOSITION | MF_STRING | MF_ENABLED*entries[i].enabled |
						MF_GRAYED * entries[i].grayed | MF_CHECKED * entries[i].checked, i, entries[i].lpEntryItem);
					this->entries.push_back( entries[i] );
					this->entries[i].id = i;
					this->entries[i].parent = this;
					break;
				
				default:
					continue;
				
			}
			
		}
	}
	
	void Menu::show( HWND hWnd ) const{
		POINT lpClickPoint;
		GetCursorPos(&lpClickPoint);
		SetForegroundWindow(hWnd);
		TrackPopupMenu(this->handle, TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_BOTTOMALIGN, lpClickPoint.x, lpClickPoint.y, 0, hWnd, NULL);
	}
	
	HMENU Menu::getHandle(){
		return this->handle;
	}
	
	MenuEntry& Menu::operator[]( unsigned int pos ){
		return this->entries[pos];
	}
	
	MenuEntry::MenuEntry( EntryType type, BYTE flags, LPCTSTR Item, EntryCallback callback, Menu* menu ){
		this->type = type;
		if( type == SEPARATOR || type == BREAK || type == BARBREAK )
			return;
		this->checked = flags & CHECKED;
		this->grayed = flags & GRAYED;
		this->enabled = this->grayed ? false : !(flags & DISABLED);
		
		this->lpEntryItem = Item;
		if(type == MenuEntry::MENU)
			this->menu = menu;
		this->callback = callback;
	}
	
	void MenuEntry::enable(){
		if(this->enabled)
			return;
		this->enabled = true;
		this->grayed = false;
		this->modify();
		
	}
	
	void MenuEntry::disable(){
		if(!this->enabled)
			return;
		this->enabled = false;
		this->grayed = false;
		this->modify();
		
	}
	
	void MenuEntry::gray(){
		if( this->grayed )
			return;
		this->enabled = false;
		this->grayed = true;
		this->modify();
		
		
	}
	
	void MenuEntry::check(){
		if( this->checked )
			return;
		
		this->checked = true;
		this->modify();
		
	}
	
	void MenuEntry::uncheck(){
		if( !this->checked )
			return;
		
		this->checked = false;
		this->modify();
		
	}
	
	void MenuEntry::modify(){
		
		UINT flags = MF_BYPOSITION | MF_DISABLED*(!this->enabled)  | MF_GRAYED * this->grayed | MF_CHECKED * this->checked;
		UINT_PTR id = this->id;
		switch( this->type ){
			
			case STRING:
				flags |= MF_STRING;
				break;
				
			case MENU:
				flags |= MF_POPUP;
				id =(UINT_PTR) this->menu->getHandle();
				break;
			
			default:
				return;
			
		}
		
		ModifyMenu(this->parent->getHandle(), this->id, flags, id, this->lpEntryItem);
	}
	
	bool MenuEntry::isChecked(){
		return this->checked;
	}
	
	bool MenuEntry::isEnabled(){
		return this->enabled;
	}
	
	bool MenuEntry::isGrayed(){
		return this->grayed;
	}
	
}

LRESULT CALLBACK TrayWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	
	UINT id = message - WM_USER_SHELLICON;
	//printf("%u %u\n", icons.size(), id);
	if(id >= 0 && id < icons.size()){
		current = icons[id];
		current->Callback( LOWORD(lParam) );
		
		return TRUE; 
	}
	switch (message)
	{
	
	case WM_MENUCOMMAND:
		// Parse the menu selections:
		current->parseMenuSelection( wParam, lParam );
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
		
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}