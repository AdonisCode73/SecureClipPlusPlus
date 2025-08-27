#include <iostream>
#include <windows.h>
#include <json.hpp>
#include <list>
#include <algorithm>
#include <fstream>

using json = nlohmann::json;

std::forward_list<std::wstring> content = {};

// TODO:
// -> Save clipboard content to disk == DONE
// -> Load content from disk == DONE
// -> Encrypt the content with password
// -> CLI interface to retrieve, clear and access n entry (Later replace with Imgui interface?)
// -> Keyboard shortcuts

void writeToDisk(const std::forward_list<std::wstring>& content) {
	std::vector<std::string> vec;

	// Convert wide character string to UTF 8
	for (const auto& val : content) {
		int stringSize = WideCharToMultiByte(CP_UTF8, 0, val.c_str(), static_cast<int>(val.size() + 1), nullptr, 0, nullptr, nullptr);
		if (stringSize) {
			std::string utf8String(stringSize, 0);
			WideCharToMultiByte(CP_UTF8, 0, val.c_str(), static_cast<int>(val.size() + 1), &utf8String[0], stringSize, nullptr, nullptr);
			vec.push_back(utf8String);
		}
	}
	json jsonData = vec;
	std::ofstream outFile("data.json");

	if (outFile) {
		outFile << jsonData;
	}
	else {
		std::cerr << "Failed to write JSON";
	}
}

void loadFromDisk(std::forward_list<std::wstring>& content) {
	std::ifstream inFile("data.json");

	json jsonData;
	inFile >> jsonData;

	// Retrieve UTF8 string and return it back to wide character
	for (const auto& val : jsonData) {
		std::string utf8 = val;
		int wideSize = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
		if (wideSize) {
			std::wstring wideCharString(wideSize, 0);
			MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wideCharString[0], wideSize);
			content.push_front(wideCharString);
		}
	}
}

LRESULT CALLBACK hiddenWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {

	switch (msg) {
	case WM_CLIPBOARDUPDATE: {

		std::clog << "Clipboard Updated" << std::endl;

		if (OpenClipboard(hWnd)) {
			HANDLE clipData = GetClipboardData(CF_UNICODETEXT);

			if (clipData) {
				LPCWSTR text = static_cast<LPCWSTR>(GlobalLock(clipData));
				if (text) {
					if (std::find(content.begin(), content.end(), text) == content.end()) {
						std::wstring copied_text(text);
						content.push_front(copied_text);
					}
					writeToDisk(content);
					for (auto& val : content) {
						std::wcout << val << std::endl;

					}
					GlobalUnlock(clipData);
				}
				else {
					std::cerr << "Failed to Lock" << std::endl;
				}
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

	loadFromDisk(content);
	for (auto& val : content) {
		std::wcout << val << std::endl;

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