#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <commctrl.h>
#include <cstdio>
#include <vector>
#include <thread>
#include <mutex>
#include <string>
#pragma comment(lib, "comctl32.lib")

#include "resource.h"
#include "constant.h"


#define WINDOW_CLASS_NAME L"WindowClass"
#define ID_MAIN_LIST 400
#define MIN_WIDTH 600
#define MIN_HEIGHT 500


HINSTANCE appInstance;
std::vector<PROCESS_INFORMATION> processInfos;
std::thread checker;
std::mutex mtx;
bool running = false;

LRESULT CALLBACK WinProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
BOOL OpenFileName(HINSTANCE hInstance, HWND hwnd, LPWSTR buffer, int sizeBuffer);
void InitListView(HINSTANCE hInstance, HWND hwnd);
BOOL LaunchApplication(LPWSTR path);
BOOL TerminateApplication(int index);
void CheckProcesses(HWND hwnd);
std::wstring GetFileName(LPWSTR filePath);


int WINAPI WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nShowCmd) 
{
	appInstance = hInstance;

	WNDCLASSEX winclass;
	HWND hwnd;
	MSG msg;

	winclass.cbSize = sizeof(winclass);
	winclass.hInstance = hInstance;
	winclass.lpszClassName = WINDOW_CLASS_NAME;
	winclass.lpszMenuName = NULL;
	winclass.lpfnWndProc = WinProc;
	winclass.cbClsExtra = 0;
	winclass.cbWndExtra = 0;
	winclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	winclass.hCursor = LoadCursor(NULL, IDC_ARROW);
	winclass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(MY_APP_ICON));
	winclass.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(MY_APP_ICON));
	winclass.style = CS_DBLCLKS | CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	
	if (!RegisterClassEx(&winclass)) {
		return 0;
	}

	HMENU hmenu = LoadMenu(hInstance, MAKEINTRESOURCE(MainMenu));

	hwnd = CreateWindowEx(
		NULL,
		WINDOW_CLASS_NAME,
		L"Диспетчер",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		500, 200, MIN_WIDTH, MIN_HEIGHT,
		NULL,
		hmenu,
		hInstance,
		NULL
	);
	
	if (!hwnd) {
		return 0;
	}
	
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.wParam;
}


LRESULT CALLBACK WinProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	switch (msg) {
	case WM_CREATE: {
		InitListView(appInstance, hwnd);
		running = true;
		checker = std::thread(CheckProcesses, hwnd);
		return 0;
	}
	case WM_COMMAND: {
		switch (LOWORD(wparam)) {
			case MENU_PROG_START: {
				WCHAR path[260];
				if (OpenFileName(appInstance, hwnd, path, sizeof(path))) {
					mtx.lock();
					if (LaunchApplication(path)) {
						HWND hwndList = GetDlgItem(hwnd, ID_MAIN_LIST);
						int itemCount = SendMessage(hwndList, LVM_GETITEMCOUNT, 0, 0);
						
						LVITEM lvi;
						lvi.mask = LVIF_TEXT;

						lvi.iItem = itemCount;
						lvi.iSubItem = 0;
						lvi.pszText = path;
						SendMessage(hwndList, LVM_INSERTITEM, itemCount, (LPARAM)&lvi);

						lvi.iSubItem = 1;
						std::wstring filename = GetFileName(path);
						lvi.pszText = const_cast<LPWSTR>(filename.c_str());
						SendMessage(hwndList, LVM_SETITEMTEXT, itemCount, (LPARAM)&lvi);
						
						lvi.iSubItem = 2;
						std::wstring processIdStr = std::to_wstring(processInfos.back().dwProcessId);
						lvi.pszText = const_cast<LPWSTR>(processIdStr.c_str());
						SendMessage(hwndList, LVM_SETITEMTEXT, itemCount, (LPARAM)&lvi);

					}
					else {
						MessageBox(hwnd, L"Не получилось запустить процесс :(", 
							L"Что-то пошло не так", MB_OK | MB_ICONWARNING);
					}
					mtx.unlock();
				}
			
				return 0;
			}
			case MENU_ABOUT: {
				MSGBOXPARAMS msgbox = { 0 };
				msgbox.cbSize = sizeof(MSGBOXPARAMS);
				msgbox.hwndOwner = hwnd;
				msgbox.hInstance = appInstance;
				msgbox.lpszText = PROGRAM_DESCRIPTION;
				msgbox.lpszCaption = L"О программе";
				msgbox.dwStyle = MB_OK | MB_USERICON;
				msgbox.lpszIcon = MAKEINTRESOURCE(MY_INFO_ICON);
				MessageBoxIndirect(&msgbox);
				//MessageBox(hwnd, PROGRAM_DESCRIPTION, L"О программе", MB_OK );
				return 0;
			}
		}
		break;
	}
	case WM_SIZE: {
		
		HWND hwndList = GetDlgItem(hwnd, ID_MAIN_LIST);
		RECT rc;
		GetClientRect(hwnd, &rc);
		MoveWindow(hwndList, 0, 0, rc.right, rc.bottom, TRUE);

		int totalWidth = rc.right;
		double k[3] = { 0.5, 0.3, 0.2 };
		int newWidth[3]{};
		int sum = 0;
		for (int i = 0; i < 2; ++i) {
			newWidth[i] = k[i] * totalWidth;
			sum += newWidth[i];
		}
		newWidth[2] = totalWidth - sum;

		for (int i = 0; i < 3; i++) {
			ListView_SetColumnWidth(hwndList, i, newWidth[i]);
		}
		return 0;
	}
	case WM_GETMINMAXINFO: {
		MINMAXINFO* mmi = (MINMAXINFO*)lparam;
		
		mmi->ptMinTrackSize.x = MIN_WIDTH;
		mmi->ptMinTrackSize.y = MIN_HEIGHT;
		return 0;
	}
	case WM_NOTIFY: {
		switch (LOWORD(wparam)) {
			case ID_MAIN_LIST: {
				switch (((LPNMHDR)lparam)->code) {
					case LVN_ITEMACTIVATE: {
					
						HWND hListView = GetDlgItem(hwnd, ID_MAIN_LIST);
						int index = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);

						if (index != -1) {
							// Если процесс уже завершен, то выведем сообщение об этом
							DWORD exitCode;
							if (GetExitCodeProcess(processInfos[index].hProcess, &exitCode)) {
								if (exitCode != STILL_ACTIVE) {
									MessageBox(hwnd, L"Процесс уже был завершен, подождите пока обновится диспетчер",
										L"Терпения мой друг", MB_OK | MB_ICONINFORMATION);
									return 0;
								}
							}
							if (TerminateApplication(index)) {
								SendMessage(hListView, LVM_DELETEITEM, index, 0);
							}
							else {
								MessageBox(hwnd, L"Не получилось завершить процесс :(", 
									L"Что-то пошло не так", MB_OK | MB_ICONWARNING);
							}
						}
						return 0;
					}
				}
				break;
			}
		}
		
		break;
	}
	case WM_CLOSE: {
		if (!processInfos.empty()) {
			int result = MessageBox(hwnd, L"Закрыть приложение?", L"Подтверждение", MB_YESNO | MB_ICONQUESTION);
			if (result == IDNO) {
				return 0;
			}
		}
		running = false;
		checker.join();
		break;
	}
	case WM_DESTROY: {
		PostQuitMessage(0);
		return 0;
	}
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}


BOOL OpenFileName(HINSTANCE hInstance, HWND hwnd, LPWSTR buffer, int sizeBuffer) {
	OPENFILENAME ofn = {};

	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = hInstance;
	ofn.lpstrFilter = L"Приложение (*.exe)\0*.exe\0\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrCustomFilter = NULL;
	ofn.lpstrFile = buffer;
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = sizeBuffer;
	ofn.lpstrFileTitle = NULL;
	ofn.lpstrInitialDir = NULL;
	ofn.lpstrTitle = L"Выберите приложение";
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	return GetOpenFileName(&ofn);
}

void InitListView(HINSTANCE hInstance, HWND hwnd) {
	HWND hwndList = CreateWindowEx(NULL, WC_LISTVIEW, L"",
		WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
		0, 0, MIN_WIDTH, MIN_HEIGHT,
		hwnd, (HMENU)ID_MAIN_LIST, hInstance, NULL);

	SendMessage(hwndList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
		LVS_EX_FULLROWSELECT | LVS_EX_TWOCLICKACTIVATE);
	
	LVCOLUMN lvc;
	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvc.fmt = LVCFMT_CENTER;

	lvc.cx = 150; 
	lvc.pszText = const_cast<LPWSTR>(L"Путь к файлу");
	SendMessage(hwndList, LVM_INSERTCOLUMN, 0, (LPARAM)&lvc);
	
	lvc.cx = 100; 
	lvc.pszText = const_cast<LPWSTR>(L"Название программы"); 
	SendMessage(hwndList, LVM_INSERTCOLUMN, 1, (LPARAM)&lvc);

	lvc.cx = 100; 
	lvc.pszText = const_cast<LPWSTR>(L"ID процесса");
	SendMessage(hwndList, LVM_INSERTCOLUMN, 2, (LPARAM)&lvc);

}

BOOL LaunchApplication(LPWSTR path) {
	STARTUPINFO si = {};
	PROCESS_INFORMATION pi = {};

	si.cb = sizeof(si);
	
	BOOL ok = CreateProcessW(
		path,
		NULL,
		NULL,
		NULL,
		FALSE,
		0,
		NULL,
		NULL, 
		&si,
		&pi 
	);
	if (!ok) {
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
	else {
		processInfos.push_back(pi);
		WaitForInputIdle(pi.hProcess, INFINITE);
	}
	return ok;
}


BOOL TerminateApplication(int index) {
	mtx.lock();
	PROCESS_INFORMATION pi = processInfos[index];
	BOOL ok = TerminateProcess(pi.hProcess, 0);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	processInfos.erase(processInfos.begin() + index);
	mtx.unlock();
	return ok;
}


void CheckProcesses(HWND hwnd) {
	HWND hwndList = GetDlgItem(hwnd, ID_MAIN_LIST);
	while (running) {
		mtx.lock();
		for (int i = 0; i < processInfos.size(); ) {
			DWORD exitCode;
			if (GetExitCodeProcess(processInfos[i].hProcess, &exitCode)) {
				if (exitCode != STILL_ACTIVE) {
					CloseHandle(processInfos[i].hProcess);
					CloseHandle(processInfos[i].hThread);
					processInfos.erase(processInfos.begin() + i);

					SendMessage(hwndList, LVM_DELETEITEM, i, 0);
					continue;
				}
			}
			++i;
		}
		mtx.unlock();
		std::this_thread::sleep_for(std::chrono::milliseconds(200)); 
	}
}


std::wstring GetFileName(LPWSTR filePath) {
	std::wstring path(filePath);

	size_t lastSlash = path.find_last_of(L"\\/");

	return path.substr(lastSlash + 1);
}