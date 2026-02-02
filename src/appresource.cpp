#include "appresource.h"
#include <QSettings>

AppResource::AppResource(QObject *parent)
    : QObject{parent}
{
}

QString AppResource::getInstallFolder()
{
    if (!m_InstallFolder.isEmpty()) return m_InstallFolder;

    QSettings settings(QSettings::NativeFormat, QSettings::SystemScope, APP_ORGANIZATION_NAME, APP_NAME);

    // Read the Path value
    QVariant value = settings.value("Path");

    if (value.isValid()) {
        m_InstallFolder = value.toString();
        return  m_InstallFolder;
    }
    else
    {
        return QString();
    }
}

