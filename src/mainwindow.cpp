#include "mainwindow.h"
#include "terminal_utils.h"
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <QThread>
#include <QFileDialog>
#include <QStandardPaths>
#include <unistd.h>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QDirIterator>
#include <QShortcut>
#include <QHeaderView>
#include <QFileInfo>
#include <QFrame>
#include <QToolButton>
#include <algorithm>

static void addSpanForHeaderRow(QTableView *view, int row) {
    if (!view) return;
    view->setSpan(row, 1, 1, view->model()->columnCount() - 1);
}

MainWindow::MainWindow(QString sitesDir, QWidget *parent)
    : QMainWindow(parent), m_sitesDir(std::move(sitesDir))
{
    resize(920, 560);
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    auto *top = new QHBoxLayout;
    auto *dirLabel = new QLabel("Directory:");
    m_sitesEdit = new QLineEdit(m_sitesDir);
    m_sitesEdit->setClearButtonEnabled(true);
    m_sitesEdit->setPlaceholderText("Path to your servers directory…");

    auto *browseBtn = new QToolButton;
    browseBtn->setText("Browse…");
    browseBtn->setIcon(QIcon::fromTheme("folder-open"));
    browseBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    browseBtn->setAutoRaise(true);
    browseBtn->setCursor(Qt::PointingHandCursor);
    browseBtn->setToolTip("Choose a directory that contains your server profiles");

    auto *filterLabel = new QLabel("Filter:");
    m_filterEdit = new QLineEdit;
    m_filterEdit->setClearButtonEnabled(true);
    m_filterEdit->setPlaceholderText("[ Ctrl+F ] …");

    m_openBtn  = new QPushButton("Connect");
    m_openBtn->setIcon(QIcon::fromTheme("network-connect"));
    m_openBtn->setToolTip("Open an SSH session in your selected terminal");

    m_filesBtn = new QPushButton("File Browser");
    m_filesBtn->setIcon(QIcon::fromTheme("folder-remote"));

    top->addWidget(dirLabel);
    top->addWidget(m_sitesEdit, 1);
    top->addWidget(browseBtn);
    top->addSpacing(10);
    top->addWidget(filterLabel);
    top->addWidget(m_filterEdit, 0);
    top->addStretch();
    top->addWidget(m_openBtn);
    top->addWidget(m_filesBtn);

    m_table = new QTableView;
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);

    m_source = new ProfilesModel(this);
    m_filter = new ProfilesFilterProxy(this);
    m_filter->setSourceModel(m_source);
    m_group = new GroupHeaderProxy(this);
    m_group->setSourceModel(m_filter);
    m_table->setModel(m_group);
    connect(m_group, &QAbstractItemModel::modelReset, this, [this]{ updateSpans(); });

    connect(m_table, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos){
        const QModelIndex idx = m_table->indexAt(pos);
        if (!idx.isValid()) return;
        if (idx.data(Qt::UserRole+3).toBool()) return;
        m_table->selectRow(idx.row());
        QMenu menu(this);
        QAction *actSSH = menu.addAction(QIcon::fromTheme("network-connect"), "Connect");
        QAction *actKru = menu.addAction(QIcon::fromTheme("folder-remote"), "File Browser");
        QAction *chosen = menu.exec(m_table->viewport()->mapToGlobal(pos));
        if (!chosen) return;
        if (chosen->text().startsWith("Connect")) openSelected();
        else mountOverSshfs();
    });

    auto *bottom = new QHBoxLayout;

    m_terminalCombo = new QComboBox;
    m_terminalCombo->setEditable(true);
    for (const QString &t : findTerminalCandidates()) if (!t.isEmpty()) m_terminalCombo->addItem(t);
    m_terminalCombo->setCurrentText(detectTerminal());
    int idx = m_terminalCombo->findText("konsole", Qt::MatchFixedString);
    if (idx >= 0) {
        QString konsoleText = m_terminalCombo->itemText(idx);
        m_terminalCombo->removeItem(idx);
        m_terminalCombo->insertItem(0, konsoleText);
        m_terminalCombo->setCurrentIndex(0);
    }

    bottom->addWidget(new QLabel("Terminal:"));
    bottom->addWidget(m_terminalCombo, 1);

    bottom->addStretch();
    m_statusLabel = new QLabel;
    m_statusLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    bottom->addWidget(m_statusLabel);

    auto *sep = new QFrame;
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Sunken);
    sep->setLineWidth(1);
    sep->setMidLineWidth(0);
    bottom->addSpacing(8);
    bottom->addWidget(sep);
    bottom->addSpacing(8);

    m_autostartCheck = new QCheckBox("Start at login");
    m_autostartCheck->setToolTip("Automatically launch YusiManager when you log in.");
    bottom->addWidget(m_autostartCheck);

    layout->addLayout(top);
    layout->addWidget(m_table, 1);
    layout->addLayout(bottom);
    setCentralWidget(central);

    setupTray();

    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &MainWindow::reload);

    connect(browseBtn, &QToolButton::clicked, this, [this]{
        QString dir = QFileDialog::getExistingDirectory(this, "Choose config folder", m_sitesEdit->text());
        if (!dir.isEmpty()) { m_sitesEdit->setText(dir); setSitesDir(dir); }
    });
    connect(m_sitesEdit, &QLineEdit::editingFinished, this, [this]{ setSitesDir(m_sitesEdit->text()); });
    connect(m_openBtn, &QPushButton::clicked, this, &MainWindow::openSelected);
    connect(m_filesBtn, &QPushButton::clicked, this, &MainWindow::mountOverSshfs);

    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex &idx){
        if (!idx.isValid() || idx.data(Qt::UserRole+3).toBool()) return;
        openSelected();
    });

    auto *scEnter  = new QShortcut(QKeySequence(Qt::Key_Return), m_table);
    auto *scReturn = new QShortcut(QKeySequence(Qt::Key_Enter),  m_table);
    connect(scEnter,  &QShortcut::activated, this, &MainWindow::openSelected);
    connect(scReturn, &QShortcut::activated, this, &MainWindow::openSelected);
    auto *scKrusader = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_K), m_table);
    connect(scKrusader, &QShortcut::activated, this, &MainWindow::mountOverSshfs);
    auto *scFilter = new QShortcut(QKeySequence::Find, this);
    connect(scFilter, &QShortcut::activated, this, [this]{ m_filterEdit->setFocus(); m_filterEdit->selectAll(); });

    m_autostartCheck->setChecked(isAutostartEnabled());
    connect(m_autostartCheck, &QCheckBox::toggled, this, [this](bool on){
        if (!setAutostartEnabled(on)) {
            QSignalBlocker b(m_autostartCheck);
            m_autostartCheck->setChecked(isAutostartEnabled());
            QMessageBox::warning(this, "Autostart", on ? "Could not enable autostart." : "Could not disable autostart.");
        }
    });

    connect(m_filterEdit, &QLineEdit::textChanged, this, [this](const QString &s){
        m_filter->setNeedle(s);
        m_group->setShowUngroupedHeader(true);
        updateSpans();
        m_statusLabel->setText(QString("%1 profiles").arg(m_filter->rowCount()));
        if (m_tray) m_tray->setToolTip(QString("YusiManager — %1 profiles").arg(m_filter->rowCount()));
    });

    setSitesDir(m_sitesDir);
}

void MainWindow::bringToFront() {
    if (isHidden() || isMinimized()) showNormal();
    raise();
    activateWindow();
}

void MainWindow::closeEvent(QCloseEvent *e) {
    if (m_tray && m_tray->isVisible()) {
        hide();
        m_tray->showMessage("YusiManager", "Now running in the system tray.", QSystemTrayIcon::Information, 3000);
        e->ignore();
        return;
    }
    QMainWindow::closeEvent(e);
}

void MainWindow::setupTray() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;
    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(qApp->windowIcon());
    m_tray->setToolTip("YusiManager");

    auto *menu = new QMenu(this);
    QAction *actShow = menu->addAction(qApp->windowIcon(), "Open YusiManager");
    menu->addSeparator();
    QAction *actQuit = menu->addAction(QIcon::fromTheme("application-exit"), "Close YusiManager");

    connect(actShow, &QAction::triggered, this, [this]{ bringToFront(); });
    connect(actQuit, &QAction::triggered, qApp, &QApplication::quit);

    m_tray->setContextMenu(menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r){
        if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick) {
            if (isHidden() || isMinimized()) showNormal(); else hide();
        }
    });
    m_tray->show();
}

void MainWindow::addWatchesForSubdirs(const QString &baseDir) {
    for (const QString &p : m_watcher->directories()) m_watcher->removePath(p);
    if (QDir(baseDir).exists()) m_watcher->addPath(baseDir);
    QDir d(baseDir);
    for (const QFileInfo &fi : d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        m_watcher->addPath(fi.absoluteFilePath());
    }
}

void MainWindow::reload() {
    QDir base(m_sitesDir);
    if (!base.exists()) {
        m_source->setProfiles({});
        m_statusLabel->setText("Folder does not exist!");
        return;
    }

    addWatchesForSubdirs(m_sitesDir);

    QVector<Profile> list;
    auto addFilesFromDir = [&](const QDir &dir, const QString &groupLabel) {
        QDir dd(dir);
        dd.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden);
        dd.setSorting(QDir::Name);
        const QStringList files = dd.entryList();
        for (const QString &fn : files) {
            const QString path = dd.absoluteFilePath(fn);
            Profile p = parseProfileFile(path);
            if (p.host.isEmpty() || p.user.isEmpty()) continue;
            p.group = groupLabel;
            list.push_back(p);
        }
    };

    addFilesFromDir(base, QString());
    for (const QFileInfo &sub : base.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        QDir subdir(sub.absoluteFilePath());
        addFilesFromDir(subdir, sub.fileName());
    }

    m_profiles = list;
    m_source->setProfiles(m_profiles);

    bool hasNamed = false;
    for (const Profile &p : m_profiles) if (!p.group.trimmed().isEmpty()) { hasNamed = true; break; }
    m_group->setShowUngroupedHeader(hasNamed);

    m_filter->setNeedle(m_filterEdit->text());
    updateSpans();

    m_statusLabel->setText(QString("%1 profiles").arg(m_filter->rowCount()));
    if (m_tray) m_tray->setToolTip(QString("YusiManager — %1 profiles").arg(m_filter->rowCount()));
}

void MainWindow::updateSpans() {
    m_table->clearSpans();
    for (int r=0;r<m_group->rowCount();++r) {
        QModelIndex idx = m_group->index(r, 0);
        if (idx.data(Qt::UserRole+3).toBool()) addSpanForHeaderRow(m_table, r);
    }
}

void MainWindow::openSelected() {
    auto sel = m_table->selectionModel()->selectedRows();
    if (sel.isEmpty()) return;
    QModelIndex proxyIdx = sel.first();
    if (proxyIdx.data(Qt::UserRole+3).toBool()) return;
    QModelIndex srcIdx = m_source->index(m_filter->mapToSource(m_group->mapToSource(proxyIdx)).row(), 0);
    int r = srcIdx.row();
    if (r < 0 || r >= m_source->rowCount()) return;

    const Profile &p = m_source->at(r);
    ensureAgentAndPreload(p.keyfile);

    QString term = m_terminalCombo->currentText().trimmed();
    if (term.isEmpty() || QStandardPaths::findExecutable(term).isEmpty())
        term = detectTerminal();

    const QStringList sshCmd = buildSshCommand(p);
    const QString title = QFileInfo(p.name).completeBaseName();
    const QStringList finalCmd = wrapInTerminal(term, sshCmd, title, false);

    const bool started = QProcess::startDetached(finalCmd.first(), finalCmd.mid(1));
    if (!started) QMessageBox::warning(this, "Failed to start", "Could not start terminal/SSH. Check terminal name and path.");
}

void MainWindow::setSitesDir(const QString &dir) {
    QString d = dir.trimmed();
    if (d.startsWith("~")) d.replace(0, 1, QDir::homePath());
    d = QFileInfo(d).absoluteFilePath();

    if (m_sitesDir == d) {
        if (!m_sitesDir.isEmpty() && QDir(m_sitesDir).exists()) addWatchesForSubdirs(m_sitesDir);
        reload();
        return;
    }
    if (!m_watcher->directories().isEmpty()) {
        for (const QString &p : m_watcher->directories()) m_watcher->removePath(p);
    }
    m_sitesDir = d;
    if (QDir(m_sitesDir).exists()) addWatchesForSubdirs(m_sitesDir);
    if (isAutostartEnabled()) setAutostartEnabled(true);
    reload();
}

void MainWindow::ensureAgentAndPreload(const QString &) {
    QString uid = QString::number(getuid());
    const QStringList candidates = {
        "/run/user/"+uid+"/gnupg/S.gpg-agent.ssh",
        "/run/user/"+uid+"/keyring/ssh",
        "/run/user/"+uid+"/ssh-agent.socket",
        "/keyring/ssh"
    };
    for (const QString &c : candidates) {
        if (QFile::exists(c)) { qputenv("SSH_AUTH_SOCK", c.toLocal8Bit()); break; }
    }
    qputenv("SSH_ASKPASS_REQUIRE", "never");
    qunsetenv("SSH_ASKPASS");
    qunsetenv("GIT_ASKPASS");
}

bool MainWindow::warmUpAuth(const Profile &p, int maxMillis, bool *timedOutOut) {
    if (timedOutOut) *timedOutOut = false;

    QString host = normalizeHostForSSH(p.host);
    QStringList cmd { "ssh",
                      "-p", p.port,
                      "-o", "IdentitiesOnly=yes",
                      "-o", "ServerAliveInterval=30",
                      "-o", "ServerAliveCountMax=3",
                      "-o", "BatchMode=yes",
                      "-o", "PreferredAuthentications=publickey",
                      "-o", "ConnectionAttempts=1",
                      "-o", "StrictHostKeyChecking=accept-new",
                      "-o", "ConnectTimeout=5" };
    if (!p.keyfile.isEmpty() && p.keyfile.toLower() != "agent") cmd << "-i" << p.keyfile;
    cmd << QString("%1@%2").arg(p.user, host) << "true";

    QString baseText = m_statusLabel->text();
    static const char *sp[] = {"⠋","⠙","⠹","⠸","⠼","⠴","⠦","⠧","⠇","⠏"};
    QElapsedTimer t; t.start();

    QProcess proc;
    proc.setProgram(cmd.first());
    proc.setArguments(cmd.mid(1));
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start();

    int frame = 0;
    while (proc.state() == QProcess::Running && t.elapsed() < maxMillis) {
        m_statusLabel->setText(QString("%1 Waiting for hardware key… %2s").arg(sp[frame%10]).arg((maxMillis - (int)t.elapsed())/1000));
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(80);
        frame++;
    }

    bool timedOut = false;
    if (proc.state() == QProcess::Running) {
        timedOut = true;
        proc.kill();
        proc.waitForFinished(500);
    }

    QProcess list; list.start("ssh-add", {"-L"}); list.waitForFinished(800);
    bool ok = (list.exitStatus() == QProcess::NormalExit && list.exitCode() == 0);

    if (timedOutOut) *timedOutOut = timedOut;
    m_statusLabel->setText(baseText);
    return ok;
}

void MainWindow::mountOverSshfs() {
    auto sel = m_table->selectionModel()->selectedRows();
    if (sel.isEmpty()) {
        QMessageBox::information(this, "No profile", "Select a profile first.");
        return;
    }
    QModelIndex proxyIdx = sel.first();
    if (proxyIdx.data(Qt::UserRole+3).toBool()) return;
    QModelIndex srcIdx = m_source->index(m_filter->mapToSource(m_group->mapToSource(proxyIdx)).row(), 0);
    int r = srcIdx.row();
    if (r < 0 || r >= m_source->rowCount()) return;

    const Profile &p = m_source->at(r);

    ensureAgentAndPreload(p.keyfile);

    bool okPort = false;
    const int port = p.port.toInt(&okPort);
    if (!okPort || port <= 0 || port > 65535) {
        QMessageBox::warning(this, "Invalid port", QString("Port is invalid: %1").arg(p.port));
        return;
    }

    auto hostForUri = [](QString h) {
        h = h.trimmed();
        if (h.startsWith('[') && h.endsWith(']')) h = h.mid(1, h.size()-2);
        return h;
    };
    QString remoteDir = p.remotedir.trimmed();
    if (remoteDir.isEmpty()) remoteDir = "/";
    if (!remoteDir.startsWith('/')) remoteDir.prepend('/');

    QUrl url;
    url.setScheme("fish");
    url.setUserName(p.user);
    url.setHost(hostForUri(p.host));
    url.setPort(port);
    url.setPath(remoteDir);
    const QString fishUrl = url.toString(QUrl::FullyEncoded);

    const bool haveKrusader = !QStandardPaths::findExecutable("krusader").isEmpty();
    if (haveKrusader) {
        QString leftPath = p.homedir.trimmed();
        if (leftPath.isEmpty()) leftPath = QDir::homePath();

        const QStringList args{
            QString("--left=%1").arg(leftPath),
            QString("--right=%1").arg(fishUrl)
        };
        const bool started = QProcess::startDetached("krusader", args);
        if (!started) {
            QMessageBox::warning(this, "Failed to start",
                                 QString("Could not launch Krusader with:\n--left=%1\n--right=%2").arg(leftPath, fishUrl));
        }
        return;
    }

    QString opener;
    const QStringList fmCandidates = { "dolphin", "kioexec" };
    for (const QString &c : fmCandidates) {
        if (!QStandardPaths::findExecutable(c).isEmpty()) { opener = c; break; }
    }
    const bool haveXdgOpen = !QStandardPaths::findExecutable("xdg-open").isEmpty();
    if (opener.isEmpty() && !haveXdgOpen) {
        QMessageBox::warning(this, "No opener found",
                             "No Krusader/Dolphin/kioexec/xdg-open found.");
        return;
    }

    bool started = false;
    if (!opener.isEmpty()) started = QProcess::startDetached(opener, { fishUrl });
    else                   started = QProcess::startDetached("xdg-open", { fishUrl });

    if (!started) {
        QMessageBox::warning(this, "Failed to open",
                             QString("Could not start %1 for %2.").arg(opener.isEmpty() ? "xdg-open" : opener, fishUrl));
        return;
    }

    m_statusLabel->setText(QString("Opened: %1").arg(fishUrl));
    if (m_tray) m_tray->showMessage("YusiManager", QString("Opened: %1").arg(fishUrl), QSystemTrayIcon::Information, 2000);
}

QString MainWindow::autostartDir() const {
    QString cfg = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (cfg.isEmpty()) cfg = QDir::homePath() + "/.config";
    return cfg + "/autostart";
}

QString MainWindow::autostartFilePath() const {
    return autostartDir() + "/YusiManager.desktop";
}

bool MainWindow::isAutostartEnabled() const {
    return QFileInfo::exists(autostartFilePath());
}

QString MainWindow::autostartDesktopContent() const {
    const QString exe = QCoreApplication::applicationFilePath();
    QString execLine;
    if (m_sitesDir.trimmed().isEmpty()) {
        execLine = QString("\"%1\" --minimized").arg(exe);
    } else {
        execLine = QString("\"%1\" \"%2\" --minimized").arg(exe, m_sitesDir);
    }

    QString content;
    content += "[Desktop Entry]\n";
    content += "Type=Application\n";
    content += "Version=1.0\n";
    content += "Name=YusiManager\n";
    content += "Comment=Manage SSH profiles and quick-launch terminals\n";
    content += "TryExec=" + exe + "\n";
    content += "Exec=" + execLine + "\n";
    content += "Icon=" + QCoreApplication::applicationDirPath() + "/yusimanager.svg\n";
    content += "StartupWMClass=YusiManager\n";
    content += "Terminal=false\n";
    content += "X-GNOME-Autostart-enabled=true\n";
    content += "X-GNOME-Autostart-Phase=Application\n";
    content += "X-GNOME-Autostart-Delay=3\n";
    content += "X-KDE-StartupNotify=false\n";
    content += "OnlyShowIn=GNOME;KDE;XFCE;LXQt;LXDE;MATE;Cinnamon;Unity;\n";
    return content;
}

bool MainWindow::setAutostartEnabled(bool on) {
    const QString dir = autostartDir();
    const QString filePath = autostartFilePath();

    if (on) {
        QDir d;
        if (!d.mkpath(dir)) return false;

        QFile f(filePath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            return false;

        QByteArray data = autostartDesktopContent().toUtf8();
        if (f.write(data) != data.size()) {
            f.close();
            return false;
        }
        f.close();
        return true;
    } else {
        if (!QFileInfo::exists(filePath)) return true;
        return QFile::remove(filePath);
    }
}
