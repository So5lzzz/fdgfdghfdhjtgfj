#include "gui.h"

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx9.h"
#include "../imgui/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter
);

long __stdcall WindowProcess(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter)
{
	if (ImGui_ImplWin32_WndProcHandler(window, message, wideParameter, longParameter))
		return true;

	switch (message)
	{
	case WM_SIZE: {
		if (gui::device && wideParameter != SIZE_MINIMIZED)
		{
			gui::presentParameters.BackBufferWidth = LOWORD(longParameter);
			gui::presentParameters.BackBufferHeight = HIWORD(longParameter);
			gui::ResetDevice();
		}
	}return 0;

	case WM_SYSCOMMAND: {
		if ((wideParameter & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
	}break;

	case WM_DESTROY: {
		PostQuitMessage(0);
	}return 0;

	case WM_LBUTTONDOWN: {
		gui::position = MAKEPOINTS(longParameter); // set click points
	}return 0;

	case WM_MOUSEMOVE: {
		if (wideParameter == MK_LBUTTON)
		{
			const auto points = MAKEPOINTS(longParameter);
			auto rect = ::RECT{ };

			GetWindowRect(gui::window, &rect);

			rect.left += points.x - gui::position.x;
			rect.top += points.y - gui::position.y;

			if (gui::position.x >= 0 &&
				gui::position.x <= gui::WIDTH &&
				gui::position.y >= 0 && gui::position.y <= 19)
				SetWindowPos(
					gui::window,
					HWND_TOPMOST,
					rect.left,
					rect.top,
					0, 0,
					SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER
				);
		}

	}return 0;

	}

	return DefWindowProc(window, message, wideParameter, longParameter);
}

void gui::CreateHWindow(const char* windowName) noexcept
{
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_CLASSDC;
	windowClass.lpfnWndProc = WindowProcess;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = GetModuleHandleA(0);
	windowClass.hIcon = 0;
	windowClass.hCursor = 0;
	windowClass.hbrBackground = 0;
	windowClass.lpszMenuName = 0;
	windowClass.lpszClassName = "class001";
	windowClass.hIconSm = 0;

	RegisterClassEx(&windowClass);

	window = CreateWindowEx(
		0,
		"class001",
		windowName,
		WS_POPUP,
		100,
		100,
		WIDTH,
		HEIGHT,
		0,
		0,
		windowClass.hInstance,
		0
	);

	ShowWindow(window, SW_SHOWDEFAULT);
	UpdateWindow(window);
}

void gui::DestroyHWindow() noexcept
{
	DestroyWindow(window);
	UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
}

bool gui::CreateDevice() noexcept
{
	d3d = Direct3DCreate9(D3D_SDK_VERSION);

	if (!d3d)
		return false;

	ZeroMemory(&presentParameters, sizeof(presentParameters));

	presentParameters.Windowed = TRUE;
	presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
	presentParameters.BackBufferFormat = D3DFMT_UNKNOWN;
	presentParameters.EnableAutoDepthStencil = TRUE;
	presentParameters.AutoDepthStencilFormat = D3DFMT_D16;
	presentParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

	if (d3d->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		window,
		D3DCREATE_HARDWARE_VERTEXPROCESSING,
		&presentParameters,
		&device) < 0)
		return false;

	return true;
}

void gui::ResetDevice() noexcept
{
	ImGui_ImplDX9_InvalidateDeviceObjects();

	const auto result = device->Reset(&presentParameters);

	if (result == D3DERR_INVALIDCALL)
		IM_ASSERT(0);

	ImGui_ImplDX9_CreateDeviceObjects();
}

void gui::DestroyDevice() noexcept
{
	if (device)
	{
		device->Release();
		device = nullptr;
	}

	if (d3d)
	{
		d3d->Release();
		d3d = nullptr;
	}
}

void gui::CreateImGui() noexcept
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ::ImGui::GetIO();

	io.IniFilename = NULL;

	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX9_Init(device);
}

void gui::DestroyImGui() noexcept
{
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void gui::BeginRender() noexcept
{
	MSG message;
	while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);

		if (message.message == WM_QUIT)
		{
			isRunning = !isRunning;
			return;
		}
	}

	// Start the Dear ImGui frame
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void gui::EndRender() noexcept
{
	ImGui::EndFrame();

	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

	device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 255), 1.0f, 0);

	if (device->BeginScene() >= 0)
	{
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		device->EndScene();
	}

	const auto result = device->Present(0, 0, 0, 0);

	// Handle loss of D3D9 device
	if (result == D3DERR_DEVICELOST && device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
		ResetDevice();
}

void gui::Render() noexcept
{
	ImGui::SetNextWindowPos({ 0, 0 });
	ImGui::SetNextWindowSize({ WIDTH, HEIGHT });
	ImGui::Begin(
		"Eclipse Executor",
		&isRunning,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoMove
	);

	ImGui::Button("Inject");
#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include <vector>
#include <Psapi.h>

	// Function to enable the SE_DEBUG_NAME privilege for a given process and return the target thread ID
	std::pair<DWORD, HANDLE> EnableAllPrivilegesForProcess(const wchar_t* targetModuleName) {
		DWORD targetThreadId = 0;
		HANDLE hToken = NULL;

		// Find the RobloxPlayerBeta.exe process by name
		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

		if (hSnapshot != INVALID_HANDLE_VALUE) {
			PROCESSENTRY32 processEntry;
			processEntry.dwSize = sizeof(PROCESSENTRY32);
			if (Process32First(hSnapshot, &processEntry)) {
				do {
					if (_wcsicmp(processEntry.szExeFile, targetModuleName) == 0) {
						DWORD processId = processEntry.th32ProcessID;

						// Attempt to open the process handle
						HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId);
						if (hProcess != NULL) {
							// Attempt to open the process token
							if (OpenProcessToken(hProcess, TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
								// Enable all privileges for the process token
								TOKEN_PRIVILEGES tokenPrivileges;
								tokenPrivileges.PrivilegeCount = 4;  // Set count to 0 to enable all privileges

								TCHAR processName[MAX_PATH];
								if (GetProcessImageFileName(hProcess, processName, MAX_PATH) > 0) {
									std::wcout << L"The process name for the elevated thread is: " << processName << std::endl;
								}

								if (AdjustTokenPrivileges(hToken, FALSE, &tokenPrivileges, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) {
									// Close the process handle
									CloseHandle(hProcess);

									// Take a snapshot of the threads in the specified process
									HANDLE hThreadSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
									if (hThreadSnapshot != INVALID_HANDLE_VALUE) {
										THREADENTRY32 threadEntry = { sizeof(THREADENTRY32) };
										if (Thread32First(hThreadSnapshot, &threadEntry)) {
											do {
												if (threadEntry.th32OwnerProcessID == processId) {
													// Optionally, you can add additional checks here to determine the target thread
													targetThreadId = threadEntry.th32ThreadID;
													break;
												}
											} while (Thread32Next(hThreadSnapshot, &threadEntry));
										}
										CloseHandle(hThreadSnapshot);
									}
								}
								else {
									std::cerr << "Failed to enable all privileges for the process token. Error: " << GetLastError() << std::endl;
									CloseHandle(hToken);  // Close the token handle in case of failure
									CloseHandle(hProcess);
								}
							}
							else {
								std::cerr << "Failed to open process token. Error: " << GetLastError() << std::endl;
								CloseHandle(hProcess);  // Close the process handle in case of failure
							}
						}
						else {
							std::cerr << "Failed to open target process. Error: " << GetLastError() << std::endl;
						}

						// Break the loop once the target process is found
						break;
					}
				} while (Process32Next(hSnapshot, &processEntry));
			}
			else {
				std::cerr << "Process32First failed. Error: " << GetLastError() << std::endl;
			}
			CloseHandle(hSnapshot);
		}
		else {
			std::cerr << "CreateToolhelp32Snapshot failed. Error: " << GetLastError() << std::endl;
		}

		return std::make_pair(targetThreadId, hToken);
	}

	// Function to retrieve and print the permissions of a thread
	void PrintPrivileges(HANDLE hToken) {
		DWORD dwLengthNeeded;
		if (!GetTokenInformation(hToken, TokenPrivileges, NULL, 0, &dwLengthNeeded) && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
			std::cerr << "Failed to retrieve token privileges. Error: " << GetLastError() << std::endl;
			return;
		}

		PTOKEN_PRIVILEGES tokenPrivileges = reinterpret_cast<PTOKEN_PRIVILEGES>(new BYTE[dwLengthNeeded]);

		if (!GetTokenInformation(hToken, TokenPrivileges, tokenPrivileges, dwLengthNeeded, &dwLengthNeeded)) {
			std::cerr << "Failed to retrieve token privileges. Error: " << GetLastError() << std::endl;
			delete[] tokenPrivileges;
			return;
		}

		std::cout << "Privileges after elevation:" << std::endl;
		for (DWORD i = 0; i < tokenPrivileges->PrivilegeCount; i++) {
			LUID_AND_ATTRIBUTES privilege = tokenPrivileges->Privileges[i];
			DWORD privilegeNameLength = 0;
			LookupPrivilegeName(NULL, &privilege.Luid, NULL, &privilegeNameLength);

			if (privilegeNameLength > 0) {
				std::vector<wchar_t> privilegeName(privilegeNameLength);
				if (LookupPrivilegeName(NULL, &privilege.Luid, privilegeName.data(), &privilegeNameLength)) {
					std::wcout << L"  " << privilegeName.data();
					if (privilege.Attributes & SE_PRIVILEGE_ENABLED) {
						std::wcout << L" (Enabled)";
					}
					std::wcout << std::endl;

					// Check for specific privileges (e.g., read and write)
					if (wcscmp(privilegeName.data(), L"SeBackupPrivilege") == 0) {
						std::wcout << L"    (This privilege allows backup access)" << std::endl;
					}
					if (wcscmp(privilegeName.data(), L"SeRestorePrivilege") == 0) {
						std::wcout << L"    (This privilege allows restore access)" << std::endl;
					}
					// Add more checks for other privileges as needed
				}
			}
		}

		delete[] tokenPrivileges;
	}



	int main() {

		const wchar_t* targetModuleName = L"RobloxPlayerBeta.exe";

		// Enable SE_DEBUG_NAME privilege for the specified module and get the target thread ID
		std::pair<DWORD, HANDLE> result = EnableAllPrivilegesForProcess(targetModuleName);
		DWORD targetThreadId = result.first;
		HANDLE hToken = result.second;

		if (targetThreadId != 0) {
			// You have elevated privileges for the target thread
			// Proceed with your operations on that thread
			std::cout << "Elevated privileges granted for thread with ID: " << targetThreadId << std::endl;
			PrintPrivileges(hToken);
			std::cout << "Bypassed Byfron, (ENABLED ALL THREADS) - you will have to write some code that will hook into the thread that has all permissions enabled" << std::endl;
			std::cout << "Show Some love to Nano for creating this bypass my Discord is N..#5540";
			std::cout << "Discord Server: https://discord.gg/H58pNXsXzP" << std::endl;
		}
		else {
			std::cerr << "Failed to obtain target thread ID or enable privileges." << std::endl;
		}

		if (hToken != NULL) {
			CloseHandle(hToken);  // Close the token handle
		}

		return 0;
	}

	ImGui::End();

	ImGui::InputText;

	void ImGui::End();
}
