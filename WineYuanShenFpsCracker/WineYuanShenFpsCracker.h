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
    //�̲߳������߳̾�������ڳ���д��wineԭ���ڴ棩
    BackgroundThreadInfo backgroundThreadInfo{};
    uintptr_t threadHandle{ NULL };
    //��ʱ�������ڼ���߳�״̬��
    int timerIndex{ -1 };
    //������ͼ���ʹ�õĲ˵�
    QSystemTrayIcon* pTaskBarIcon{ NULL };
    QMenu* pMenu{ NULL };
    //�����Ƿ������˳���Ĭ�ϵ��˳������أ�
    bool isActionExit = false;
    //�����壨ֻ����Ӧ�ÿ�һ��ʵ����
    HANDLE hMutex = NULL;

    void timerEvent(QTimerEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    //������ʾ�����ػص������ڿ���������ͼ�꣩
    void hideEvent(QHideEvent* event) override;
    void showEvent(QShowEvent* event) override;
private slots:
    //������ͼ����ʾ�˵��ص�
    void ShowHandler();
    //�������˳���ʾ�˵��ص�
    void ExitHandler();
    //�����̰߳�ť�ص�
    void StartBtnHandler();
    //ֹͣ�̻߳ص�
    void StopBtnHandler();
};
