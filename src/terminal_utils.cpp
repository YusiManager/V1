#include "terminal_utils.h"
#include <QStandardPaths>

QStringList findTerminalCandidates() {
    return {
        qEnvironmentVariable("TERMINAL"),
        "gnome-terminal", "konsole", "xfce4-terminal", "kitty", "alacritty",
        "tilix", "mate-terminal", "xterm", "lxterminal", "terminator", "urxvt", "st"
    };
}

QString detectTerminal() {
    for (const QString &t : findTerminalCandidates()) {
        if (t.isEmpty()) continue;
        if (!QStandardPaths::findExecutable(t).isEmpty()) return t;
    }
    return {};
}

QStringList buildSshCommand(const Profile &p) {
    QString host = normalizeHostForSSH(p.host);
    QStringList cmd{ "ssh", "-p", p.port, "-o", "IdentitiesOnly=yes",
                     "-o", "ServerAliveInterval=30", "-o", "ServerAliveCountMax=3" };
    if (!p.keyfile.isEmpty() && p.keyfile.toLower() != "agent") cmd << "-i" << p.keyfile;
    cmd << QString("%1@%2").arg(p.user, host);
    return cmd;
}

QString shellQuoteArg(const QString &a) {
    QString q = a;
    q.replace("'", "'\\''");
    return "'" + q + "'";
}

QStringList wrapInTerminal(const QString &terminal,
                           const QStringList &sshCmd,
                           const QString &title,
                           bool hold)
{
    QStringList quoted;
    for (const QString &a : sshCmd) quoted << shellQuoteArg(a);

    const QString holdTail = hold ? "; echo; read -n1 -s -r -p '[enter] om te sluiten'" : "";
    const QString payload = QString("bash -lc %1").arg("'" + quoted.join(" ") + holdTail + "'");

    if (terminal.isEmpty() || QStandardPaths::findExecutable(terminal).isEmpty())
        return sshCmd;

    QStringList args;
    auto addTitle = [&](std::initializer_list<QString> flags){
        if (title.isEmpty()) return;
        for (const QString &f : flags) {
            if (f.contains("%1"))
                args << f.arg(title);
            else
                args << f << title;
        }
    };

    if (terminal == "konsole") {
        addTitle({ "--title=%1" });
        args << "--new-tab" << "-e" << payload;
    } else if (terminal == "gnome-terminal" || terminal == "mate-terminal") {
        addTitle({ "--title=%1" });
        args << "--" << "bash" << "-lc" << quoted.join(" ") + holdTail;
        return QStringList{ terminal } << args;
    } else if (terminal == "xfce4-terminal") {
        addTitle({ "--title=%1" });
        args << "--disable-server" << "--command" << payload;
    } else if (terminal == "kitty" || terminal == "alacritty") {
        addTitle({ "--title" });
        args << "-e" << "bash" << "-lc" << quoted.join(" ") + holdTail;
        return QStringList{ terminal } << args;
    } else if (terminal == "tilix") {
        addTitle({ "--title=%1" });
        args << "-e" << payload;
    } else if (terminal == "xterm" || terminal == "urxvt" || terminal == "st") {
        addTitle({ "-T" });
        args << "-e" << "bash" << "-lc" << quoted.join(" ") + holdTail;
        return QStringList{ terminal } << args;
    } else if (terminal == "lxterminal" || terminal == "terminator") {
        addTitle({ "--title=%1" });
        args << "-e" << payload;
    } else {
        addTitle({ "--title=%1" });
        args << "-e" << "bash" << "-lc" << quoted.join(" ") + holdTail;
        return QStringList{ terminal } << args;
    }

    return QStringList{ terminal } << args;
}
