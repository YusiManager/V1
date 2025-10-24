#pragma once
#include <QtCore>

struct Profile {
    QString group;
    QString name;
    QString host;
    QString user;
    QString port;
    QString keyfile;
    QString remotedir;
    QString homedir;
    QString path;
};

QString trimAll(const QString &s);
QString expandPath(QString p);
QString normalizeHostForSSH(QString h);
Profile parseProfileFile(const QString &filePath);
