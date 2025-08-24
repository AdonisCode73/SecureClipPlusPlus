#include <iostream>
#include <windows.h>

LRESULT CALLBACK hiddenWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {

	switch (msg) {
	case WM_CLIPBOARDUPDATE: {
		HANDLE clipData;
		LPCWSTR text;

		std::clog << "Clipboard Updated" << std::endl;

		if (OpenClipboard(hWnd)) {
			clipData = GetClipboardData(CF_UNICODETEXT);

			if (clipData) {
				text = static_cast<LPCWSTR>(GlobalLock(clipData));
				std::wcout << text << std::endl;
				GlobalUnlock(clipData);
			}

			else {
				std::cerr << "Failed to retrieve clipboard data" << std::endl;
			}

			CloseClipboard();
		}

		else {
			std::cerr << "Failed to open clipboard" << std::endl;
		}

		return 0;
	}

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

void clipBoardErrorHandler() {
	DWORD err = GetLastError();
	LPWSTR buffer = nullptr;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		err,
		NULL,
		(LPWSTR)&buffer,
		0,
		nullptr
	);

	std::wcerr << L"Error: AddClipboardFormatListener() - " << buffer << L"\n";

	if (buffer) {
		LocalFree(buffer);
	}

	exit(1);
}

int main() {

	HINSTANCE hInst = GetModuleHandle(nullptr);

	WNDCLASS wc = { 0 };
	wc.hInstance = hInst;
	wc.lpszClassName = L"Hidden Window";
	wc.lpfnWndProc = hiddenWndProc;

	if (!RegisterClass(&wc)) {
		std::cerr << "Failed register hidden window class" << std::endl;
		exit(1);
	}

	HWND win = CreateWindowEx(0, wc.lpszClassName, L"Clipboard Listener Window", 0, 0, 0, 0, 0, nullptr, nullptr, hInst, nullptr);

	if (!win) {
		std::cerr << "Failed to create hidden window" << std::endl;
		exit(1);
	}

	if (!AddClipboardFormatListener(win)) {
		clipBoardErrorHandler();
	}

	std::cout << "Listening for clipboard changes..." << std::endl;

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	RemoveClipboardFormatListener(win);
	DestroyWindow(win);

	return 0;

}