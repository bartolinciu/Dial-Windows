all:
	g++ main.cpp dial_data.o dial_server.o mongoose.o quick_ssdp.o url_lib.o nameDialog.o -lstdc++fs -o server.exe -lNtdll -lws2_32
test:all
	server.exe

tray:
	g++ tray.cpp test.cpp -o tray.exe
	tray.exe