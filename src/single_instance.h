#pragma once
#include <QtCore>

QString instanceServerName();
bool notifyRunningInstance(const QString &name, const QByteArray &msg);
