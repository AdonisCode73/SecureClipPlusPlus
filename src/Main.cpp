#include <iostream>
#include <windows.h>
#include <json.hpp>
#include <list>
#include <algorithm>
#include <fstream>
#include <thread>
#include "sodium.h"



using json = nlohmann::json;

std::forward_list<std::wstring> content = {};
std::string g_password;
bool g_verbose = false;

// TODO:
// -> Save clipboard content to disk == DONE
// -> Load content from disk == DONE
// -> Encrypt the content with password == DONE
// -> CLI interface to retrieve, clear and access n entry == DONE			(Maybe do a visual clickable interface at a later time)
// -> Keyboard shortcut(s) == Hotkey for copying is done, maybe do others

static unsigned int listStoredContent() {
	unsigned int counter = 1;

	for (auto& val : content) {
		std::wcout << std::endl;
		std::wcout << counter++ << ". " << val << std::endl;
		std::wcout << std::endl;
	}

	if (counter == 1) {
		std::cout << "(empty)" << std::endl;
	}

	return counter;
}

static void retrieveText() {
	unsigned int counter = listStoredContent();

	if (counter > 1) {
		std::string selection;
		std::getline(std::cin, selection);
		try {
			unsigned int idx = std::stoul(selection);
			if (idx >= 1 && idx < counter) {
				auto it = content.begin();
				std::advance(it, idx - 1);

				const std::wstring& selected = *it;

				if (OpenClipboard(nullptr)) {
					EmptyClipboard();
					size_t bytes = (selected.size() + 1) * sizeof(wchar_t);
					HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
					if (hMem) {
						memcpy(GlobalLock(hMem), selected.c_str(), bytes);
						GlobalUnlock(hMem);
						SetClipboardData(CF_UNICODETEXT, hMem);
					}
					CloseClipboard();
					std::cout << "Copied to clipboard" << std::endl;
				}
				else {
					std::cout << "Failed to open clipboard" << std::endl;
				}
			}
			else {
				std::cout << "Invalid selection" << std::endl;
			}
		}
		catch (...) {
			std::cout << "Invalid Input" << std::endl;
		}
	}
}

std::string toBase64(const unsigned char* data, int len) {
	size_t b64len = sodium_base64_encoded_len(len, sodium_base64_VARIANT_ORIGINAL);
	std::string result(b64len, '\0');
	sodium_bin2base64(result.data(), b64len, data, len, sodium_base64_VARIANT_ORIGINAL);
	result.resize(strlen(result.c_str()));
	return result;
}

std::vector<unsigned char> fromBase64(const std::string& b64) {
	std::vector<unsigned char> result(b64.size());
	size_t binLen;
	if (sodium_base642bin(result.data(), result.size(), b64.data(), b64.size(), nullptr, &binLen, nullptr, sodium_base64_VARIANT_ORIGINAL) < 0) {
		throw std::runtime_error("Base 64 decoding failed");
	}

	result.resize(binLen);
	return result;
}

std::string retrievePassword(const std::string& prompt) {
	std::cout << prompt << std::endl;

	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode = 0;

	GetConsoleMode(hStdin, &mode);
	SetConsoleMode(hStdin, mode & ~ENABLE_ECHO_INPUT);

	std::string password;
	std::getline(std::cin, password);

	SetConsoleMode(hStdin, mode);
	std::cout << std::endl;

	return password;
}

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

	std::string plainText = json(vec).dump();

	unsigned char salt[crypto_pwhash_SALTBYTES];
	randombytes_buf(salt, sizeof(salt));

	unsigned char nonce[crypto_secretbox_NONCEBYTES];
	randombytes(nonce, sizeof(nonce));

	unsigned char key[crypto_box_SEEDBYTES];
	if (crypto_pwhash
	(key, sizeof(key), g_password.c_str(), g_password.size(), salt,
		crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE,
		crypto_pwhash_ALG_DEFAULT) != 0)
	{
		std::cerr << "Deriving key failed - probably out of memory" << std::endl;
		sodium_memzero(key, sizeof(key));
		return;
	}

	std::vector<unsigned char> cypherText(crypto_secretbox_MACBYTES + plainText.size());

	crypto_secretbox_easy(cypherText.data(), reinterpret_cast<const unsigned char*>(plainText.c_str()), plainText.size(), nonce, key);
	sodium_memzero(key, sizeof(key));


	json outData = {
		{"salt", toBase64(salt, sizeof(salt))},
		{"nonce", toBase64(nonce, sizeof(nonce))},
		{"cypher", toBase64(cypherText.data(), static_cast<int>(cypherText.size()))}
	};


	std::ofstream outFile("data.enc");

	if (outFile) {
		outFile << outData;
	}
	else {
		std::cerr << "Failed to write encrypted data";
	}
}

bool loadFromDisk(std::forward_list<std::wstring>& content) {
	std::ifstream inFile("data.enc");

	json inputData;
	inFile >> inputData;

	std::vector<uint8_t> saltVec = fromBase64(inputData["salt"].get<std::string>());
	std::vector<uint8_t> nonceVec = fromBase64(inputData["nonce"].get<std::string>());
	std::vector<uint8_t> cypherVec = fromBase64(inputData["cypher"].get<std::string>());

	if (saltVec.size() != crypto_pwhash_SALTBYTES || nonceVec.size() != crypto_secretbox_NONCEBYTES || cypherVec.size() < crypto_secretbox_MACBYTES) {
		std::cerr << "Corrupted Data - probably" << std::endl;
		return false;
	}

	unsigned char key[crypto_box_SEEDBYTES];
	if (crypto_pwhash(key, sizeof(key), g_password.c_str(), g_password.size(), saltVec.data(),
		crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE, crypto_pwhash_ALG_DEFAULT) < 0) {
		std::cerr << "Deriving key failed" << std::endl;
		return false;
	}

	std::vector<unsigned char> plainText(cypherVec.size() - crypto_secretbox_MACBYTES);
	if (crypto_secretbox_open_easy(plainText.data(), cypherVec.data(), cypherVec.size(), nonceVec.data(), key) < 0) {
		std::cerr << "Decryption Failed" << std::endl;
		return false;
	}

	sodium_memzero(key, sizeof(key));

	std::string plainTextStr(plainText.begin(), plainText.end());
	json jsonData = json::parse(plainTextStr);

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

	return true;
}

LRESULT CALLBACK hiddenWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {

	switch (msg) {
	case WM_HOTKEY: {

		if (g_verbose) std::clog << "Clipboard Updated" << std::endl;

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
					GlobalUnlock(clipData);
				}
				else {
					if (g_verbose) std::cerr << "Failed to Lock" << std::endl;
				}
			}
			else {
				if (g_verbose) std::cerr << "Failed to retrieve clipboard data" << std::endl;
			}

			CloseClipboard();
		}
		else {
			if (g_verbose) std::cerr << "Failed to open clipboard" << std::endl;
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

void menuPrinter(HWND* win) {
	std::string choice;
	while (true) {
		std::cout << "\n=== SecureClip++ ===" << std::endl;
		std::cout << "1. View clipboard history" << std::endl;
		std::cout << "2. Copy item to clipboard" << std::endl;
		std::cout << "3. Clear clipboard history" << std::endl;
		std::cout << "4. Toggle verbose logging [" << (g_verbose ? "ON" : "OFF") << "]" << std::endl;
		std::cout << "5. Exit" << std::endl;
		std::cout << "> " << std::flush;

		if (!std::getline(std::cin, choice)) break;
		try {
			int option = std::stoi(choice);

			switch (option) {
			case 1: {
				listStoredContent();
				break;
			}
			case 2: {
				retrieveText();
				break;
			}
			case 3: {
				content.clear();
				writeToDisk(content);
				std::cout << "Clipboard history cleared." << std::endl;
				break;
			}
			case 4: {
				g_verbose = !g_verbose;
				std::cout << "Verbose logging " << (g_verbose ? "enabled" : "disabled") << std::endl;
				break;
			}
			case 5: {
				PostMessage(*win, WM_DESTROY, 0, 0);
				break;
			}
			default:
				std::cout << "Invalid choice. Enter a choice 1...5" << std::endl;
				break;
			};
		}
		catch (...) {
			std::cout << "Invalid input. Enter a numerical value 1...5" << std::endl;
		}
	}
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

	if (RegisterHotKey(win, 1, MOD_CONTROL | MOD_SHIFT, 0x44) == 0) {
		std::cerr << "Failed to register hotkey" << std::endl;
	}

	if (sodium_init() < 0) {
		std::cerr << "Failed to init Sodium" << std::endl;
		exit(1);
	}

	if (std::ifstream("data.enc")) {
		g_password = retrievePassword("Enter password to login: ");

		if (!loadFromDisk(content)) {
			std::cerr << "Failed to load content from disk - Aborting" << std::endl;
			exit(1);
		}
	}
	else {
		g_password = retrievePassword("First time login - Set your password: ");
	}

	std::thread menuThread(&menuPrinter, &win);
	menuThread.detach();

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	RemoveClipboardFormatListener(win);
	DestroyWindow(win);

	sodium_memzero(g_password.data(), g_password.size());

	return 0;
}