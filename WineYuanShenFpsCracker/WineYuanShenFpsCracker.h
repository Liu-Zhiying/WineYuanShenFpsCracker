#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_WineYuanShenFpsCracker.h"
#include <stdint.h>
#include <process.h>
#include <qmessagebox.h>
#include <Windows.h>
#include <cstdlib>
#include <TlHelp32.h>
#include <qsystemtrayicon.h>
#include <qstyle.h>
#include <qmenu.h>
#include <qevent.h>

class WineYuanShenFpsCracker : public QMainWindow
{
    Q_OBJECT

public:
    WineYuanShenFpsCracker(QWidget* parent = nullptr);
    ~WineYuanShenFpsCracker();
    typedef struct
    {
        LONG pid;
        LONG fps;
        volatile LONG exitCtrl;
    } BackgroundThreadInfo;
private:
    Ui::WineYuanShenFpsCrackerClass ui;
    //线程参数和线程句柄（用于持续写入wine原神内存）
    BackgroundThreadInfo backgroundThreadInfo{};
    uintptr_t threadHandle{ NULL };
    //定时器（用于检查线程状态）
    int timerIndex{ -1 };
    //任务栏图标和使用的菜单
    QSystemTrayIcon* pTaskBarIcon{ NULL };
    QMenu* pMenu{ NULL };
    //设置是否真正退出（默认的退出变隐藏）
    bool isActionExit = false;
    //互斥体（只允许应用开一个实例）
    HANDLE hMutex = NULL;

    void timerEvent(QTimerEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    //窗口显示和隐藏回调（用于控制任务栏图标）
    void hideEvent(QHideEvent* event) override;
    void showEvent(QShowEvent* event) override;
private slots:
    //任务栏图标显示菜单回调
    void ShowHandler();
    //任务栏退出显示菜单回调
    void ExitHandler();
    //启动线程按钮回调
    void StartBtnHandler();
    //停止线程回调
    void StopBtnHandler();
};
