/*
 * soundkonverter.h
 *
 * Copyright (C) 2007 Daniel Faust <hessijames@gmail.com>
 */
#ifndef SOUNDKONVERTER_H
#define SOUNDKONVERTER_H


#include <KXmlGuiWindow>
#include <KUrl>
#include <kdeversion.h>

class soundKonverterView;
class KToggleAction;
class KUrl;
class Config;
class Logger;
class LogViewer;
class CDManager;
class ReplayGainScanner;

#if KDE_IS_VERSION(4,4,0)
    class KStatusNotifierItem;
#else
    class KSystemTrayIcon;
#endif


/**
 * This class serves as the main window for soundKonverter.  It handles the
 * menus, toolbars, and status bars.
 *
 * @short Main window class
 * @author Daniel Faust <hessijames@gmail.com>
 * @version 1.0
 */
class soundKonverter : public KXmlGuiWindow
{
    Q_OBJECT
public:
    /** Default Constructor */
    soundKonverter();

    /** Default Destructor */
    ~soundKonverter();

    virtual void saveProperties( KConfigGroup& configGroup );

    void showSystemTray();
    void addConvertFiles( const KUrl::List& urls, const QString& profile, const QString& format, const QString& directory, const QString& notifyCommand );
    void addReplayGainFiles( const KUrl::List& urls );
    bool ripCd( const QString& device, const QString& profile, const QString& format, const QString& directory, const QString& notifyCommand );
    void setAutoClose( bool enabled ) { autoclose = enabled; }
    void startConversion();
    void loadAutosaveFileList();

private slots:
    void showConfigDialog();
    void showLogViewer( const int logId = 0 );
    void showReplayGainScanner();
    void showAboutPlugins();
    void progressChanged( const QString& progress );

    /** The conversion has started */
    void conversionStarted();
    /** The conversion has stopped */
    void conversionStopped( bool failed );

private:
    Config *config;
    Logger *logger;
    CDManager *cdManager;
    QWeakPointer<ReplayGainScanner> replayGainScanner;

    soundKonverterView *m_view;
    LogViewer *logViewer;

    #if KDE_IS_VERSION(4,4,0)
        KStatusNotifierItem *systemTray;
    #else
        KSystemTrayIcon *systemTray;
    #endif

    /// exit soundkonverter after all files have been converted
    bool autoclose;

    void setupActions();

};

#endif // _SOUNDKONVERTER_H_
