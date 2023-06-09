#include "WineYuanShenFpsCracker.h"

//通过内存搜索获取fps数值和垂直同步数值的内存地址
//搜索步骤github上的，不解释
void GetUnityFPSLimitCodeAndVsyncAddress(HANDLE hProcess, LPCWSTR pwszModuleNames[], const PVOID* pModulesStart, const SIZE_T* pModulesSize, const SIZE_T searchModuleCount, PBYTE* pFps, PBYTE* pVSync)
{
	if (pFps != NULL && pVSync != NULL)
	{
		*pFps = NULL;
		*pVSync = NULL;

		//模块入参搜索
		auto searchModuleInfoFunc = [](LPCWSTR pwszModuleNames[], const SIZE_T searchModuleCount, LPCWSTR pwszTargetStr) -> SIZE_T {
			for (SIZE_T index = 0; index < searchModuleCount; index++)
			{
				if (!wcscmp(pwszModuleNames[index], pwszTargetStr))
					return index;
			}
			return -1;
		};
		//机器码扫描，输入的是word，高字节表示是否匹配这个字节，为非0时不匹配，低字节是原机器码
		auto searchMatchCode = [](LPBYTE searchStart, SIZE_T searchByteLen, const WORD* matchCode, SIZE_T nMatchCodeLen) -> LPBYTE
		{
			for (SIZE_T byteOffset = 0; byteOffset + nMatchCodeLen < searchByteLen; ++byteOffset)
			{
				bool matched = true;
				for (SIZE_T matchCodeIndex = 0; matchCodeIndex < nMatchCodeLen; ++matchCodeIndex)
				{
					if (!(matchCode[matchCodeIndex] & 0xff00) && (matchCode[matchCodeIndex] & 0xff) != searchStart[matchCodeIndex + byteOffset])
					{
						matched = false;
						break;
					}
				}
				if (matched)
					return searchStart + byteOffset;
			}
			return NULL;
		};

		SIZE_T upIndex = searchModuleInfoFunc(pwszModuleNames, searchModuleCount, L"UnityPlayer.dll");
		SIZE_T uaIndex = searchModuleInfoFunc(pwszModuleNames, searchModuleCount, L"UserAssembly.dll");

		PBYTE pUpBuffer = NULL;
		PBYTE pUaBuffer = NULL;

		SIZE_T memSizeProcessed = 0;

		do
		{
			if (uaIndex == -1 && upIndex == -1)
				break;

			pUpBuffer = (PBYTE)malloc(pModulesSize[upIndex]);
			pUaBuffer = (PBYTE)malloc(pModulesSize[uaIndex]);

			if (pUpBuffer == NULL || pUaBuffer == NULL)
				break;

			ReadProcessMemory(hProcess, pModulesStart[upIndex], pUpBuffer, pModulesSize[upIndex], &memSizeProcessed);
			if (memSizeProcessed != pModulesSize[upIndex])
				break;

			ReadProcessMemory(hProcess, pModulesStart[uaIndex], pUaBuffer, pModulesSize[uaIndex], &memSizeProcessed);
			if (memSizeProcessed != pModulesSize[uaIndex])
				break;

			LONG offset = ((PIMAGE_DOS_HEADER)pUaBuffer)->e_lfanew;
			DWORD timeDateStamp = ((PIMAGE_NT_HEADERS)(pUaBuffer + offset))->FileHeader.TimeDateStamp;
			LPBYTE pCodeStart = NULL;
			const WORD match2[10] = { 0xE8,0x1FF,0x1FF,0x1FF,0x1FF,0x8B,0xE8,0x49,0x8B,0x1E };
			const SIZE_T match2Size = sizeof match2 / sizeof match2[0];
			
			//FPS地址算法根据报版本选择

			// 3.7 以前的老算法
			if (timeDateStamp < 0x645B245A)
			{
				const WORD match1[4] = { 0x7F,0x0F,0x8B,0x05 };

				const SIZE_T match1Size = sizeof match1 / sizeof match1[0];
				
				pCodeStart = searchMatchCode(pUpBuffer, pModulesSize[upIndex], match1, match1Size);
				if (pCodeStart != NULL)
				{
					DWORD ripOffset = *((DWORD*)(pCodeStart + 4));
					*pFps = (PBYTE)(ripOffset + (PBYTE)pModulesStart[upIndex] + offset + 8);
				}
			}
			// 3.7 之后的新算法
			else
			{
				const WORD match3[] = { 0xE8, 0x1FF, 0x1FF, 0x1FF, 0x1FF, 0x85, 0xC0, 0x7E, 0x07, 0xE8, 0x1FF, 0x1FF, 0x1FF, 0x1FF, 0xEB, 0x05 };
				const SIZE_T match3Size = sizeof match3 / sizeof match3[0];

				pCodeStart = searchMatchCode(pUaBuffer, pModulesSize[uaIndex], match3, match3Size);
				if (pCodeStart != NULL)
				{
					pCodeStart += (*(int32_t*)(pCodeStart + 1)) + 5;
					pCodeStart += (*(int32_t*)(pCodeStart + 3)) + 7;

					LPVOID ptr = NULL;
					LPBYTE pTarget = pCodeStart - pUaBuffer + (LPBYTE)pModulesStart[uaIndex];
					do
					{
						ReadProcessMemory(hProcess, (LPCVOID)pTarget, &ptr, sizeof ptr, &memSizeProcessed);
					} while (!ptr && memSizeProcessed == sizeof ptr);
					if (ptr != NULL)
					{
						pCodeStart = (LPBYTE)ptr - (SIZE_T)pModulesStart[upIndex] + (SIZE_T)pUpBuffer;

						while (*pCodeStart == 0xE8 || *pCodeStart == 0xE9)
							pCodeStart += (*(int32_t*)(pCodeStart + 1)) + 5;

						*pFps = (pCodeStart + (*(int32_t*)(pCodeStart + 2)) + 6);
						*pFps -= (SIZE_T)pUpBuffer;
						*pFps += (SIZE_T)pModulesStart[upIndex];
					}
				}
			}

			pCodeStart = searchMatchCode(pUpBuffer, pModulesSize[upIndex], match2, match2Size);
			if (pCodeStart != NULL)
			{
				PBYTE pVsyncCode = pCodeStart;
				INT firstOffset = *((PINT)(&pVsyncCode[1]));
				pVsyncCode = pVsyncCode + firstOffset + 5;
				DWORD secondOffset = *((PDWORD)(pVsyncCode + 3));
				PBYTE ppVsync = secondOffset + pVsyncCode + 7;
				SIZE_T pVSyncStart = 0;
				ppVsync -= (SIZE_T)pUpBuffer;
				ppVsync += (SIZE_T)(pModulesStart[upIndex]);
				do
				{
					ReadProcessMemory(hProcess, ppVsync, &pVSyncStart, sizeof pVSyncStart, &memSizeProcessed);
				} while (memSizeProcessed == sizeof pVSyncStart && !pVSyncStart);
				pVsyncCode += 7;
				DWORD pVsyncOffset = *((PDWORD)(pVsyncCode + 2));
				*pVSync = (PBYTE)(pVsyncOffset + pVSyncStart);
			}

		} while (false);

		if (pUpBuffer != NULL)
			free(pUpBuffer);
		if (pUaBuffer != NULL)
			free(pUaBuffer);
	}
}
//通过 ToolHelp32 API 获取 UnityPlayer.dll 的 基地址 和 大小 用于内存搜索
//根据数据搜索算法 3.7 之后 FPS 地址改变，需要判断 UserAssembly.dll 的版本
void GetModuleStartAndSize(HANDLE hProcess, LPCWSTR pwszModuleNames[], PVOID* pStart, PSIZE_T pSize, SIZE_T searchModuleCount)
{
	

	if (pStart != NULL && pSize != NULL && pwszModuleNames != NULL && hProcess != NULL)
	{
		for (SIZE_T index = 0; index < searchModuleCount; index++)
		{
			pStart[index] = NULL;
			pSize[index] = NULL;
		}
		DWORD modulesExists;
		DWORD moduleArraySize = 0;
		
		EnumProcessModules(hProcess, NULL, 0, &modulesExists);
		HMODULE* phModuleArray = (HMODULE*)malloc(sizeof(HMODULE) * modulesExists);
		if (phModuleArray != NULL)
		{
			moduleArraySize = modulesExists;
			if (EnumProcessModules(hProcess, phModuleArray, moduleArraySize, &modulesExists))
			{
				for (DWORD moduleIndex = 0; moduleIndex < moduleArraySize; moduleIndex++)
				{
					WCHAR szModuleName[MAX_PATH]{};
					if (!GetModuleBaseNameW(hProcess, phModuleArray[moduleIndex], szModuleName, MAX_PATH))
						continue;

					wprintf(L"%s\n", szModuleName);

					for (SIZE_T nameIndex = 0; nameIndex < searchModuleCount; nameIndex++)
					{
						if (!wcscmp(pwszModuleNames[nameIndex], szModuleName))
						{
							
							MODULEINFO modInfo{};
							if (!GetModuleInformation(hProcess, phModuleArray[moduleIndex], &modInfo, sizeof(MODULEINFO)))
								continue;

							pStart[nameIndex] = modInfo.lpBaseOfDll;
							pSize[nameIndex] = modInfo.SizeOfImage;
						}
					}
				}
			}
			free(phModuleArray);
		}
	}
}

//线程入口（这个线程用于写原神内存，破解fps限制）
unsigned __stdcall CrackThreadEntry(void* param)
{
	//AllocConsole();
	//freopen("CON", "w", stdout);
	PROCESS_INFORMATION pi = {};
	//HANDLE hProcess = NULL;
	//STARTUPINFOW si = {};
	//si.cb = sizeof si;
	DWORD dwExitCode = 0;
	WineYuanShenFpsCracker::BackgroundThreadInfo* pInfo = (WineYuanShenFpsCracker::BackgroundThreadInfo*)param;

	PBYTE pFpsLimitAddress, pVsyncAddress;
	DWORD temp;
	SIZE_T memSizeProcessed = 0;
	INT vsync = 0;

	LPCWSTR moduleNames[] = { L"UnityPlayer.dll", L"UserAssembly.dll" };
	const SIZE_T searchModuleCount = sizeof moduleNames / sizeof moduleNames[0];
	PVOID searchModulesStart[searchModuleCount] = {};
	SIZE_T searchModulesSize[searchModuleCount] = {};

	do
	{
		dwExitCode = 1;
		pi.hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pInfo->pid);
		if (pi.hProcess == NULL)
			break;
		//if (!CreateProcessW(L"D:\\Apps\\Genshin Impact\\Genshin Impact Game\\YuanShen.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
		//	break;
		//Sleep(4000);
		dwExitCode = 2;
		GetModuleStartAndSize(pi.hProcess, moduleNames, searchModulesStart, searchModulesSize, searchModuleCount);
		bool searchModulesOK = true;
		for (SIZE_T index = 0; index < searchModuleCount; index++)
		{
			if (searchModulesStart[index] == NULL || searchModulesSize == NULL)
			{
				searchModulesOK = false;
				break;
			}
		}
		if (!searchModulesOK)
			break;
		dwExitCode = 3;
		GetUnityFPSLimitCodeAndVsyncAddress(pi.hProcess, moduleNames, searchModulesStart, searchModulesSize, searchModuleCount, &pFpsLimitAddress, &pVsyncAddress);
		if (pFpsLimitAddress == NULL || pVsyncAddress == NULL)
			break;
		dwExitCode = 4;
		if (!VirtualProtectEx(pi.hProcess, (PVOID)searchModulesStart[0], sizeof searchModulesSize[0], PAGE_READWRITE, &temp))
			break;
		dwExitCode = 5;
		do
		{
			//写fps
			WriteProcessMemory(pi.hProcess, pFpsLimitAddress, &pInfo->fps, sizeof pInfo->fps, &memSizeProcessed);
			if (memSizeProcessed != sizeof pInfo->fps)
				break;
			//写垂直同步
			WriteProcessMemory(pi.hProcess, pVsyncAddress, &vsync, sizeof vsync, &memSizeProcessed);
			if (memSizeProcessed != sizeof vsync)
				break;
			//检测线程退出标记
			if (InterlockedAdd(&pInfo->exitCtrl, 0))
			{
				dwExitCode = 0;
				break;
			}
		} while (true);
	} while (false);
	//if (pi.hThread != NULL)
	//	CloseHandle(pi.hThread);
	if (pi.hProcess)
		CloseHandle(pi.hProcess);
	return dwExitCode;
}

WineYuanShenFpsCracker::WineYuanShenFpsCracker(QWidget* parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);
	//setWindowFlags(Qt::WindowType::Tool);
	QIcon icon = QIcon("icons/256x256.ico");
	pMenu = new QMenu();
	pMenu->addAction(u8"Show", this, &WineYuanShenFpsCracker::ShowHandler);
	pMenu->addAction(u8"Exit", this, &WineYuanShenFpsCracker::ExitHandler);
	pTaskBarIcon = new QSystemTrayIcon();
	pTaskBarIcon->setIcon(icon);
	pTaskBarIcon->setContextMenu(pMenu);

	//应用程序实例检测
	hMutex = CreateMutexW(NULL, TRUE, L"WineYuanShenFPSCracker");
	if (hMutex == NULL || WaitForSingleObject(hMutex, 100) != WAIT_OBJECT_0)
	{
		QMessageBox::critical(this, u8"Start Error:", u8"The application can only run an instance ", QMessageBox::StandardButton::Ok);
		if (hMutex != NULL)
			CloseHandle(hMutex);
		hMutex = NULL;
		//检测失败，标记退出，并且启用定时器执行退出代码
		isActionExit = true;
		timerIndex = startTimer(0);
	}
	else
	{
		hMutex = CreateMutexW(NULL, TRUE, L"WineYuanShenFPSCracker");
	}
}

WineYuanShenFpsCracker::~WineYuanShenFpsCracker()
{
	//###############################################
	//标记并等待线程退出（下同）
	if (threadHandle != NULL)
	{
		InterlockedAdd(&backgroundThreadInfo.exitCtrl, 1);
		DWORD dwExitCode = 0;
		do
		{
			GetExitCodeThread((HANDLE)threadHandle, &dwExitCode);
		} while (dwExitCode == STILL_ACTIVE);
		CloseHandle((HANDLE)threadHandle);
	}
	//###############################################
	if (timerIndex != -1)
		killTimer(timerIndex);
	if (hMutex != NULL)
	{
		ReleaseMutex(hMutex);
		CloseHandle(hMutex);
	}
	delete pMenu;
	delete pTaskBarIcon;
}

void WineYuanShenFpsCracker::StopBtnHandler()
{
	if (threadHandle != NULL && timerIndex != -1)
	{
		ui.stopBtn->setEnabled(false);
		InterlockedAdd(&backgroundThreadInfo.exitCtrl, 1);
		DWORD dwExitCode = 0;
		do
		{
			GetExitCodeThread((HANDLE)threadHandle, &dwExitCode);
		} while (dwExitCode == STILL_ACTIVE);
		CloseHandle((HANDLE)threadHandle);
		threadHandle = NULL;
		killTimer(timerIndex);
		timerIndex = -1;
		ui.startBtn->setEnabled(true);
		ui.pidBox->setEnabled(true);
		ui.fpsBox->setEnabled(true);
	}
}

void WineYuanShenFpsCracker::timerEvent(QTimerEvent* event)
{
	//标记为退出，执行退出代码
	if (isActionExit)
	{
		killTimer(timerIndex);
		timerIndex = -1;
		close();
	}

	DWORD dwExitCode = STILL_ACTIVE;
	if (GetExitCodeThread((HANDLE)threadHandle, &dwExitCode) && dwExitCode != STILL_ACTIVE)
	{
		CloseHandle((HANDLE)threadHandle);
		threadHandle = NULL;
		killTimer(timerIndex);
		timerIndex = -1;

		QString exitCode = QString::number(dwExitCode);

		QMessageBox::critical(this, u8"Crack Error:", u8"Crack thread stopped, error code = " + exitCode, QMessageBox::StandardButton::Ok);

		ui.startBtn->setEnabled(true);
		ui.stopBtn->setEnabled(false);
		ui.pidBox->setEnabled(true);
		ui.fpsBox->setEnabled(true);
		showNormal();
	}
}

void WineYuanShenFpsCracker::hideEvent(QHideEvent* event)
{
	if (!isActionExit)
	{
		pTaskBarIcon->setVisible(true);
		pTaskBarIcon->showMessage(u8"Wine YuanShen FPS Cracker is hidden"
			, u8"Go to system tray icon to operate this application", QIcon("icons/256x256.ico"));
	}
}

void WineYuanShenFpsCracker::closeEvent(QCloseEvent* event)
{
	if (!isActionExit)
	{
		event->ignore();
		hide();
	}
}

void WineYuanShenFpsCracker::showEvent(QShowEvent* event)
{
	pTaskBarIcon->setVisible(false);
	pTaskBarIcon->hide();
}

void WineYuanShenFpsCracker::StartBtnHandler()
{
	ui.startBtn->setEnabled(false);
	ui.pidBox->setEnabled(false);
	ui.fpsBox->setEnabled(false);

	bool pidOk = true, fpsOk = true;
	backgroundThreadInfo.exitCtrl = 0;
	backgroundThreadInfo.pid = ui.pidBox->text().toUInt(&pidOk);
	backgroundThreadInfo.fps = ui.fpsBox->text().toUInt(&pidOk);

	//检查用户输入的pid和fps是否正确
	if (pidOk && fpsOk && backgroundThreadInfo.fps > 0)
	{
		//启动写内存线程
		threadHandle = _beginthreadex(NULL, 0, CrackThreadEntry, &backgroundThreadInfo, 0, NULL);
		if (threadHandle == NULL)
		{
			QMessageBox::critical(this, u8"Crack Error:", u8"start crack thread error!", QMessageBox::StandardButton::Ok);
			ui.startBtn->setEnabled(true);
			ui.pidBox->setEnabled(true);
			ui.fpsBox->setEnabled(true);
		}
		else
		{
			timerIndex = startTimer(100);
			ui.stopBtn->setEnabled(true);
			hide();
		}
	}
	else
	{
		QMessageBox::critical(this, u8"Input Error:", u8"PID or FPS input error!", QMessageBox::StandardButton::Ok);
		ui.startBtn->setEnabled(true);
		ui.pidBox->setEnabled(true);
		ui.fpsBox->setEnabled(true);
	}
}

void WineYuanShenFpsCracker::ShowHandler()
{
	showNormal();
}
void WineYuanShenFpsCracker::ExitHandler()
{
	isActionExit = true;
	close();
}
