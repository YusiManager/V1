#include "profile.h"
#include <QRegularExpression>

QString trimAll(const QString &s) {
    QString t = s;
    if (!t.isEmpty() && t.at(0) == QChar::ObjectReplacementCharacter) {}
    t.replace("\r", "");
    return t.trimmed();
}

QString expandPath(QString p) {
    p = trimAll(p);
    if (p.startsWith("~")) p.replace(0, 1, QDir::homePath());
    return p;
}

QString normalizeHostForSSH(QString h) {
    if (h.startsWith('[') && h.endsWith(']')) h = h.mid(1, h.size() - 2);
    return h;
}

Profile parseProfileFile(const QString &filePath) {
    QFile f(filePath);
    Profile p;
    p.path = filePath;
    p.name = QFileInfo(filePath).fileName();

    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return p;

    QString host, user, port, keyfile, remotedir, homedir;
    QTextStream ts(&f);
    while (!ts.atEnd()) {
        QString raw = ts.readLine();
        QString line = trimAll(raw);
        if (line.isEmpty() || line.startsWith('#')) continue;
        static const QRegularExpression kv("^([^:=]+)\\s*[:=]\\s*(.*)$");
        auto m = kv.match(line);
        if (!m.hasMatch()) continue;
        QString key = m.captured(1).toLower().replace(" ", "");
        QString val = trimAll(m.captured(2));

        if (key == "host" || key == "hostname") {
            static const QRegularExpression reIpv6Port("^\\[.*\\]:[0-9]+$");
            if (reIpv6Port.match(val).hasMatch()) {
                int colon = val.lastIndexOf(':');
                host = val.left(colon);
                port = val.mid(colon + 1);
            } else if (val.contains(':')) {
                QString maybePort = val.section(':', -1);
                if (QRegularExpression("^[0-9]+$").match(maybePort).hasMatch()) {
                    host = val.section(":", 0, -2);
                    if (port.isEmpty()) port = maybePort;
                } else {
                    host = val;
                }
            } else {
                host = val;
            }
        } else if (key == "user" || key == "username" || key == "login") {
            user = val;
        } else if (key == "port") {
            if (QRegularExpression("^[0-9]+$").match(val).hasMatch()) {
                port = val;
            } else {
                if (user.isEmpty() && !val.isEmpty()) user = val;
                port.clear();
            }
        } else if (key == "key" || key == "keyfile" || key == "identityfile") {
            keyfile = val;
        } else if (key == "remotedir" || key == "remote" || key == "path") {
            remotedir = val;
        } else if (key == "homedir" || key == "local" || key == "left") {
            homedir = expandPath(val);
        }
    }

    if (port.isEmpty()) port = "22";
    if (!keyfile.isEmpty() && keyfile.toLower() != "agent") keyfile = expandPath(keyfile);
    if (remotedir.isEmpty()) remotedir = "/";
    if (homedir.isEmpty()) homedir = QDir::homePath();

    p.host = host;
    p.user = user;
    p.port = port;
    p.keyfile = keyfile;
    p.remotedir = remotedir;
    p.homedir = homedir;

    return p;
}
