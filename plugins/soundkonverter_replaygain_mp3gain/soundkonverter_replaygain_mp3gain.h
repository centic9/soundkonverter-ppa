
#ifndef SOUNDKONVERTER_REPLAYGAIN_MP3GAIN_H
#define SOUNDKONVERTER_REPLAYGAIN_MP3GAIN_H

#include "../../core/replaygainplugin.h"

#include <KUrl>
#include <QWeakPointer>

class ConversionOptions;
class KDialog;
class QComboBox;
class QCheckBox;


class Mp3GainPluginItem : public ReplayGainPluginItem
{
    Q_OBJECT
public:
    Mp3GainPluginItem( QObject *parent=0 );
    ~Mp3GainPluginItem();

    KUrl::List undoFileList;
};


class soundkonverter_replaygain_mp3gain : public ReplayGainPlugin
{
    Q_OBJECT
public:
    /** Default Constructor */
    soundkonverter_replaygain_mp3gain( QObject *parent, const QStringList& args );

    /** Default Destructor */
    ~soundkonverter_replaygain_mp3gain();

    QString name();

    QList<ReplayGainPipe> codecTable();

    bool isConfigSupported( ActionType action, const QString& codecName );
    void showConfigDialog( ActionType action, const QString& codecName, QWidget *parent );
    bool hasInfo();
    void showInfo( QWidget *parent );

    unsigned int apply( const KUrl::List& fileList, ApplyMode mode = Add );
    float parseOutput( const QString& output );

private:
    QWeakPointer<KDialog> configDialog;
    QComboBox *configDialogTagModeComboBox;
    QCheckBox *configDialogModifyAudioStreamCheckBox;

    int tagMode;
    bool modifyAudioStream;

private slots:
    /** The undo process has exited */
    virtual void undoProcessExit( int exitCode, QProcess::ExitStatus exitStatus );

    void configDialogSave();
    void configDialogDefault();

};

K_EXPORT_SOUNDKONVERTER_REPLAYGAIN( mp3gain, soundkonverter_replaygain_mp3gain )


#endif // _SOUNDKONVERTER_REPLAYGAIN_MP3GAIN_H_


