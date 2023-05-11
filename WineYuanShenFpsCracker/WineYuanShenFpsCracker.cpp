#include "WineYuanShenFpsCracker.h"

//通过内存搜索获取fps数值和垂直同步数值的内存地址
//搜索步骤github上的，不解释
BOOL GetUnityFPSLimitCodeAndVsyncAddress(HANDLE hProcess, SIZE_T start, SIZE_T size, PBYTE* pCode, PBYTE* pVSync)
{
	if (pCode != NULL && pVSync != NULL)
	{
		*pCode = NULL;
		*pVSync = NULL;
		const BYTE match1[4] = { 0x7F,0x0F,0x8B,0x05 };
		const BYTE match2[10] = { 0xE8,0xff,0xff,0xff,0xff,0x8B,0xE8,0x49,0x8B,0x1E };
		PBYTE buffer = (PBYTE)malloc(size);
		const SIZE_T match1Size = sizeof match1 / sizeof match1[0];
		const SIZE_T match2Size = sizeof match2 / sizeof match2[0];
		BYTE* pAddress = (BYTE*)start;
		SIZE_T memSizeProcessed = 0;
		SIZE_T offset = 0;
		if (buffer != NULL)
		{
			ReadProcessMemory(hProcess, pAddress, buffer, size, &memSizeProcessed);
			if (memSizeProcessed == size)
			{
				while (offset + match1Size <= size)
				{
					if (!memcmp(&buffer[offset], match1, match1Size))
					{
						DWORD ripOffset = *((DWORD*)(buffer + offset + 4));
						*pCode = (PBYTE)(ripOffset + start + offset + 8);
					}
					++offset;
				}
				offset = 0;
				while (offset + match2Size <= size)
				{
					if (!memcmp(&buffer[offset + 5], &match2[5], 5) && buffer[offset] == match2[0])
					{
						PBYTE pVsyncCode = (PBYTE)&buffer[offset];
						INT firstOffset = *((PINT)(&pVsyncCode[1]));
						pVsyncCode = pVsyncCode + firstOffset + 5;
						DWORD secondOffset = *((PDWORD)(pVsyncCode + 3));
						PBYTE ppVsync = secondOffset + pVsyncCode + 7;
						SIZE_T pVSyncStart = 0;
						ppVsync -= (SIZE_T)buffer;
						ppVsync += start;
						do
						{
							ReadProcessMemory(hProcess, ppVsync, &pVSyncStart, sizeof pVSyncStart, &memSizeProcessed);
						} while (memSizeProcessed == sizeof pVSyncStart && !pVSyncStart);
						pVsyncCode += 7;
						DWORD pVsyncOffset = *((PDWORD)(pVsyncCode + 2));
						*pVSync = (PBYTE)(pVsyncOffset + pVSyncStart);
					}
					++offset;
				}
			}
			free(buffer);
			if (*pCode != NULL && *pVSync != NULL)
				return TRUE;
		}
	}
	return FALSE;
}
//通过 ToolHelp32 API 获取 UnityPlayer.dll 的 基地址 和 大小 用于内存搜索
BOOL GetModuleStartAndSize(DWORD dwPid, LPCWSTR szwModuleName, PSIZE_T pStart, PSIZE_T pSize)
{
	if (pStart != NULL && pSize != NULL)
	{
		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwPid);
		if (hSnapshot != INVALID_HANDLE_VALUE)
		{
			MODULEENTRY32W info = {};
			info.dwSize = sizeof info;
			Module32FirstW(hSnapshot, &info);
			do
			{
				if (!wcscmp(info.szModule, szwModuleName))
				{
					*pStart = (SIZE_T)info.modBaseAddr;
					*pSize = info.modBaseSize;
					return TRUE;
				}
				Module32NextW(hSnapshot, &info);
			} while (GetLastError() == ERROR_SUCCESS);
			CloseHandle(hSnapshot);
		}
	}
	return FALSE;
}

//线程入口（这个线程用于写原神内存，破解fps限制）
unsigned __stdcall CrackThreadEntry(void* param)
{
	PROCESS_INFORMATION pi = {};
	//HANDLE hProcess = NULL;
	STARTUPINFOW si = {};
	si.cb = sizeof si;
	DWORD dwExitCode = 0;
	WineYuanShenFpsCracker::BackgroundThreadInfo* pInfo = (WineYuanShenFpsCracker::BackgroundThreadInfo*)param;
	SIZE_T searchStart = 0, searchSize = 0;
	PBYTE pFpsLimitAddress, pVsyncAddress;
	DWORD temp;
	SIZE_T memSizeProcessed = 0;
	INT vsync = 0;
	do
	{
		dwExitCode = 1;
		pi.hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pInfo->pid);
		if (pi.hProcess == NULL)
			break;
		dwExitCode = 2;
		if (!GetModuleStartAndSize(GetProcessId(pi.hProcess), L"UnityPlayer.dll", &searchStart, &searchSize))
			break;
		dwExitCode = 3;
		if (!GetUnityFPSLimitCodeAndVsyncAddress(pi.hProcess, searchStart, searchSize, &pFpsLimitAddress, &pVsyncAddress))
			break;
		dwExitCode = 4;
		if (!VirtualProtectEx(pi.hProcess, (PVOID)searchStart, sizeof searchSize, PAGE_READWRITE, &temp))
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

	bool pidOk = false, fpsOk = false;
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
