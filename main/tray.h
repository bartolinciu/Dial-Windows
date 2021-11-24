
#include <windows.h>
#include <vector>
#include <iostream>
#include <thread>
#include <cstdint>

#define NULL_CALLBACK (void*)NULL
#define GRAYED 1
#define DISABLED 2
#define CHECKED 4

LRESULT CALLBACK TrayWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

namespace Tray{
	
	class Menu;
	class MenuEntry;
	class ButtonCallback;
	class TrayIcon;
	typedef void( * EntryCallback )(Menu*, unsigned int id);
	typedef void( * SimpleCallback)();
	
	class MenuEntry{
		
		Menu* parent;
		unsigned int id;
		
		Menu* menu;
		
		LPCTSTR lpEntryItem;
		EntryCallback callback;
		bool grayed;
		bool checked; 
		bool enabled;
		
		void modify();
		
		public:
		
		enum EntryType{
			STRING,
			MENU,
			SEPARATOR,
			BREAK,
			BARBREAK
		}type;
		
		MenuEntry( EntryType type, BYTE flags, LPCTSTR Item, EntryCallback callback = NULL, Menu* menu=NULL );
		
		void enable();
		void disable();
		void gray();
		void check();
		void uncheck();
		
		bool isChecked();
		bool isEnabled();
		bool isGrayed();
		
		friend Menu;
		friend TrayIcon;
	};
	
	class Menu{
		std::vector<MenuEntry> entries;
		HMENU handle;
		std::vector<Menu*> descendants;
		void trackDescendants( HWND hWnd );
		public:
		
		Menu( MenuEntry* ptr, size_t count=1 );
		HMENU getHandle();
		void show( HWND hWnd ) const;
		
		MenuEntry& operator[]( unsigned int pos );
		friend TrayIcon;
	};
	
	class ButtonCallback{
		public:
		enum BCEnum{
			MENU,
			FUNCTION,
			_HWND,
			NONE
		}type;
		union{
			const Menu* menu;
			HWND hWnd;
			SimpleCallback callback;
		}data;
		
		ButtonCallback();
		ButtonCallback( const void* a );
		ButtonCallback( const ButtonCallback& a );
		ButtonCallback( const Menu* a);
		ButtonCallback( const HWND a );
		ButtonCallback( SimpleCallback a);
	};
	
	class TrayIcon{
		bool _good;
		
		std::string lastError;
		std::thread dispatch;
		NOTIFYICONDATA nidApp;
		
		ButtonCallback lButton;
		ButtonCallback rButton;
		HWND hWnd;
		
		public:
		
		
		
		TrayIcon( const char* hint, HICON icon, ButtonCallback lButton = NULL_CALLBACK, ButtonCallback rButton = NULL_CALLBACK );
		TrayIcon( const char* hint, const char* iconPath, ButtonCallback lButton = NULL_CALLBACK, ButtonCallback rButton = NULL_CALLBACK );
		~TrayIcon();
		bool good();
		std::string getLastError();
		bool hide();
		bool show();
		void parseMenuSelection( WPARAM wParam, LPARAM lParam);
		void Callback( short side );
		
		friend LRESULT CALLBACK ::TrayWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	};
	
	
	
}