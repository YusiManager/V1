#pragma once
#include <QtWidgets>
#include <QFileSystemWatcher>
#include "profile.h"
#include "profiles_model.h"
#include "filter_proxy.h"
#include "group_header_proxy.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QString sitesDir, QWidget *parent=nullptr);
    void bringToFront();

protected:
    void closeEvent(QCloseEvent *e) override;

private:
    void setupTray();
    void reload();
    void mountOverSshfs();
    void openSelected();
    void setSitesDir(const QString &dir);

    void ensureAgentAndPreload(const QString &keyfile);
    bool warmUpAuth(const Profile &p, int maxMillis = 15000, bool *timedOutOut = nullptr);

    void addWatchesForSubdirs(const QString &baseDir);
    QString autostartDir() const;
    QString autostartFilePath() const;
    bool isAutostartEnabled() const;
    bool setAutostartEnabled(bool on);
    QString autostartDesktopContent() const;

    QString ensureIconDeployed() const;

    QLineEdit *m_sitesEdit{};
    QLineEdit *m_filterEdit{};
    QTableView *m_table{};
    QLabel *m_statusLabel{};
    QPushButton *m_openBtn{};
    QComboBox *m_terminalCombo{};
    QPushButton *m_filesBtn{};
    QCheckBox *m_autostartCheck{};
    QString m_sitesDir;
    QFileSystemWatcher *m_watcher{};
    QVector<Profile> m_profiles;
    QSystemTrayIcon *m_tray{};

    ProfilesModel *m_source{};
    ProfilesFilterProxy *m_filter{};
    GroupHeaderProxy *m_group{};

    void updateSpans();
};
