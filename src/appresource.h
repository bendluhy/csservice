#ifndef APPRESOURCE_H
#define APPRESOURCE_H

#include <QObject>

#define APP_ORGANIZATION_NAME   "Patrol PC"
#define APP_NAME                "Control Screens"
#define APP_ORGANIZATION_DOMAIN "patrolpc.com"

class AppResource : public QObject
{
    Q_OBJECT
public:
    static AppResource* getInstance()
    {
        static AppResource instance;
        return &instance;
    };
    ~AppResource(){};

    QString getInstallFolder(void);
signals:

private:
    explicit AppResource(QObject *parent = nullptr);
    QString m_InstallFolder;

};

#endif // APPRESOURCE_H
