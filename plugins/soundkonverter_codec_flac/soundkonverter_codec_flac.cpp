
#include "flaccodecglobal.h"

#include "soundkonverter_codec_flac.h"
#include "../../core/conversionoptions.h"
#include "flaccodecwidget.h"


soundkonverter_codec_flac::soundkonverter_codec_flac( QObject *parent, const QStringList& args  )
    : CodecPlugin( parent )
{
    binaries["flac"] = "";

    allCodecs += "flac";
    allCodecs += "wav";
}

soundkonverter_codec_flac::~soundkonverter_codec_flac()
{}

QString soundkonverter_codec_flac::name()
{
    return global_plugin_name;
}

QList<ConversionPipeTrunk> soundkonverter_codec_flac::codecTable()
{
    QList<ConversionPipeTrunk> table;
    ConversionPipeTrunk newTrunk;

    newTrunk.codecFrom = "wav";
    newTrunk.codecTo = "flac";
    newTrunk.rating = 100;
    newTrunk.enabled = ( binaries["flac"] != "" );
    newTrunk.problemInfo = i18n("In order to encode flac files, you need to install 'flac'.\nflac should be shipped with your distribution.");
    newTrunk.data.hasInternalReplayGain = true;
    table.append( newTrunk );

    newTrunk.codecFrom = "flac";
    newTrunk.codecTo = "wav";
    newTrunk.rating = 100;
    newTrunk.enabled = ( binaries["flac"] != "" );
    newTrunk.problemInfo = i18n("In order to decode flac files, you need to install 'flac'.\nflac should be shipped with your distribution.");
    newTrunk.data.hasInternalReplayGain = false;
    table.append( newTrunk );

    return table;
}

BackendPlugin::FormatInfo soundkonverter_codec_flac::formatInfo( const QString& codecName )
{
    BackendPlugin::FormatInfo info;
    info.codecName = codecName;

    if( codecName == "flac" )
    {
        info.lossless = true;
        info.description = i18n("Flac is a free and lossless audio codec.\nFor more information see: http://flac.sourceforge.net");
        info.mimeTypes.append( "audio/x-flac" );
        info.mimeTypes.append( "audio/x-flac+ogg" );
        info.mimeTypes.append( "audio/x-oggflac" );
        info.extensions.append( "flac" );
        info.extensions.append( "fla" );
//         info.extensions.append( "ogg" );
    }
    else if( codecName == "wav" )
    {
        info.lossless = true;
        info.description = i18n("Wave won't compress the audio stream.");
        info.mimeTypes.append( "audio/x-wav" );
        info.mimeTypes.append( "audio/wav" );
        info.extensions.append( "wav" );
    }

    return info;
}

bool soundkonverter_codec_flac::isConfigSupported( ActionType action, const QString& codecName )
{
    return false;
}

void soundkonverter_codec_flac::showConfigDialog( ActionType action, const QString& codecName, QWidget *parent )
{}

bool soundkonverter_codec_flac::hasInfo()
{
    return false;
}

void soundkonverter_codec_flac::showInfo( QWidget *parent )
{}

QWidget *soundkonverter_codec_flac::newCodecWidget()
{
    FlacCodecWidget *widget = new FlacCodecWidget();
    if( lastUsedConversionOptions )
    {
        widget->setCurrentConversionOptions( lastUsedConversionOptions );
        delete lastUsedConversionOptions;
        lastUsedConversionOptions = 0;
    }
    return qobject_cast<QWidget*>(widget);
}

int soundkonverter_codec_flac::convert( const KUrl& inputFile, const KUrl& outputFile, const QString& inputCodec, const QString& outputCodec, ConversionOptions *_conversionOptions, TagData *tags, bool replayGain )
{
    QStringList command = convertCommand( inputFile, outputFile, inputCodec, outputCodec, _conversionOptions, tags, replayGain );
    if( command.isEmpty() ) return -1;

    CodecPluginItem *newItem = new CodecPluginItem( this );
    newItem->id = lastId++;
    newItem->process = new KProcess( newItem );
    newItem->process->setOutputChannelMode( KProcess::MergedChannels );
    connect( newItem->process, SIGNAL(readyRead()), this, SLOT(processOutput()) );
    connect( newItem->process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(processExit(int,QProcess::ExitStatus)) );

    newItem->process->clearProgram();
    newItem->process->setShellCommand( command.join(" ") );
    newItem->process->start();

    emit log( newItem->id, command.join(" ") );

    backendItems.append( newItem );
    return newItem->id;
}

QStringList soundkonverter_codec_flac::convertCommand( const KUrl& inputFile, const KUrl& outputFile, const QString& inputCodec, const QString& outputCodec, ConversionOptions *_conversionOptions, TagData *tags, bool replayGain )
{
    if( !_conversionOptions ) return QStringList();
    
    QStringList command;
    ConversionOptions *conversionOptions = _conversionOptions;

    if( outputCodec == "flac" )
    {
        command += binaries["flac"];
        command += "-V";
        if( conversionOptions->pluginName == global_plugin_name )
        {
            command += "--compression-level-"+QString::number((int)conversionOptions->compressionLevel);
        }
        if( conversionOptions->replaygain )
        {
            command += "--replay-gain";
        }
        command += "\"" + inputFile.toLocalFile() + "\"";
        command += "-o";
        command += "\"" + outputFile.toLocalFile() + "\"";
    }
    else
    {
        command += binaries["flac"];
        command += "-d";
        command += "\"" + inputFile.toLocalFile() + "\"";
        command += "-o";
        command += "\"" + outputFile.toLocalFile() + "\"";
    }

    return command;
}

float soundkonverter_codec_flac::parseOutput( const QString& output )
{
    // 01-Unknown.wav: 98% complete, ratio=0,479    // encode
    // 01-Unknown.wav: 27% complete                 // decode
  
    QRegExp regEnc("(\\d+)% complete");
    if( output.contains(regEnc) )
    {
        return (float)regEnc.cap(1).toInt();
    }

    return -1;
}


#include "soundkonverter_codec_flac.moc"
