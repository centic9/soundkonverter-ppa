
#ifndef SOUNDKONVERTER_REPLAYGAIN_WVGAIN_H
#define SOUNDKONVERTER_REPLAYGAIN_WVGAIN_H

#include "../../core/replaygainplugin.h"

#include <KUrl>

class ConversionOptions;


class soundkonverter_replaygain_wvgain : public ReplayGainPlugin
{
    Q_OBJECT
public:
    /** Default Constructor */
    soundkonverter_replaygain_wvgain( QObject *parent, const QStringList& args );

    /** Default Destructor */
    virtual ~soundkonverter_replaygain_wvgain();

    QString name();

//     QMap<QString,int> codecList();
    QList<ReplayGainPipe> codecTable();
    BackendPlugin::FormatInfo formatInfo( const QString& codecName );
//     bool canApply( const KUrl& filename );
    bool isConfigSupported( ActionType action, const QString& codecName );
    void showConfigDialog( ActionType action, const QString& codecName, QWidget *parent );
    bool hasInfo();
    void showInfo( QWidget *parent );

    int apply( const KUrl::List& fileList, ApplyMode mode = Add );
    float parseOutput( const QString& output );
//     QString applyCommand( const KUrl::List& fileList, ApplyMode mode = Add );
};

// K_EXPORT_COMPONENT_FACTORY( soundkonverter_replaygain_wvgain, KGenericFactory<soundkonverter_replaygain_wvgain> );
K_EXPORT_SOUNDKONVERTER_REPLAYGAIN( wvgain, soundkonverter_replaygain_wvgain );


#endif // _SOUNDKONVERTER_REPLAYGAIN_WVGAIN_H_


