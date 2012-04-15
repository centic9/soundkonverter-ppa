
#include "convert.h"
#include "config.h"
#include "logger.h"
#include "filelist.h"
#include "core/conversionoptions.h"
#include "core/codecplugin.h"
#include "outputdirectory.h"
#include "global.h"

#include <KLocale>
#include <KGlobal>
#include <KTemporaryFile>
#include <KStandardDirs>
#include <KMessageBox>
#include <KComponentData>

#include <QFile>
#include <QFileInfo>
#include <QTimer>


ConvertItem::ConvertItem( FileListItem *item )
{
    fileListItem = item;
    fileListItems.clear();
    getTime = convertTime = decodeTime = encodeTime = replaygainTime = bpmTime = 0.0f;
    encodePlugin = 0;
    convertID = -1;
    replaygainID = -1;
    take = 0;
    killed = false;
}

ConvertItem::ConvertItem( QList<FileListItem*> items )
{
    fileListItem = 0;
    fileListItems = items;
    getTime = convertTime = decodeTime = encodeTime = replaygainTime = bpmTime = 0.0f;
    encodePlugin = 0;
    convertID = -1;
    replaygainID = -1;
    take = 0;
    killed = false;
}

ConvertItem::~ConvertItem()
{}

KUrl ConvertItem::generateTempUrl( const QString& trunk, const QString& extension, bool useSharedMemory )
{
    QString tempUrl;
    int i=0;
    do {
        if( useSharedMemory )
        {
            tempUrl = "/dev/shm/" + QString("soundkonverter_temp_%1_%2_%3.%4").arg(trunk).arg(logID).arg(i).arg(extension);
        }
        else
        {
            tempUrl = KStandardDirs::locateLocal( "tmp", QString("soundkonverter_temp_%1_%2_%3.%4").arg(trunk).arg(logID).arg(i).arg(extension) );
        }
        i++;
    } while( QFile::exists(tempUrl) );

    return KUrl(tempUrl);
}

void ConvertItem::updateTimes()
{
    getTime = ( mode & ConvertItem::get ) ? 0.8f : 0.0f;            // TODO file size? connection speed?
    convertTime = ( mode & ConvertItem::convert ) ? 1.4f : 0.0f;    // NOTE either convert OR decode & encode is used --- or only replay gain
    if( fileListItem && fileListItem->track == -1 )
    {
        decodeTime = ( mode & ConvertItem::decode ) ? 0.4f : 0.0f;
        encodeTime = ( mode & ConvertItem::encode ) ? 1.0f : 0.0f;
    }
    else
    {
        decodeTime = ( mode & ConvertItem::decode ) ? 1.0f : 0.0f;  // TODO drive speed?
        encodeTime = ( mode & ConvertItem::encode ) ? 0.4f : 0.0f;
    }
    replaygainTime = ( mode & ConvertItem::replaygain ) ? 0.2f : 0.0f;
    bpmTime = ( mode & ConvertItem::bpm ) ? 0.2f : 0.0f;

    const float sum = getTime + convertTime + decodeTime + encodeTime + replaygainTime + bpmTime;

    float length = 0;
    if( fileListItem )
    {
        length = fileListItem->length;
    }
    else if( fileListItems.count() > 0 )
    {
        for( int i=0; i<fileListItems.count(); i++ )
        {
            length += fileListItems.at(i)->length;
        }
    }
    getTime *= length/sum;
    convertTime *= length/sum;
    decodeTime *= length/sum;
    encodeTime *= length/sum;
    replaygainTime *= length/sum;
    bpmTime *= length/sum;

    finishedTime = 0.0f;
    switch( state )
    {
        case ConvertItem::convert:
            if( mode & ConvertItem::get ) finishedTime += getTime;
            break;
        case ConvertItem::decode:
            if( mode & ConvertItem::get ) finishedTime += getTime;
            break;
        case ConvertItem::encode:
            if( mode & ConvertItem::get ) finishedTime += getTime;
            if( mode & ConvertItem::decode ) finishedTime += decodeTime;
            break;
        case ConvertItem::replaygain:
            if( mode & ConvertItem::get ) finishedTime += getTime;
            if( mode & ConvertItem::convert ) finishedTime += convertTime;
            if( mode & ConvertItem::decode ) finishedTime += decodeTime;
            if( mode & ConvertItem::encode ) finishedTime += encodeTime;
            break;
        case ConvertItem::bpm:
            if( mode & ConvertItem::get ) finishedTime += getTime;
            if( mode & ConvertItem::convert ) finishedTime += convertTime;
            if( mode & ConvertItem::decode ) finishedTime += decodeTime;
            if( mode & ConvertItem::encode ) finishedTime += encodeTime;
            if( mode & ConvertItem::replaygain ) finishedTime += replaygainTime;
            break;
        default:
            break;
    }
}

// =============
// class Convert
// =============

Convert::Convert( Config *_config, FileList *_fileList, Logger *_logger )
    : config( _config ),
    fileList( _fileList ),
    logger( _logger )
{
    connect( fileList, SIGNAL(convertItem(FileListItem*)), this, SLOT(add(FileListItem*)) );
    connect( fileList, SIGNAL(killItem(FileListItem*)), this, SLOT(kill(FileListItem*)) );
    connect( fileList, SIGNAL(replaygainItems(QList<FileListItem*>)), this, SLOT(replaygain(QList<FileListItem*>)) );
    connect( this, SIGNAL(finished(FileListItem*,int)), fileList, SLOT(itemFinished(FileListItem*,int)) );
    connect( this, SIGNAL(replaygainFinished(QList<FileListItem*>,int)), fileList, SLOT(replaygainFinished(QList<FileListItem*>,int)) );
    connect( this, SIGNAL(rippingFinished(const QString&)), fileList, SLOT(rippingFinished(const QString&)) );
    connect( this, SIGNAL(finishedProcess(int,int)), logger, SLOT(processCompleted(int,int)) );

    connect( &updateTimer, SIGNAL(timeout()), this, SLOT(updateProgress()) );

    QList<CodecPlugin*> codecPlugins = config->pluginLoader()->getAllCodecPlugins();
    for( int i=0; i<codecPlugins.size(); i++ )
    {
        connect( codecPlugins.at(i), SIGNAL(jobFinished(int,int)), this, SLOT(pluginProcessFinished(int,int)) );
        connect( codecPlugins.at(i), SIGNAL(log(int,const QString&)), this, SLOT(pluginLog(int,const QString&)) );
    }
    QList<ReplayGainPlugin*> replaygainPlugins = config->pluginLoader()->getAllReplayGainPlugins();
    for( int i=0; i<replaygainPlugins.size(); i++ )
    {
        connect( replaygainPlugins.at(i), SIGNAL(jobFinished(int,int)), this, SLOT(pluginProcessFinished(int,int)) );
        connect( replaygainPlugins.at(i), SIGNAL(log(int,const QString&)), this, SLOT(pluginLog(int,const QString&)) );
    }
    QList<RipperPlugin*> ripperPlugins = config->pluginLoader()->getAllRipperPlugins();
    for( int i=0; i<ripperPlugins.size(); i++ )
    {
        connect( ripperPlugins.at(i), SIGNAL(jobFinished(int,int)), this, SLOT(pluginProcessFinished(int,int)) );
        connect( ripperPlugins.at(i), SIGNAL(log(int,const QString&)), this, SLOT(pluginLog(int,const QString&)) );
    }
}

Convert::~Convert()
{}

void Convert::cleanUp()
{
    // TODO clean up
}

void Convert::get( ConvertItem *item )
{
    if( item->take > 0 )
        remove( item, -1 );

    logger->log( item->logID, i18n("Getting file") );
    item->state = ConvertItem::get;

    item->tempInputUrl = item->generateTempUrl( "download", item->inputUrl.fileName().mid(item->inputUrl.fileName().lastIndexOf(".")+1) );

    if( !updateTimer.isActive() )
        updateTimer.start( config->data.general.updateDelay );

/*    if( item->inputUrl.isLocalFile() && item->tempInputUrl.isLocalFile() )
    {
        item->process.data()->clearProgram();

        *(item->process.data()) << "cp";
        *(item->process.data()) << item->inputUrl.toLocalFile();
        *(item->process.data()) << item->tempInputUrl.toLocalFile();

        logger->log( item->logID, "cp \"" + item->inputUrl.toLocalFile() + "\" \"" + item->tempInputUrl.toLocalFile() + "\"" );

//         item->process.data()->setPriority( config->data.general.priority );
        item->process.data()->start();
    }
    else
    {
        logger->log( item->logID, "copying \"" + item->inputUrl.pathOrUrl() + "\" to \"" + item->tempInputUrl.toLocalFile() + "\"" );

        item->kioCopyJob.data() = KIO::file_copy( item->inputUrl, item->tempInputUrl, 0700 , KIO::HideProgressInfo );
        connect( item->kioCopyJob.data(), SIGNAL(result(KJob*)), this, SLOT(kioJobFinished(KJob*)) );
        connect( item->kioCopyJob.data(), SIGNAL(percent(KJob*,unsigned long)), this, SLOT(kioJobProgress(KJob*,unsigned long)) );
    }*/

    logger->log( item->logID, i18n("Copying \"%1\" to \"%2\"",item->inputUrl.pathOrUrl(),item->tempInputUrl.toLocalFile()) );

    item->kioCopyJob = KIO::file_copy( item->inputUrl, item->tempInputUrl, 0700 , KIO::HideProgressInfo );
    connect( item->kioCopyJob.data(), SIGNAL(result(KJob*)), this, SLOT(kioJobFinished(KJob*)) );
    connect( item->kioCopyJob.data(), SIGNAL(percent(KJob*,unsigned long)), this, SLOT(kioJobProgress(KJob*,unsigned long)) );
}

void Convert::convert( ConvertItem *item )
{
    if( !item )
        return;

    ConversionOptions *conversionOptions = config->conversionOptionsManager()->getConversionOptions( item->fileListItem->conversionOptionsId );
    if( !conversionOptions )
        return;

    KUrl inputUrl;
    if( !item->tempInputUrl.toLocalFile().isEmpty() )
        inputUrl = item->tempInputUrl;
    else
        inputUrl = item->inputUrl;

    if( item->outputUrl.isEmpty() )
    {
        item->outputUrl = ( !item->fileListItem->outputUrl.url().isEmpty() ) ? OutputDirectory::makePath(item->fileListItem->outputUrl) : OutputDirectory::makePath(OutputDirectory::uniqueFileName(OutputDirectory::calcPath(item->fileListItem,config,"",false),usedOutputNames.values()) );
        item->fileListItem->outputUrl = item->outputUrl;
        fileList->updateItem( item->fileListItem );
    }
    usedOutputNames.insert( item->logID, item->outputUrl.toLocalFile() );

    if( item->take > item->conversionPipes.count() - 1 )
    {
        logger->log( item->logID, "\t" + i18n("No more backends left to try :(") );
        remove( item, -1 );
        return;
    }

    if( !updateTimer.isActive() )
        updateTimer.start( config->data.general.updateDelay );

    if( item->conversionPipes.at(item->take).trunks.count() == 1 ) // conversion can be done by one plugin alone
    {
        logger->log( item->logID, i18n("Converting") );
        item->state = ConvertItem::convert;
        item->convertPlugin = item->conversionPipes.at(item->take).trunks.at(0).plugin;
        if( item->convertPlugin->type() == "codec" )
        {
            const bool replaygain = ( item->conversionPipes.at(item->take).trunks.at(0).data.hasInternalReplayGain && item->mode & ConvertItem::replaygain );
            item->convertID = qobject_cast<CodecPlugin*>(item->convertPlugin)->convert( inputUrl, item->outputUrl, item->conversionPipes.at(item->take).trunks.at(0).codecFrom, item->conversionPipes.at(item->take).trunks.at(0).codecTo, conversionOptions, item->fileListItem->tags, replaygain );
        }
        else if( item->convertPlugin->type() == "ripper" )
        {
            item->fileListItem->state = FileListItem::Ripping;
            item->convertID = qobject_cast<RipperPlugin*>(item->convertPlugin)->rip( item->fileListItem->device, item->fileListItem->track, item->fileListItem->tracks, item->outputUrl );
        }
    }
    else if( item->conversionPipes.at(item->take).trunks.count() == 2 ) // conversion needs two plugins
    {
        BackendPlugin *plugin1;
        CodecPlugin *plugin2;
        plugin1 = item->conversionPipes.at(item->take).trunks.at(0).plugin;
        plugin2 = qobject_cast<CodecPlugin*>(item->conversionPipes.at(item->take).trunks.at(1).plugin);

        QStringList command1, command2;
        if( plugin1->type() == "codec" )
        {
            command1 = qobject_cast<CodecPlugin*>(plugin1)->convertCommand( inputUrl, KUrl(), item->conversionPipes.at(item->take).trunks.at(0).codecFrom, item->conversionPipes.at(item->take).trunks.at(0).codecTo, conversionOptions, item->fileListItem->tags );
        }
        else if( plugin1->type() == "ripper" )
        {
            item->fileListItem->state = FileListItem::Ripping;
            command1 = qobject_cast<RipperPlugin*>(plugin1)->ripCommand( item->fileListItem->device, item->fileListItem->track, item->fileListItem->tracks, KUrl() );
        }
        const bool replaygain = ( item->conversionPipes.at(item->take).trunks.at(1).data.hasInternalReplayGain && item->mode & ConvertItem::replaygain );
        command2 = plugin2->convertCommand( KUrl(), item->outputUrl, item->conversionPipes.at(item->take).trunks.at(1).codecFrom, item->conversionPipes.at(item->take).trunks.at(1).codecTo, conversionOptions, item->fileListItem->tags, replaygain );
        if( !command1.isEmpty() && !command2.isEmpty() )
        {
            // both plugins support pipes
            logger->log( item->logID, i18n("Converting") );
            item->state = ConvertItem::convert;
            logger->log( item->logID, "\t" + command1.join(" ") + " | " + command2.join(" ") );
            item->process = new KProcess();
            item->process.data()->setOutputChannelMode( KProcess::MergedChannels );
            connect( item->process.data(), SIGNAL(readyRead()), this, SLOT(processOutput()) );
            connect( item->process.data(), SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(processExit(int,QProcess::ExitStatus)) );
            item->process.data()->clearProgram();
            item->process.data()->setShellCommand( command1.join(" ") + " | " + command2.join(" ") );
            item->process.data()->start();
        }
        else
        {
            // at least on plugins doesn't support pipes
            // decode with plugin1
            logger->log( item->logID, i18n("Decoding") );
            item->state = ConvertItem::decode;
            item->mode = ConvertItem::Mode( item->mode ^ ConvertItem::convert );
            item->mode = ConvertItem::Mode( item->mode | ConvertItem::decode | ConvertItem::encode );
            item->updateTimes();
            item->convertPlugin = plugin1;
            item->encodePlugin = plugin2;
            int estimatedFileSize = 0;
            if( item->fileListItem->state == FileListItem::Ripping && item->fileListItem->tags )
            {
                estimatedFileSize = item->fileListItem->tags->length * 44100*16*2/8/1024/1024;
            }
            else
            {
                QFileInfo fileInfo( inputUrl.toLocalFile() );
                estimatedFileSize = fileInfo.size();
                if( config->pluginLoader()->isCodecLossless(item->fileListItem->codecName) )
                {
                    estimatedFileSize *= 1.4;
                }
                else if( item->fileListItem->codecName == "midi" || item->fileListItem->codecName == "mod" )
                {
                    estimatedFileSize *= 1000;
                }
                else
                {
                    estimatedFileSize *= 10;
                }
                estimatedFileSize /= 1024*1024;
            }
            const bool useSharedMemory = config->data.advanced.useSharedMemoryForTempFiles && estimatedFileSize > 0 && estimatedFileSize < config->data.advanced.maxSizeForSharedMemoryTempFiles;
            item->tempConvertUrl = item->generateTempUrl( "convert", config->pluginLoader()->codecExtensions(item->conversionPipes.at(item->take).trunks.at(0).codecTo).first(), useSharedMemory );
            if( plugin1->type() == "codec" )
            {
                item->convertID = qobject_cast<CodecPlugin*>(plugin1)->convert( inputUrl, item->tempConvertUrl, item->conversionPipes.at(item->take).trunks.at(0).codecFrom, item->conversionPipes.at(item->take).trunks.at(0).codecTo, conversionOptions, item->fileListItem->tags );
            }
            else if( plugin1->type() == "ripper" )
            {
                item->fileListItem->state = FileListItem::Ripping;
                item->convertID = qobject_cast<RipperPlugin*>(plugin1)->rip( item->fileListItem->device, item->fileListItem->track, item->fileListItem->tracks, item->tempConvertUrl );
            }
        }
    }

    if( !item->process.data() )
    {
        if( item->convertID == -1 )
        {
            executeSameStep( item );
        }
        else if( item->convertID == -100 )
        {
            logger->log( item->logID, "\t" + i18n("Conversion failed. At least one of the used backends needs to be configured properly.") );
            remove( item, 100 );
        }
    }
}

void Convert::encode( ConvertItem *item )
{
    // file has been decoded by plugin1 last time this function was executed
    if( !item || !item->encodePlugin )
        return;

    ConversionOptions *conversionOptions = config->conversionOptionsManager()->getConversionOptions( item->fileListItem->conversionOptionsId );
    if( !conversionOptions )
        return;

    logger->log( item->logID, i18n("Encoding") );
    item->state = ConvertItem::encode;
    item->convertPlugin = item->encodePlugin;
    item->encodePlugin = 0;
    const bool replaygain = ( item->conversionPipes.at(item->take).trunks.at(1).data.hasInternalReplayGain && item->mode & ConvertItem::replaygain );
    item->convertID = qobject_cast<CodecPlugin*>(item->convertPlugin)->convert( item->tempConvertUrl, item->outputUrl, item->conversionPipes.at(item->take).trunks.at(1).codecFrom, item->conversionPipes.at(item->take).trunks.at(1).codecTo, conversionOptions, item->fileListItem->tags, replaygain );
}

void Convert::replaygain( ConvertItem *item )
{
    if( !item )
        return;

    logger->log( item->logID, i18n("Applying Replay Gain") );

    if( item->take > item->replaygainPipes.count() - 1 )
    {
        logger->log( item->logID, "\t" + i18n("No more backends left to try :(") );

        if( item->fileListItem )
        {
            remove( item, -1 );
        }
        else
        {
            removeAlbumGainItem( item, -1 );
        }

        return;
    }

    item->state = ConvertItem::replaygain;
    item->replaygainPlugin = item->replaygainPipes.at(item->take).plugin;
    if( item->fileListItem ) // normal conversion item
    {
        item->replaygainID = item->replaygainPlugin->apply( item->outputUrl );
    }
    else // album gain item
    {
        KUrl::List fileList;
        for( int i=0; i<items.count(); i++ )
        {
            fileList.append( items.at(i)->outputUrl );
        }
        item->replaygainID = item->replaygainPlugin->apply( fileList );
    }

    if( !updateTimer.isActive() )
        updateTimer.start( config->data.general.updateDelay );
}

void Convert::writeTags( ConvertItem *item )
{
    if( !item || !item->fileListItem || !item->fileListItem->tags )
        return;

    logger->log( item->logID, i18n("Writing tags") );
//     item->state = ConvertItem::write_tags;
//     item->fileListItem->setText( 0, i18n("Writing tags")+"... 00 %" );

    config->tagEngine()->writeTags( item->outputUrl, item->fileListItem->tags );

    KUrl inputUrl;
    if( !item->tempInputUrl.toLocalFile().isEmpty() )
        inputUrl = item->tempInputUrl;
    else
        inputUrl = item->inputUrl;

    if( !item->fileListItem->tags->coversRead )
    {
        item->fileListItem->tags->covers = config->tagEngine()->readCovers( inputUrl );
        item->fileListItem->tags->coversRead = true;
    }

    const bool success = config->tagEngine()->writeCovers( item->outputUrl, item->fileListItem->tags->covers );
    if( config->data.coverArt.writeCovers == 0 || ( config->data.coverArt.writeCovers == 1 && !success ) )
    {
        config->tagEngine()->writeCoversToDirectory( item->outputUrl.directory(), item->fileListItem->tags->covers );
    }
}

void Convert::put( ConvertItem *item )
{
    logger->log( item->logID, i18n("Moving file to destination") );
    item->state = ConvertItem::put;

}

void Convert::executeUserScript( ConvertItem *item )
{
    logger->log( item->logID, i18n("Running user script") );
    item->state = ConvertItem::execute_userscript;

//     KUrl source( item->fileListItem->options.filePathName );
//     KUrl destination( item->outputFilePathName );
//
//     item->fileListItem->setText( 0, i18n("Running user script")+"... 00 %" );
//
//     item->convertProcess->clearProgram();
//
//     QString userscript = locate( "data", "soundkonverter/userscript.sh" );
//     if( userscript == "" ) executeNextStep( item );
//
//     *(item->convertProcess) << userscript;
//     *(item->convertProcess) << source.path();
//     *(item->convertProcess) << destination.path();
//
//     logger->log( item->logID, userscript + " \"" + source.path() + "\" \"" + destination.path() + "\"" );

// // /*    item->convertProcess->setPriority( config->data.general.priority );
// //     item->convertProcess->start( KProcess::NotifyOnExit, KProcess::AllOutput );*/
}

void Convert::executeNextStep( ConvertItem *item )
{
    logger->log( item->logID, i18n("Executing next step") );

    item->lastTake = item->take;
    item->take = 0;
    item->progress = 0.0f;

    switch( item->state )
    {
        case ConvertItem::get:
            if( item->mode & ConvertItem::convert ) convert(item);
            else remove( item, 0 );
            break;
        case ConvertItem::convert:
            if( item->mode & ConvertItem::replaygain ) replaygain(item);
//             else if( item->mode & ConvertItem::bpm ) bpm(item);
            else remove( item, 0 );
            break;
        case ConvertItem::decode:
            if( item->mode & ConvertItem::encode ) { item->take = item->lastTake; encode(item); }
            else if( item->mode & ConvertItem::replaygain ) replaygain(item);
//             else if( item->mode & ConvertItem::bpm ) bpm(item);
            else remove( item, 0 );
            break;
        case ConvertItem::encode:
            if( item->mode & ConvertItem::replaygain ) replaygain(item);
//             else if( item->mode & ConvertItem::bpm ) bpm(item);
            else remove( item, 0 );
            break;
        case ConvertItem::replaygain:
//             if( item->mode & ConvertItem::bpm ) bpm(item);
            /*else*/ remove( item, 0 );
            break;
        case ConvertItem::bpm:
            remove( item, 0 );
            break;
        default:
            if( item->mode & ConvertItem::get ) get(item);
            else if( item->mode & ConvertItem::convert ) convert(item);
            else if( item->mode & ConvertItem::replaygain ) replaygain(item);
            else remove( item, 0 );
            break;
    }
}

void Convert::executeSameStep( ConvertItem *item )
{
    item->take++;
    item->progress = 0.0f;

    switch( item->state )
    {
        case ConvertItem::get:
            get(item);
            return;
        case ConvertItem::convert:
            convert(item);
            return;
        case ConvertItem::decode:
            convert(item);
            return;
        case ConvertItem::encode: // TODO try next encoder instead of decoding again (not so easy at the moment)
            convert(item);
            return;
        case ConvertItem::replaygain:
            replaygain(item);
            return;
//         case ConvertItem::bpm:
//             bpm(item);
//             return;
        default:
            break;
    }

    remove( item, -1 ); // shouldn't be possible
}

void Convert::kioJobProgress( KJob *job, unsigned long percent )
{
    // search the item list for our item
    for( int i=0; i<items.count(); i++ )
    {
        if( items.at(i)->kioCopyJob.data() == job )
        {
            items.at(i)->progress = (float)percent;
        }
    }
}

void Convert::kioJobFinished( KJob *job )
{
    // search the item list for our item
    for( int i=0; i<items.count(); i++ )
    {
        if( items.at(i)->kioCopyJob.data() == job )
        {
            items.at(i)->kioCopyJob.data()->deleteLater();

            // copy was successful
            if( job->error() == 0 )
            {
                float fileTime;
                switch( items.at(i)->state )
                {
                    case ConvertItem::get: fileTime = items.at(i)->getTime; break;
                    default: fileTime = 0.0f;
                }
                if( items.at(i)->state == ConvertItem::get )
                {
//                     items.at(i)->tempInputUrl = items.at(i)->tempConvertUrl;
//                     items.at(i)->tempConvertUrl = KUrl();
                    if( !items.at(i)->fileListItem->tags )
                    {
                        items.at(i)->fileListItem->tags = config->tagEngine()->readTags( items.at(i)->tempInputUrl );
                        if( items.at(i)->fileListItem->tags )
                        {
                            logger->log( items.at(i)->logID, i18n("Read tags sucessfully") );
                        }
                        else
                        {
                            logger->log( items.at(i)->logID, i18n("Unable to read tags") );
                        }
                    }
                }
                items.at(i)->finishedTime += fileTime;
                executeNextStep( items.at(i) );
            }
            else
            {
                if( QFile::exists(items.at(i)->tempInputUrl.toLocalFile()) )
                {
                    QFile::remove(items.at(i)->tempInputUrl.toLocalFile());
                    logger->log( items.at(i)->logID, i18nc("removing file","Removing: %1",items.at(i)->tempInputUrl.toLocalFile()) );
                }
                if( QFile::exists(items.at(i)->tempInputUrl.toLocalFile()+".part") )
                {
                    QFile::remove(items.at(i)->tempInputUrl.toLocalFile()+".part");
                    logger->log( items.at(i)->logID, i18nc("removing file","Removing: %1",items.at(i)->tempInputUrl.toLocalFile()+".part") );
                }

                if( job->error() == 1 )
                {
                    remove( items.at(i), 1 );
                }
                else
                {
                    logger->log( items.at(i)->logID, i18n("An error accured. Error code: %1 (%2)",job->error(),job->errorString()) );
                    remove( items.at(i), -1 );
                }
            }
        }
    }
}

void Convert::processOutput()
{
    QString output;
    float progress1, progress2;

    for( int i=0; i<items.size(); i++ )
    {
        if( items.at(i)->process.data() == QObject::sender() )
        {
            if( items.at(i)->conversionPipes.at(items.at(i)->take).trunks.count() == 1 )
            {
                BackendPlugin *plugin1;
                plugin1 = items.at(i)->conversionPipes.at(items.at(i)->take).trunks.at(0).plugin;
                output = items.at(i)->process.data()->readAllStandardOutput().data();
                progress1 = plugin1->parseOutput( output );
                if( progress1 > items.at(i)->progress ) items[i]->progress = progress1;
                if( progress1 == -1 && !output.simplified().isEmpty() ) logger->log( items.at(i)->logID, "\t" + output.trimmed() );
            }
            else if( items.at(i)->conversionPipes.at(items.at(i)->take).trunks.count() == 2 )
            {
                // NOTE we can only use the decoder's progress because the encoder encodes a stream (with to the encoder unknown length)
                BackendPlugin *plugin1, *plugin2;
                plugin1 = items.at(i)->conversionPipes.at(items.at(i)->take).trunks.at(0).plugin;
                plugin2 = items.at(i)->conversionPipes.at(items.at(i)->take).trunks.at(1).plugin;
                output = items.at(i)->process.data()->readAllStandardOutput().data();
                progress1 = plugin1->parseOutput( output );
                progress2 = plugin2->parseOutput( output );
                if( progress1 > items.at(i)->progress ) items[i]->progress = progress1;
                if( progress1 == -1 && progress2 == -1 && !output.simplified().isEmpty() ) logger->log( items.at(i)->logID, "\t" + output.trimmed() );
            }

            return;
        }
    }
}

void Convert::processExit( int exitCode, QProcess::ExitStatus exitStatus )
{
    Q_UNUSED(exitStatus)

    if( QObject::sender() == 0 )
    {
        logger->log( 1000, QString("Error: processExit was called from a null sender. Exit code: %1").arg(exitCode) );
        KMessageBox::error( 0, QString("Error: processExit was called from a null sender. Exit code: %1").arg(exitCode) );
        return;
    }

    // search the item list for our item
    for( int i=0; i<items.size(); i++ )
    {
        if( items.at(i)->process.data() == QObject::sender() )
        {
            // FIXME crash discovered here - but no solution yet - maybe fixed by using deleteLater
            items.at(i)->process.data()->deleteLater();

            if( items.at(i)->killed )
            {
                // TODO clean up temp files, pipes, etc.
                remove( items.at(i), 1 );
                return;
            }

            if( exitCode == 0 )
            {
                float fileTime;
                switch( items.at(i)->state )
                {
                    case ConvertItem::convert: fileTime = items.at(i)->convertTime; break;
                    case ConvertItem::decode: fileTime = items.at(i)->decodeTime; break;
                    case ConvertItem::encode: fileTime = items.at(i)->encodeTime; break;
                    case ConvertItem::replaygain: fileTime = items.at(i)->replaygainTime; break;
                    case ConvertItem::bpm: fileTime = items.at(i)->bpmTime; break;
                    default: fileTime = 0.0f;
                }
                items.at(i)->finishedTime += fileTime;
                /*if( items.at(i)->state == ConvertItem::get )
                {
                    items.at(i)->tempInputUrl = items.at(i)->tempConvertUrl;
                    items.at(i)->tempConvertUrl = KUrl();
                }
                else*/ if( items.at(i)->state == ConvertItem::decode && items.at(i)->convertPlugin->type() == "ripper" )
                {
                    items.at(i)->fileListItem->state = FileListItem::Converting;
                    emit rippingFinished( items.at(i)->fileListItem->device );
                }
                if( items.at(i)->conversionPipes.at(items.at(i)->take).trunks.at(0).data.hasInternalReplayGain && items.at(i)->mode & ConvertItem::replaygain )
                {
                    items.at(i)->mode = ConvertItem::Mode( items[i]->mode ^ ConvertItem::replaygain );
                }
                if( items.at(i)->state == ConvertItem::decode )
                {
                    encode( items.at(i) );
                }
                else
                {
                    executeNextStep( items.at(i) );
                }
            }
            else
            {
                if( items.at(i)->state == ConvertItem::convert && QFile::exists(items.at(i)->outputUrl.toLocalFile()) )
                {
                    QFile::remove(items.at(i)->outputUrl.toLocalFile());
                }
                else if( items.at(i)->state == ConvertItem::decode && QFile::exists(items.at(i)->tempConvertUrl.toLocalFile()) )
                {
                    QFile::remove(items.at(i)->tempConvertUrl.toLocalFile());
                }
                else if( items.at(i)->state == ConvertItem::encode )
                {
                    if( QFile::exists(items.at(i)->tempConvertUrl.toLocalFile()) )
                        QFile::remove(items.at(i)->tempConvertUrl.toLocalFile());
                    if( QFile::exists(items.at(i)->outputUrl.toLocalFile()) )
                        QFile::remove(items.at(i)->outputUrl.toLocalFile());
                }
                logger->log( items.at(i)->logID, "\t" + i18n("Conversion failed. Exit code: %1",exitCode) );
                executeSameStep( items.at(i) );
            }
        }
    }
}

void Convert::pluginProcessFinished( int id, int exitCode )
{
    if( QObject::sender() == 0 )
    {
        logger->log( 1000, QString("Error: pluginProcessFinished was called from a null sender. Exit code: %1").arg(exitCode) );
        KMessageBox::error( 0, QString("Error: pluginProcessFinished was called from a null sender. Exit code: %1").arg(exitCode) );
        return;
    }

    for( int i=0; i<items.size(); i++ )
    {
        if( ( items.at(i)->convertPlugin && items.at(i)->convertPlugin == QObject::sender() && items.at(i)->convertID == id ) ||
            ( items.at(i)->replaygainPlugin && items.at(i)->replaygainPlugin == QObject::sender() && items.at(i)->replaygainID == id ) )
        {
            items.at(i)->convertID = -1;
            items.at(i)->replaygainID = -1;

            if( items.at(i)->killed )
            {
                remove( items.at(i), 1 );
                return;
            }

            if( exitCode == 0 )
            {
                float fileTime;
                switch( items.at(i)->state )
                {
                    case ConvertItem::convert: fileTime = items.at(i)->convertTime; break;
                    case ConvertItem::decode: fileTime = items.at(i)->decodeTime; break;
                    case ConvertItem::encode: fileTime = items.at(i)->encodeTime; break;
                    case ConvertItem::replaygain: fileTime = items.at(i)->replaygainTime; break;
                    case ConvertItem::bpm: fileTime = items.at(i)->bpmTime; break;
                    default: fileTime = 0.0f;
                }
                items.at(i)->finishedTime += fileTime;
                if( items.at(i)->state == ConvertItem::decode && items.at(i)->convertPlugin->type() == "ripper" )
                {
                    items.at(i)->fileListItem->state = FileListItem::Converting;
                    emit rippingFinished( items.at(i)->fileListItem->device );
                }
                if( items.at(i)->conversionPipes.count() > items.at(i)->take && items.at(i)->conversionPipes.at(items.at(i)->take).trunks.at(0).data.hasInternalReplayGain && items.at(i)->mode & ConvertItem::replaygain )
                {
                    items.at(i)->mode = ConvertItem::Mode( items.at(i)->mode ^ ConvertItem::replaygain );
                }
                executeNextStep( items.at(i) );
            }
            else
            {
                if( items.at(i)->state == ConvertItem::convert && config->data.general.removeFailedFiles && QFile::exists(items.at(i)->outputUrl.toLocalFile()) )
                {
                    QFile::remove(items.at(i)->outputUrl.toLocalFile());
                }
                else if( items.at(i)->state == ConvertItem::decode && QFile::exists(items.at(i)->tempConvertUrl.toLocalFile()) )
                {
                    QFile::remove(items.at(i)->tempConvertUrl.toLocalFile());
                }
                else if( items.at(i)->state == ConvertItem::encode )
                {
                    if( QFile::exists(items.at(i)->tempConvertUrl.toLocalFile()) )
                        QFile::remove(items.at(i)->tempConvertUrl.toLocalFile());
                    if( config->data.general.removeFailedFiles && QFile::exists(items.at(i)->outputUrl.toLocalFile()) )
                        QFile::remove(items.at(i)->outputUrl.toLocalFile());
                }

                logger->log( items.at(i)->logID, "\t" + i18n("Conversion failed. Exit code: %1",exitCode) );
                executeSameStep( items.at(i) );
            }
        }
    }

    for( int i=0; i<albumGainItems.size(); i++ )
    {
        if( albumGainItems.at(i)->replaygainPlugin && albumGainItems.at(i)->replaygainPlugin == QObject::sender() && albumGainItems.at(i)->replaygainID == id )
        {
            albumGainItems.at(i)->replaygainID = -1;

            if( albumGainItems.at(i)->killed )
            {
                removeAlbumGainItem( albumGainItems.at(i), 1 );
                return;
            }

            if( exitCode == 0 )
            {
                albumGainItems.at(i)->finishedTime += albumGainItems.at(i)->replaygainTime;
                removeAlbumGainItem( albumGainItems.at(i), 0 );
                return;
            }
            else
            {
                logger->log( albumGainItems.at(i)->logID, "\t" + i18n("Calculating Replay Gain failed. Exit code: %1",exitCode) );
                executeSameStep( albumGainItems.at(i) );
            }
        }
    }
}

void Convert::pluginLog( int id, const QString& message )
{
    // log all cached logs that can be logged now
    for( int i=0; i<pluginLogQueue.size(); i++ )
    {
        for( int j=0; j<items.size(); j++ )
        {
            if( ( items.at(j)->convertPlugin && items.at(j)->convertPlugin == pluginLogQueue.at(i).plugin && items.at(j)->convertID == pluginLogQueue.at(i).id ) ||
                ( items.at(j)->replaygainPlugin && items.at(j)->replaygainPlugin == pluginLogQueue.at(i).plugin && items.at(j)->replaygainID == pluginLogQueue.at(i).id ) )
            {
                for( int k=0; k<pluginLogQueue.at(i).messages.size(); k++ )
                {
                    logger->log( items.at(j)->logID, "\t" + pluginLogQueue.at(i).messages.at(k).trimmed().replace("\n","\n\t") );
                }
                pluginLogQueue.removeAt(i);
                i--;
                break;
            }
        }

        for( int j=0; j<albumGainItems.size(); j++ )
        {
            if( albumGainItems.at(j)->replaygainPlugin && albumGainItems.at(j)->replaygainPlugin == pluginLogQueue.at(i).plugin && albumGainItems.at(j)->replaygainID == pluginLogQueue.at(i).id )
            {
                for( int k=0; k<pluginLogQueue.at(i).messages.size(); k++ )
                {
                    logger->log( albumGainItems.at(j)->logID, "\t" + pluginLogQueue.at(i).messages.at(k).trimmed().replace("\n","\n\t") );
                }
                pluginLogQueue.removeAt(i);
                i--;
                break;
            }
        }
    }

    if( message.trimmed().isEmpty() )
        return;

    // search the matching process
    for( int i=0; i<items.size(); i++ )
    {
        if( ( items.at(i)->convertPlugin && items.at(i)->convertPlugin == QObject::sender() && items.at(i)->convertID == id ) ||
            ( items.at(i)->replaygainPlugin && items.at(i)->replaygainPlugin == QObject::sender() && items.at(i)->replaygainID == id ) )
        {
            logger->log( items.at(i)->logID, "\t" + message.trimmed().replace("\n","\n\t") );
            return;
        }
    }

    // search the matching process
    for( int i=0; i<albumGainItems.size(); i++ )
    {
        if( albumGainItems.at(i)->replaygainPlugin && albumGainItems.at(i)->replaygainPlugin == QObject::sender() && albumGainItems.at(i)->replaygainID == id )
        {
            logger->log( albumGainItems.at(i)->logID, "\t" + message.trimmed().replace("\n","\n\t") );
            return;
        }
    }

    // if the process can't be found; add to the log queue
    for( int i=0; i<pluginLogQueue.size(); i++ )
    {
        if( pluginLogQueue.at(i).plugin == QObject::sender() && pluginLogQueue.at(i).id == id )
        {
            pluginLogQueue[i].messages += message;
            return;
        }
    }

    // no existing item in the log queue; create new log queue item
    LogQueue newLog;
    newLog.plugin = qobject_cast<BackendPlugin*>(QObject::sender());
    newLog.id = id;
    newLog.messages = QStringList(message);
    pluginLogQueue += newLog;

//     logger->log( 1000, qobject_cast<BackendPlugin*>(QObject::sender())->name() + ": " + message.trimmed().replace("\n","\n\t") );
}

void Convert::add( FileListItem* item )
{
    KUrl fileName;
    if( item->track >= 0 )
    {
        if( item->tags )
        {
            fileName = KUrl( i18nc("identificator for the logger","CD track %1: %2 - %3",QString().sprintf("%02i",item->tags->track),item->tags->artist,item->tags->title) );
        }
        else // shouldn't be possible
        {
            fileName = KUrl( i18nc("identificator for the logger","CD track %1",item->track) );
        }
    }
    else
    {
        fileName = item->url;
    }
    logger->log( 1000, i18n("Adding new item to conversion list: '%1'",fileName.pathOrUrl()) );

    ConvertItem *newItem = new ConvertItem( item );
    items.append( newItem );

    // register at the logger
    newItem->logID = logger->registerProcess( fileName );
    logger->log( 1000, "\t" + i18n("Got log ID: %1",newItem->logID) );

//     logger->log( newItem->logID, "Mime Type: " + newItem->fileListItem->mimeType );
//     if( newItem->fileListItem->tags ) logger->log( newItem->logID, i18n("Tags successfully read") );
//     else logger->log( newItem->logID, i18n("Reading tags failed") );

    // set some variables to default values
    newItem->mode = (ConvertItem::Mode)0x0000;
    newItem->state = (ConvertItem::Mode)0x0000;

    newItem->inputUrl = item->url;

    // connect moveProcess of our new item with the slots of Convert
//     newItem->process = new KProcess();
//     newItem->process->setOutputChannelMode( KProcess::MergedChannels );
//     connect( newItem->process, SIGNAL(readyRead()), this, SLOT(processOutput()) );
//     connect( newItem->process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(processExit(int,QProcess::ExitStatus)) );

    ConversionOptions *conversionOptions = config->conversionOptionsManager()->getConversionOptions(item->conversionOptionsId);
    if( !conversionOptions )
    {
        logger->log( 1000, "Convert::add(...) no ConversionOptions found" );
        remove( newItem, -1 );
        return;
    }

    if( item->track >= 0 )
    {
        logger->log( newItem->logID, "\t" + i18n("Track number: %1, device: %2",QString::number(item->track),item->device) );
    }

    newItem->conversionPipes = config->pluginLoader()->getConversionPipes( item->codecName, conversionOptions->codecName, conversionOptions->pluginName );

    logger->log( newItem->logID, "\t" + i18n("Possible conversion strategies:") );
    for( int i=0; i<newItem->conversionPipes.size(); i++ )
    {
        QStringList pipe_str;

        for( int j=0; j<newItem->conversionPipes.at(i).trunks.size(); j++ )
        {
            pipe_str += QString("%1 %2 %3 (%4)").arg(newItem->conversionPipes.at(i).trunks.at(j).codecFrom).arg("->").arg(newItem->conversionPipes.at(i).trunks.at(j).codecTo).arg(newItem->conversionPipes.at(i).trunks.at(j).plugin->name());
        }

        logger->log( newItem->logID, "\t\t" + pipe_str.join(", ") );
    }

    logger->log( newItem->logID, i18n("File system type: %1",conversionOptions->outputFilesystem) );

    newItem->mode = ConvertItem::Mode( newItem->mode | ConvertItem::convert );

    if( conversionOptions->replaygain && !config->data.general.waitForAlbumGain )
    {
        newItem->replaygainPipes = config->pluginLoader()->getReplayGainPipes( conversionOptions->codecName );
        newItem->mode = ConvertItem::Mode( newItem->mode | ConvertItem::replaygain );
    }

    if( !newItem->inputUrl.isLocalFile() && item->track == -1 )
        newItem->mode = ConvertItem::Mode( newItem->mode | ConvertItem::get );
//     if( (!newItem->inputUrl.isLocalFile() && item->track == -1) || newItem->inputUrl.url().toAscii() != newItem->inputUrl.url() ) newItem->mode = ConvertItem::Mode( newItem->mode | ConvertItem::get );

    newItem->updateTimes();

    // (visual) feedback
    item->state = FileListItem::Converting;

    newItem->progressedTime.start();

    // and start
    executeNextStep( newItem );
}

void Convert::remove( ConvertItem *item, int state )
{
    // TODO "remove" (re-add) the times to the progress indicator
    //emit uncountTime( item->getTime + item->getCorrectionTime + item->ripTime +
    //                  item->decodeTime + item->encodeTime + item->replaygainTime );

    QString exitMessage;

    QFileInfo outputFileInfo( item->outputUrl.toLocalFile() );
    float fileRatio = outputFileInfo.size();
    if( item->fileListItem->track > 0 )
    {
        fileRatio /= item->fileListItem->length*4*44100;
    }
    else if( !item->fileListItem->local )
    {
        QFileInfo inputFileInfo( item->tempInputUrl.toLocalFile() );
        fileRatio /= inputFileInfo.size();
    }
    else
    {
        QFileInfo inputFileInfo( item->inputUrl.toLocalFile() );
        fileRatio /= inputFileInfo.size();
    }
    ConversionOptions *conversionOptions = config->conversionOptionsManager()->getConversionOptions( item->fileListItem->conversionOptionsId );
    if( fileRatio < 0.01 && outputFileInfo.size() < 100000 && state != 1 && ( !conversionOptions || conversionOptions->codecName != "speex" ) )
    {
        exitMessage = i18n("An error occured, the output file size is less than one percent of the input file size");

        if( state == 0 )
        {
            logger->log( item->logID, i18n("Conversion failed, trying again. Exit code: -2 (%1)",exitMessage) );
            item->take = item->lastTake;
            QFile::remove( item->outputUrl.toLocalFile() );
            executeSameStep( item );
            return;
        }
    }

    if( state == 0 )
        exitMessage = i18nc("Conversion exit status","Normal exit");
    else if( state == 1 )
        exitMessage = i18nc("Conversion exit status","Aborted by the user");
    else if( state == 100 )
        exitMessage = i18nc("Conversion exit status","Backend needs configuration");
    else
        exitMessage = i18nc("Conversion exit status","An error occured");

    if( state == 0 )
    {
        writeTags( item );
        item->fileListItem->url = item->outputUrl;
    }

    if( !item->fileListItem->notifyCommand.isEmpty() && ( !config->data.general.waitForAlbumGain || !conversionOptions->replaygain ) )
    {
        QString command = item->fileListItem->notifyCommand;
//         command.replace( "%u", item->fileListItem->url );
        command.replace( "%i", item->inputUrl.toLocalFile() );
        command.replace( "%o", item->outputUrl.toLocalFile() );
        logger->log( item->logID, i18n("Executing command: \"%1\"",command) );

        QProcess::startDetached( command );
    }

    // remove temp/failed files
    if( QFile::exists(item->tempInputUrl.toLocalFile()) )
    {
        QFile::remove(item->tempInputUrl.toLocalFile());
    }
    if( QFile::exists(item->tempConvertUrl.toLocalFile()) )
    {
        QFile::remove(item->tempConvertUrl.toLocalFile());
    }
    if( state != 0 && config->data.general.removeFailedFiles && QFile::exists(item->outputUrl.toLocalFile()) )
    {
        QFile::remove(item->outputUrl.toLocalFile());
        logger->log( item->logID, i18n("Removing partially converted output file") );
    }

    usedOutputNames.remove( item->logID );

    logger->log( item->logID, i18n("Removing file from conversion list. Exit code %1 (%2)",state,exitMessage) );

    logger->log( item->logID, "\t" + i18n("Conversion time") + ": " + Global::prettyNumber(item->progressedTime.elapsed(),"ms") );
    logger->log( item->logID, "\t" + i18n("Output file size") + ": " + Global::prettyNumber(outputFileInfo.size(),"B") );
    logger->log( item->logID, "\t" + i18n("File size ratio") + ": " + Global::prettyNumber(fileRatio*100,"%") );

    emit timeFinished( item->finishedTime );

    if( item->process.data() )
        item->process.data()->deleteLater();
    if( item->kioCopyJob.data() )
        item->kioCopyJob.data()->deleteLater();

    emit finished( item->fileListItem, state ); // send signal to FileList
    emit finishedProcess( item->logID, state ); // send signal to Logger

    items.removeAll( item );
    delete item;

    if( items.size() == 0 && albumGainItems.size() == 0 )
        updateTimer.stop();
}

void Convert::removeAlbumGainItem( ConvertItem *item, int state )
{
    // TODO "remove" (re-add) the times to the progress indicator
    //emit uncountTime( item->getTime + item->getCorrectionTime + item->ripTime +
    //                  item->decodeTime + item->encodeTime + item->replaygainTime );

    QString exitMessage;

    if( state == 0 )
        exitMessage = i18nc("Conversion exit status","Normal exit");
    else if( state == 1 )
        exitMessage = i18nc("Conversion exit status","Aborted by the user");
    else if( state == 100 )
        exitMessage = i18nc("Conversion exit status","Backend needs configuration");
    else
        exitMessage = i18nc("Conversion exit status","An error occured");

    for( int i=0; i<item->fileListItems.count(); i++ )
    {
        if( !item->fileListItems.at(i)->notifyCommand.isEmpty() )
        {
            QString command = item->fileListItems.at(i)->notifyCommand;
//             command.replace( "%u", item->fileListItems.at(i)->url );
            command.replace( "%i", item->fileListItems.at(i)->url.toLocalFile() );
            command.replace( "%o", item->fileListItems.at(i)->outputUrl.toLocalFile() );
            logger->log( item->logID, i18n("Executing command: \"%1\"",command) );

            QProcess::startDetached( command );
        }
    }

    logger->log( item->logID, i18n("Removing file from conversion list. Exit code %1 (%2)",state,exitMessage) );

    logger->log( item->logID, "\t" + i18n("Conversion time") + ": " + Global::prettyNumber(item->progressedTime.elapsed(),"ms") );

    emit timeFinished( item->finishedTime );

    emit replaygainFinished( item->fileListItems, state ); // send signal to FileList
    emit finishedProcess( item->logID, state );            // send signal to Logger

    albumGainItems.removeAll( item );
    delete item;

    if( items.size() == 0 && albumGainItems.size() == 0 )
        updateTimer.stop();
}

void Convert::kill( FileListItem *item )
{
    for( int i=0; i<items.size(); i++ )
    {
        if( items.at(i)->fileListItem == item )
        {
            items.at(i)->killed = true;

            if( items.at(i)->convertID != -1 && items.at(i)->convertPlugin )
            {
                items.at(i)->convertPlugin->kill( items.at(i)->convertID );
            }
            else if( items.at(i)->replaygainID != -1 && items.at(i)->replaygainPlugin )
            {
                items.at(i)->replaygainPlugin->kill( items.at(i)->replaygainID );
            }
            else if( items.at(i)->process.data() != 0 )
            {
                items.at(i)->process.data()->kill();
            }
            else if( items.at(i)->kioCopyJob.data() != 0 )
            {
                items.at(i)->kioCopyJob.data()->kill( KJob::EmitResult );
            }
        }
    }

//     for( int i=0; i<albumGainItems.size(); i++ )
//     {
//         for( int j=0; j<albumGainItems.at(i)->fileListItems.count(); j++ )
//         {
//             if( albumGainItems.at(i)->fileListItems.at(j) == item && !albumGainItems.at(i)->killed )
//             {
//                 albumGainItems.at(i)->killed = true;
//
//                 if( albumGainItems.at(i)->replaygainID != -1 && albumGainItems.at(i)->replaygainPlugin )
//                 {
//                     albumGainItems.at(i)->replaygainPlugin->kill( albumGainItems.at(i)->replaygainID );
//                 }
//
//                 break;
//             }
//         }
//     }
}

void Convert::replaygain( QList<FileListItem*> items )
{
    if( items.isEmpty() )
        return;

    const QString albumName = items.at(0)->tags ? items.at(0)->tags->album : i18n("Unknown Album");

    logger->log( 1000, i18n("Adding new item to conversion list: '%1'",i18n("Replay Gain for album: %1",albumName)) );

    ConvertItem *newItem = new ConvertItem( items );
    albumGainItems.append( newItem );

    // register at the logger
    newItem->logID = logger->registerProcess( KUrl(i18n("Replay Gain for album: %1",albumName)) );
    logger->log( 1000, "\t" + i18n("Got log ID: %1",newItem->logID) );

    // set some variables to default values
    newItem->mode = ConvertItem::replaygain;
    newItem->state = (ConvertItem::Mode)0x0000;
    newItem->updateTimes();

    ConversionOptions *conversionOptions = config->conversionOptionsManager()->getConversionOptions( items.at(0)->conversionOptionsId );
    newItem->replaygainPipes = config->pluginLoader()->getReplayGainPipes( conversionOptions->codecName );

    newItem->progressedTime.start();

    replaygain( newItem );
}

void Convert::updateProgress()
{
    float time = 0;
    float fileTime;
    float fileProgress;
    QString fileProgressString;

    // trigger flushing of the logger cache
    pluginLog( 0, "" );

    for( int i=0; i<items.size(); i++ )
    {
        if( items.at(i)->convertID != -1 && items.at(i)->convertPlugin )
        {
            fileProgress = items.at(i)->convertPlugin->progress( items.at(i)->convertID );
        }
        else if( items.at(i)->replaygainID != -1 && items.at(i)->replaygainPlugin )
        {
            fileProgress = items.at(i)->replaygainPlugin->progress( items.at(i)->replaygainID );
        }
        else
        {
            fileProgress = items.at(i)->progress;
        }

        if( fileProgress >= 0 )
        {
            fileProgressString = Global::prettyNumber(fileProgress,"%");
        }
        else
        {
            fileProgressString = i18nc("The conversion progress can't be determined","Unknown");
            fileProgress = 0; // make it a valid value so the calculations below work
        }

        switch( items.at(i)->state )
        {
            case ConvertItem::get:
                fileTime = items.at(i)->getTime;
                items.at(i)->fileListItem->setText( 0, i18n("Getting file")+"... "+fileProgressString );
                break;
            case ConvertItem::convert:
                fileTime = items.at(i)->convertTime;
                items.at(i)->fileListItem->setText( 0, i18n("Converting")+"... "+fileProgressString );
                break;
            case ConvertItem::decode:
                fileTime = items.at(i)->decodeTime;
                if( items.at(i)->convertPlugin->type() == "convert" ) items.at(i)->fileListItem->setText( 0, i18n("Decoding")+"... "+fileProgressString );
                else if( items.at(i)->convertPlugin->type() == "ripper" ) items.at(i)->fileListItem->setText( 0, i18n("Ripping")+"... "+fileProgressString );
                break;
            case ConvertItem::encode:
                fileTime = items.at(i)->encodeTime;
                items.at(i)->fileListItem->setText( 0, i18n("Encoding")+"... "+fileProgressString );
                break;
            case ConvertItem::replaygain:
                fileTime = items.at(i)->replaygainTime;
                items.at(i)->fileListItem->setText( 0, i18n("Replay Gain")+"... "+fileProgressString );
                break;
//             case ConvertItem::bpm:
//                 items.at(i)->fileListItem->setText( 0, i18n("Calculating BPM")+"... "+Global::prettyNumber(fileProgress,"%") );
//                 fileTime = items.at(i)->bpmTime;
//                 break;
            default: fileTime = 0.0f;
        }
        time += items.at(i)->finishedTime + fileProgress * fileTime / 100.0f;
        logger->log( items.at(i)->logID, i18n("Progress: %1",fileProgress) );
    }

    for( int i=0; i<albumGainItems.size(); i++ )
    {
        if( albumGainItems.at(i)->replaygainID != -1 && albumGainItems.at(i)->replaygainPlugin )
        {
            fileProgress = albumGainItems.at(i)->replaygainPlugin->progress( albumGainItems.at(i)->replaygainID );
        }
        else
        {
            fileProgress = albumGainItems.at(i)->progress;
        }

        if( fileProgress >= 0 )
        {
            fileProgressString = Global::prettyNumber(fileProgress,"%");
        }
        else
        {
            fileProgressString = i18nc("The conversion progress can't be determined","Unknown");
            fileProgress = 0; // make it a valid value so the calculations below work
        }

        for( int j=0; j<albumGainItems.at(i)->fileListItems.count(); j++ )
        {
            albumGainItems.at(i)->fileListItems[j]->setText( 0, i18n("Replay Gain")+"... "+fileProgressString );
        }

        time += albumGainItems.at(i)->finishedTime + fileProgress * albumGainItems.at(i)->replaygainTime / 100.0f;
        logger->log( albumGainItems.at(i)->logID, i18n("Progress: %1",fileProgress) );
    }

    emit updateTime( time );
}

