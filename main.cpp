#include <QCoreApplication>
#include "WindowsService.h"

int main(int argc, char *argv[])
{
    QString serviceName = "CSService";
    WindowsService service(serviceName);

    if (argc >= 2) {
        QString arg = argv[1];
        if (arg == "--install") {
            if (service.install())
                qDebug() << "Service installed successfully.";
            else
                qDebug() << "Failed to install service.";
            return 0;
        } else if (arg == "--uninstall") {
            if (service.uninstall())
                qDebug() << "Service uninstalled successfully.";
            else
                qDebug() << "Failed to uninstall service.";
            return 0;
        } else if (arg == "--run") {
            service.runAsApp();         // will call QCoreApplication::exec()
            return 0;                   // DON'T call app.exec() here
        }
    }

    // If no arguments, run as a service via SCM
    std::wstring serviceNameW = serviceName.toStdWString();
    SERVICE_TABLE_ENTRY serviceTable[] = {
        { &serviceNameW[0], (LPSERVICE_MAIN_FUNCTION) WindowsService::serviceMain },
        { NULL, NULL }
    };

    if (!StartServiceCtrlDispatcher(serviceTable)) {
        qDebug() << "StartServiceCtrlDispatcher failed. Running as app instead.";

        // Fallback: run as normal app
        service.runAsApp();
        return 0;  // app.exec() is called from runAsApp
    }

    return 0;
}

