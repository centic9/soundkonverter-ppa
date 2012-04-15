
#include "filelist.h"
// // #include "filelistitem.h"
#include "config.h"
#include "logger.h"
#include "optionseditor.h"
#include "optionslayer.h"
#include "core/conversionoptions.h"
#include "outputdirectory.h"
#include "codecproblems.h"

#include <KApplication>
#include <KLocale>
#include <KIcon>
#include <KAction>
// #include <kactioncollection.h>
#include <KMessageBox>
#include <KStandardDirs>
// #include <KDiskFreeSpaceInfo>
#include <kmountpoint.h>
// #include <KIO/Job>

#include <QLayout>
#include <QGridLayout>
#include <QStringList>
#include <QMenu>
#include <QFile>
#include <QResizeEvent>
#include <QDir>
#include <QProgressBar>


FileList::FileList( Logger *_logger, Config *_config, QWidget *parent )
    : QTreeWidget( parent ),
    logger( _logger ),
    config( _config )
{
    queue = false;
    optionsEditor = 0;
    tagEngine = config->tagEngine();

    setAcceptDrops( true );
    setDragEnabled( false );

    setItemDelegate( new FileListItemDelegate(this) );

    setColumnCount( 4 );
    QStringList labels;
    labels.append( i18n("State") );
    labels.append( i18n("Input") );
    labels.append( i18n("Output") );
    labels.append( i18n("Quality") );
    setHeaderLabels( labels );
//     header()->setClickEnabled( false );

    setSelectionBehavior( QAbstractItemView::SelectRows );
    setSelectionMode( QAbstractItemView::ExtendedSelection );
    setSortingEnabled( false );

    setRootIsDecorated( false );
    setDragDropMode( QAbstractItemView::InternalMove );

    setMinimumHeight( 200 );

    QGridLayout *grid = new QGridLayout( this );
    grid->setRowStretch( 0, 1 );
    grid->setRowStretch( 2, 1 );
    grid->setColumnStretch( 0, 1 );
    grid->setColumnStretch( 2, 1 );
    pScanStatus = new QProgressBar( this );
    pScanStatus->setMinimumHeight( pScanStatus->height() );
    pScanStatus->setFormat( "%v / %m" );
    pScanStatus->hide();
    grid->addWidget( pScanStatus, 1, 1 );
    grid->setColumnStretch( 1, 2 );

    // we haven't got access to the action collection of soundKonverter, so let's create a new one
//     actionCollection = new KActionCollection( this );

    editAction = new KAction( KIcon("view-list-text"), i18n("Edit options..."), this );
    connect( editAction, SIGNAL(triggered()), this, SLOT(showOptionsEditorDialog()) );
    startAction = new KAction( KIcon("system-run"), i18n("Start conversion"), this );
    connect( startAction, SIGNAL(triggered()), this, SLOT(convertSelectedItems()) );
    stopAction = new KAction( KIcon("process-stop"), i18n("Stop conversion"), this );
    connect( stopAction, SIGNAL(triggered()), this, SLOT(killSelectedItems()) );
    removeAction = new KAction( KIcon("edit-delete"), i18n("Remove"), this );
    removeAction->setShortcut( QKeySequence::Delete );
    connect( removeAction, SIGNAL(triggered()), this, SLOT(removeSelectedItems()) );
    addAction( removeAction );
//     KAction *removeActionGlobal = new KAction( KIcon("edit-delete"), i18n("Remove"), this );
//     removeActionGlobal->setShortcut( QKeySequence::Delete );
//     connect( removeActionGlobal, SIGNAL(triggered()), this, SLOT(removeSelectedItems()) );
//     addAction( removeActionGlobal );
//     paste = new KAction( i18n("Paste"), "editpaste", 0, this, 0, actionCollection, "paste" );  // TODO paste

    contextMenu = new QMenu( this );

    setContextMenuPolicy( Qt::CustomContextMenu );
    connect( this, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showContextMenu(const QPoint&)) );

    connect( this, SIGNAL(itemSelectionChanged()), this, SLOT(itemsSelected()) );
}

FileList::~FileList()
{
    // NOTE no cleanup needed since it all gets cleand up in other classes

    if( !KApplication::kApplication()->sessionSaving() )
    {
        QFile listFile( KStandardDirs::locateLocal("data","soundkonverter/filelist_autosave.xml") );
        listFile.remove();
    }
}

void FileList::dragEnterEvent( QDragEnterEvent *event )
{
    if( event->mimeData()->hasFormat("text/uri-list") )
        event->acceptProposedAction();
}

void FileList::dropEvent( QDropEvent *event )
{
    QList<QUrl> q_urls = event->mimeData()->urls();
    KUrl::List k_urls;
    QStringList errorList;
    //    codec    @0 files @1 solutions
    QMap< QString, QList<QStringList> > problems;
    QString fileName;

    for( int i=0; i<q_urls.size(); i++ )
    {
        QString codecName = config->pluginLoader()->getCodecFromFile( q_urls.at(i) );

        if( codecName == "inode/directory" || config->pluginLoader()->canDecode(codecName,&errorList) )
        {
            k_urls += q_urls.at(i);
        }
        else
        {
            fileName = KUrl(q_urls.at(i)).pathOrUrl();
            if( codecName.isEmpty() )
                codecName = fileName.right(fileName.length()-fileName.lastIndexOf(".")-1);
            if( problems.value(codecName).count() < 2 )
            {
                problems[codecName] += QStringList();
                problems[codecName] += QStringList();
            }
            problems[codecName][0] += fileName;
            if( !errorList.isEmpty() )
            {
                problems[codecName][1] += errorList;
            }
            else
            {
                problems[codecName][1] += i18n("This file type is unknown to soundKonverter.\nMaybe you need to install an additional soundKonverter plugin.\nYou should have a look at your distribution's package manager for this.");
            }
        }
    }

    QList<CodecProblems::Problem> problemList;
    for( int i=0; i<problems.count(); i++ )
    {
        CodecProblems::Problem problem;
        problem.codecName = problems.keys().at(i);
        if( problem.codecName != "wav" )
        {
            #if QT_VERSION >= 0x040500
                problems[problem.codecName][1].removeDuplicates();
            #else
                QStringList found;
                for( int j=0; j<problems.value(problem.codecName).at(1).count(); j++ )
                {
                    if( found.contains(problems.value(problem.codecName).at(1).at(j)) )
                    {
                        problems[problem.codecName][1].removeAt(j);
                        j--;
                    }
                    else
                    {
                        found += problems.value(problem.codecName).at(1).at(j);
                    }
                }
            #endif
            problem.solutions = problems.value(problem.codecName).at(1);
            if( problems.value(problem.codecName).at(0).count() <= 3 )
            {
                problem.affectedFiles = problems.value(problem.codecName).at(0);
            }
            else
            {
                problem.affectedFiles += problems.value(problem.codecName).at(0).at(0);
                problem.affectedFiles += problems.value(problem.codecName).at(0).at(1);
                problem.affectedFiles += i18n("... and %1 more files",problems.value(problem.codecName).at(0).count()-3);
            }
            problemList += problem;
        }
    }

    if( problemList.count() > 0 )
    {
        CodecProblems *problemsDialog = new CodecProblems( CodecProblems::Decode, problemList, this );
        problemsDialog->exec();
    }

    if( k_urls.count() > 0 )
    {
        optionsLayer->addUrls( k_urls );
        optionsLayer->fadeIn();
    }

    event->acceptProposedAction();
}

void FileList::resizeEvent( QResizeEvent *event )
{
    if( event->size().width() < 500 )
        return;

    setColumnWidth( Column_State, 140 );
    setColumnWidth( Column_Input, (event->size().width()-260)/2 );
    setColumnWidth( Column_Output, (event->size().width()-260)/2 );
    setColumnWidth( Column_Quality, 120 );
}

int FileList::listDir( const QString& directory, const QStringList& filter, bool recursive, int conversionOptionsId, bool fast, int count )
{
    QString codecName;

    QDir dir( directory );
    dir.setFilter( QDir::Files | QDir::Dirs | QDir::NoSymLinks | QDir::Readable );

    QStringList list = dir.entryList();

    for( QStringList::Iterator it = list.begin(); it != list.end(); ++it )
    {
        if( *it == "." || *it == ".." )
            continue;

        QFileInfo fileInfo( directory + "/" + *it );

        if( fileInfo.isDir() && recursive )
        {
            count = listDir( directory + "/" + *it, filter, recursive, conversionOptionsId, fast, count );
        }
        else if( !fileInfo.isDir() ) // NOTE checking for isFile may not work with all file names
        {
            count++;

            if( fast )
            {
                pScanStatus->setMaximum( count );
            }
            else
            {
                codecName = config->pluginLoader()->getCodecFromFile( directory + "/" + *it );

                if( filter.count() == 0 || filter.contains(codecName) )
                {
                    addFiles( KUrl(directory + "/" + *it), 0, "", codecName, conversionOptionsId );
                    if( tScanStatus.elapsed() > config->data.general.updateDelay * 10 )
                    {
                        pScanStatus->setValue( count );
                        tScanStatus.start();
                    }
                }
            }
        }
    }

    return count;
}

void FileList::addFiles( const KUrl::List& fileList, ConversionOptions *conversionOptions, const QString& command, const QString& _codecName, int conversionOptionsId, FileListItem *after, bool enabled )
{
    FileListItem *lastListItem;
    if( !after && !enabled )
        lastListItem = topLevelItem( topLevelItemCount()-1 );
    else
        lastListItem = after;

    QString codecName;
    QString filePathName;
    QString device;

    if( !conversionOptions && conversionOptionsId == -1 )
    {
        logger->log( 1000, "@addFiles: No conversion options given" );
        return;
    }

    for( int i=0; i<fileList.count(); i++ )
    {
        QFileInfo fileInfo( fileList.at(i).toLocalFile() );
        if( fileInfo.isDir() )
        {
            addDir( fileList.at(i), true, config->pluginLoader()->formatList(PluginLoader::Decode,PluginLoader::CompressionType(PluginLoader::Lossy|PluginLoader::Lossless|PluginLoader::Hybrid)), conversionOptions );
            continue;
        }

        if( !_codecName.isEmpty() )
        {
            codecName = _codecName;
        }
        else
        {
            codecName = config->pluginLoader()->getCodecFromFile( fileList.at(i) );

            if( !config->pluginLoader()->canDecode(codecName) )
            {
                continue;
            }
        }

        FileListItem *newItem = new FileListItem( this, lastListItem );
        if( conversionOptionsId == -1 )
        {
            if( i == 0 )
            {
                newItem->conversionOptionsId = config->conversionOptionsManager()->addConversionOptions( conversionOptions );
            }
            else
            {
                newItem->conversionOptionsId = config->conversionOptionsManager()->increaseReferences( lastListItem->conversionOptionsId );
            }
        }
        else
        {
            newItem->conversionOptionsId = config->conversionOptionsManager()->increaseReferences( conversionOptionsId );
        }
        lastListItem = newItem;
        newItem->codecName = codecName;
        newItem->track = -1;
        newItem->url = fileList.at(i);
        newItem->local = ( newItem->url.isLocalFile() || newItem->url.protocol() == "file" );
        newItem->tags = tagEngine->readTags( newItem->url );
        if( !newItem->tags && newItem->codecName == "wav" && newItem->local )
        {
            QFile file( newItem->url.toLocalFile() );
            newItem->length = file.size() / 176400; // assuming it's a 44100 Hz, 16 bit wave file
        }
        else
        {
            newItem->length = ( newItem->tags && newItem->tags->length > 0 ) ? newItem->tags->length : 200.0f;
        }
        newItem->notifyCommand = command;
        addTopLevelItem( newItem );
        updateItem( newItem );
        emit timeChanged( newItem->length );
    }

    emit fileCountChanged( topLevelItemCount() );

    if( QObject::sender() == optionsLayer )
        save( false );
}

void FileList::addDir( const KUrl& directory, bool recursive, const QStringList& codecList, ConversionOptions *conversionOptions )
{
    TimeCount = 0;

    if( !conversionOptions )
    {
        logger->log( 1000, "@addDir: No conversion options given" );
        return;
    }

    const int conversionOptionsId = config->conversionOptionsManager()->addConversionOptions( conversionOptions );

    pScanStatus->setValue( 0 );
    pScanStatus->setMaximum( 0 );
    pScanStatus->show(); // show the status while scanning the directories
//     kapp->processEvents();
    tScanStatus.start();

    Time.start();
    listDir( directory.path(), codecList, recursive, conversionOptionsId, true );
    listDir( directory.path(), codecList, recursive, conversionOptionsId );
    TimeCount += Time.elapsed();

    pScanStatus->hide(); // hide the status bar, when the scan is done

    qDebug() << "TimeCount: " << TimeCount;
}

void FileList::addTracks( const QString& device, QList<int> trackList, int tracks, QList<TagData*> tagList, ConversionOptions *conversionOptions )
{
    FileListItem *lastListItem = 0;

    if( !conversionOptions )
    {
        logger->log( 1000, "@addTracks: No conversion options given" );
        return;
    }

    for( int i=0; i<trackList.count(); i++ )
    {
        FileListItem *newItem = new FileListItem( this );
        if( i == 0 )
        {
            newItem->conversionOptionsId = config->conversionOptionsManager()->addConversionOptions( conversionOptions );
        }
        else
        {
            newItem->conversionOptionsId = config->conversionOptionsManager()->increaseReferences( lastListItem->conversionOptionsId );
        }
        lastListItem = newItem;
        newItem->codecName = "audio cd";
//         newItem->notifyCommand = notifyCommand;
        newItem->track = trackList.at(i);
        newItem->tracks = tracks;
        newItem->device = device;
        newItem->tags = tagList.at(i);
        newItem->length = newItem->tags ? newItem->tags->length : 200.0f;
        addTopLevelItem( newItem );
        updateItem( newItem );
        emit timeChanged( newItem->length );
    }

    emit fileCountChanged( topLevelItemCount() );
}

void FileList::updateItem( FileListItem *item )
{
    if( !item )
        return;

    KUrl outputUrl;

    if( !item->outputUrl.toLocalFile().isEmpty() )
    {
        outputUrl = item->outputUrl;
    }
    else
    {
        outputUrl = OutputDirectory::calcPath( item, config );
    }
//     if( QFile::exists(outputUrl.toLocalFile()) )
//     {
//         if( config->data.general.conflictHandling == Config::Data::General::ConflictHandling::NewFileName )
//         {
//             outputUrl = OutputDirectory::uniqueFileName( outputUrl );
//         }
//     }
    item->setText( Column_Output, outputUrl.toLocalFile() );

    switch( item->state )
    {
        case FileListItem::WaitingForConversion:
        {
            if( QFile::exists(outputUrl.toLocalFile()) )
            {
                item->setText( Column_State, i18n("Will be skipped") );
            }
            else
            {
                item->setText( Column_State, i18n("Waiting") );
            }
            break;
        }
        case FileListItem::WaitingForAlbumGain:
        {
            item->setText( Column_State, i18n("Waiting for Replay Gain") );
            break;
        }
        case FileListItem::ApplyingReplayGain:
        {
            item->setText( Column_State, i18n("Replay Gain") );
            break;
        }
        case FileListItem::Ripping:
        {
            item->setText( Column_State, i18n("Ripping") );
            break;
        }
        case FileListItem::Converting:
        {
            item->setText( Column_State, i18n("Converting") );
            break;
        }
        case FileListItem::Stopped:
        {
            item->setText( Column_State, i18n("Stopped") );
            break;
        }
        case FileListItem::BackendNeedsConfiguration:
        {
            item->setText( Column_State, i18n("Backend not configured") );
            break;
        }
        case FileListItem::DiscFull:
        {
            item->setText( Column_State, i18n("Disc full") );
            break;
        }
        case FileListItem::Failed:
        {
            item->setText( Column_State, i18n("Failed") );
            break;
        }
    }

    ConversionOptions *options = config->conversionOptionsManager()->getConversionOptions(item->conversionOptionsId);
    if( options )
        item->setText( Column_Quality, options->profile );

    if( item->track >= 0 )
    {
        if( item->tags )
        {
            item->setText( Column_Input, QString().sprintf("%02i",item->tags->track) + " - " + item->tags->artist + " - " + item->tags->title );
        }
        else // shouldn't be possible
        {
            item->setText( Column_Input, i18n("CD track %1").arg(item->track) );
        }
    }
    else
    {
        item->setText( Column_Input, item->url.pathOrUrl() );
        //if( options ) item->setToolTip( 0, i18n("The file %1 will be converted from %2 to %3 using the %4 profile.\nIt will be saved to: %5").arg(item->url.pathOrUrl()).arg(item->codecName).arg(options->codecName).arg(options->profile).arg(outputUrl.toLocalFile()) );
    }

    update( indexFromItem( item, 0 ) );
    update( indexFromItem( item, 1 ) );
    update( indexFromItem( item, 2 ) );
    update( indexFromItem( item, 3 ) );
}

void FileList::updateItems( QList<FileListItem*> items )
{
    for( int i=0; i<items.size(); i++ )
    {
        updateItem( items.at(i) );
    }
}

void FileList::updateAllItems()
{
    for( int i=0; i<topLevelItemCount(); i++ )
    {
        updateItem( topLevelItem(i) );
    }
}

void FileList::startConversion()
{
    // iterate through all items and set the state to "Waiting"
    FileListItem *item;
    for( int i=0; i<topLevelItemCount(); i++ )
    {
        item = topLevelItem( i );
        if( item->state == FileListItem::Stopped ||
            item->state == FileListItem::BackendNeedsConfiguration ||
            item->state == FileListItem::DiscFull ||
            item->state == FileListItem::Failed
          )
        {
            item->state = FileListItem::WaitingForConversion;
            updateItem( item );
        }
    }

    queue = true;
    emit queueModeChanged( queue );
    emit conversionStarted();
    convertNextItem();
}

void FileList::killConversion()
{
    queue = false;
    emit queueModeChanged( queue );

    FileListItem *item;
    for( int i=0; i<topLevelItemCount(); i++ )
    {
        item = topLevelItem( i );
        if( item->state == FileListItem::Ripping ||
            item->state == FileListItem::Converting ||
            item->state == FileListItem::ApplyingReplayGain
          )
        {
            emit killItem( item );
        }
    }
}

void FileList::stopConversion()
{
    queue = false;
    emit queueModeChanged( queue );
}

void FileList::continueConversion()
{
    queue = true;
    emit queueModeChanged( queue );

    convertNextItem();
}

void FileList::convertNextItem()
{
    if( !queue )
        return;

    int count = convertingCount();
    QStringList devices;
    FileListItem *item;

    // look for tracks that are being ripped
    for( int i=0; i<topLevelItemCount(); i++ )
    {
        item = topLevelItem( i );
        if( item->state == FileListItem::Ripping )
        {
            devices += item->device;
        }
    }

    // look for waiting files
    for( int i=0; i<topLevelItemCount() && count < config->data.general.numFiles; i++ )
    {
        item = topLevelItem( i );
        if( item->state == FileListItem::WaitingForConversion )
        {
            if( item->track >= 0 && !devices.contains(item->device) )
            {
                count++;
                devices += item->device;
                emit convertItem( item );
            }
            else if( item->track < 0 )
            {
                count++;
                emit convertItem( item );
            }
        }
    }

//     itemsSelected();

    if( count == 0 )
        itemFinished( 0, 0 );
}

int FileList::waitingCount()
{
    int count = 0;
    FileListItem *item;

    for( int i=0; i<topLevelItemCount(); i++ )
    {
        item = topLevelItem( i );
        if( item->state == FileListItem::WaitingForConversion )
            count++;
    }

    return count;
}

int FileList::convertingCount()
{
    int count = 0;
    FileListItem *item;

    for( int i=0; i<topLevelItemCount(); i++ )
    {
        item = topLevelItem( i );
        if( item->state == FileListItem::Ripping ||
            item->state == FileListItem::Converting ||
            item->state == FileListItem::ApplyingReplayGain
          )
            count++;
    }

    return count;
}

// qulonglong FileList::spaceLeftForDirectory( const QString& dir )
// {
//     if( dir.isEmpty() )
//         return 0;
//
//     KMountPoint::Ptr mp = KMountPoint::currentMountPoints().findByPath( dir );
//     if( !mp )
//         return 0;
//
//     KDiskFreeSpaceInfo job = KDiskFreeSpaceInfo::freeSpaceInfo( mp->mountPoint() );
//     if( !job.isValid() )
//         return 0;
//
//     KIO::filesize_t mBSize = job.size() / 1024 / 1024;
//     KIO::filesize_t mBUsed = job.used() / 1024 / 1024;
//
//     return mBSize - mBUsed;
// }

void FileList::itemFinished( FileListItem *item, int state )
{
    bool calculatingAlbumGain = false;

    if( item )
    {
        if( state == 0 )
        {
            ConversionOptions *conversionOptions = config->conversionOptionsManager()->getConversionOptions(item->conversionOptionsId);
            if( conversionOptions && conversionOptions->replaygain && config->data.general.waitForAlbumGain )
            {
                item->state = FileListItem::WaitingForAlbumGain;
                updateItem( item );
                calculatingAlbumGain = checkWaitingForAlbumGain();
            }
            else
            {
                config->conversionOptionsManager()->removeConversionOptions( item->conversionOptionsId );
                emit itemRemoved( item );
                if( item->tags )
                    delete item->tags;
                delete item;
    //         itemsSelected();
            }
        }
        else if( state == 1 )
        {
            item->state = FileListItem::Stopped;
            updateItem( item );
        }
        else if( state == 100 )
        {
            item->state = FileListItem::BackendNeedsConfiguration;
            updateItem( item );
        }
        else if( state == 101 )
        {
            item->state = FileListItem::DiscFull;
            updateItem( item );
        }
        else
        {
            item->state = FileListItem::Failed;
            updateItem( item );
        }
    }

    // FIXME disabled until saving gets faster
//     save( false );

    if( waitingCount() > 0 && queue )
    {
        convertNextItem();
    }
    else if( convertingCount() == 0 && !calculatingAlbumGain )
    {
        queue = false;
        save( false );
        emit queueModeChanged( queue );
        float time = 0;
        for( int i=0; i<topLevelItemCount(); i++ )
        {
            FileListItem *temp_item = topLevelItem( i );
            updateItem( temp_item ); // TODO why?
            time += temp_item->length;
        }
        emit finished( time );
        emit conversionStopped( state );
        emit fileCountChanged( topLevelItemCount() );
    }
}

bool FileList::checkWaitingForAlbumGain()
{
    QString albumName;
    QString codecName;
    QList<FileListItem*> items;

    for( int i=0; i<topLevelItemCount(); i++ )
    {
        FileListItem *item = topLevelItem( i );
        if( item->state == FileListItem::WaitingForAlbumGain )
        {
            ConversionOptions *conversionOptions = config->conversionOptionsManager()->getConversionOptions(item->conversionOptionsId);

            if( item->tags )
            {
                if( albumName.isEmpty() )
                    albumName = item->tags.data()->album;

                if( codecName.isEmpty() )
                    codecName = conversionOptions->codecName;

                if( item->tags.data()->album == albumName && conversionOptions->codecName == codecName )
                {
                    items.append( item );
                }
                else
                {
                    break;
                }
            }
            else
            {
                items.append( item );
                break;
            }
        }
        else if( item->state == FileListItem::WaitingForConversion || item->state == FileListItem::Ripping || item->state == FileListItem::Converting || item->state == FileListItem::ApplyingReplayGain )
        {
            if( item->tags )
            {
                ConversionOptions *conversionOptions = config->conversionOptionsManager()->getConversionOptions(item->conversionOptionsId);

                if( items.isEmpty() )
                {
                    continue;
                }
                else if( item->tags.data()->album == albumName && conversionOptions->codecName == codecName )
                {
                    albumName.clear();
                    codecName.clear();
                    items.clear();
                    break; // NOTE this way album gain will be worked off in the order of the file list, not which album finishes conversion first but it is faster with big file lists
                }
                else
                {
                    break;
                }
            }
            else
            {
                if( albumName.isEmpty() )
                {
                    continue;
                }
                else
                {
                    break;
                }
            }
        }
    }

    if( items.count() > 0 )
    {
        for( int i=0; i<items.count(); i++ )
        {
            items[i]->state = FileListItem::ApplyingReplayGain;
            updateItem( items.at(i) );
        }
        emit replaygainItems( items );
        return true;
    }
    else
    {
        // tigger conversion of the next item
        itemFinished( 0, 0 );
    }

    return false;
}

void FileList::replaygainFinished( QList<FileListItem*> items, int state )
{
    // TODO check for errors
    for( int i=0; i<items.count(); i++ )
    {
        if( state == 0 )
        {
            config->conversionOptionsManager()->removeConversionOptions( items.at(i)->conversionOptionsId );
            emit itemRemoved( items.at(i) );
            if( items.at(i)->tags )
                delete items.at(i)->tags;
            delete items.at(i);
//             itemsSelected();
        }
        else if( state == 1 )
        {
            items[i]->state = FileListItem::Stopped;
            updateItem( items.at(i) );
        }
        else if( state == 100 )
        {
            items[i]->state = FileListItem::BackendNeedsConfiguration;
            updateItem( items.at(i) );
        }
        else if( state == 101 )
        {
            items[i]->state = FileListItem::DiscFull;
            updateItem( items.at(i) );
        }
        else
        {
            items[i]->state = FileListItem::Failed;
            updateItem( items.at(i) );
        }
    }

    checkWaitingForAlbumGain();
}

void FileList::rippingFinished( const QString& device )
{
    if( waitingCount() > 0 && queue )
    {
        FileListItem *item;

        // look for waiting files
        for( int i=0; i<topLevelItemCount(); i++ )
        {
            item = topLevelItem( i );
            if( item->state == FileListItem::WaitingForConversion )
            {
                if( item->track >= 0 && item->device == device )
                {
                    emit convertItem( item );
                    return;
                }
            }
        }
    }
}

void FileList::showContextMenu( const QPoint& point )
{
    FileListItem *item = (FileListItem*)itemAt( point );

    // if item is null, we can abort here
    if( !item )
        return;

    // add a tilte to our context manu
    //contextMenu->insertTitle( static_cast<FileListItem*>(item)->fileName ); // TODO sqeeze or something else

    // TODO implement pasting, etc.

    contextMenu->clear();

    // is this file (of our item) beeing converted at the moment?
    if( item->state == FileListItem::WaitingForConversion ||
        item->state == FileListItem::Stopped ||
        item->state == FileListItem::BackendNeedsConfiguration ||
        item->state == FileListItem::DiscFull ||
        item->state == FileListItem::Failed
      )
    {
        contextMenu->addAction( editAction );
        contextMenu->addSeparator();
        contextMenu->addAction( removeAction );
        //contextMenu->addAction( paste );
        contextMenu->addSeparator();
        contextMenu->addAction( startAction );
    }
    else
    {
        //contextMenu->addAction( paste );
        //contextMenu->addSeparator();
        contextMenu->addAction( stopAction );
    }

    // show the popup menu
    contextMenu->popup( viewport()->mapToGlobal(point) );
}

void FileList::showOptionsEditorDialog()
{
    if( !optionsEditor )
    {
        optionsEditor = new OptionsEditor( config, this );
        if( !optionsEditor )
        {
             // TODO error message
            return;
        }
        connect( this, SIGNAL(editItems(QList<FileListItem*>)), optionsEditor, SLOT(itemsSelected(QList<FileListItem*>)) );
        connect( this, SIGNAL(setPreviousItemEnabled(bool)), optionsEditor, SLOT(setPreviousEnabled(bool)) );
        connect( this, SIGNAL(setNextItemEnabled(bool)), optionsEditor, SLOT(setNextEnabled(bool)) );
        connect( this, SIGNAL(itemRemoved(FileListItem*)), optionsEditor, SLOT(itemRemoved(FileListItem*)) );
        connect( optionsEditor, SIGNAL(user2Clicked()), this, SLOT(selectPreviousItem()) );
        connect( optionsEditor, SIGNAL(user1Clicked()), this, SLOT(selectNextItem()) );
        connect( optionsEditor, SIGNAL(updateFileListItems(QList<FileListItem*>)), this, SLOT(updateItems(QList<FileListItem*>)) );
    }
    itemsSelected();
    optionsEditor->show();
}

void FileList::selectPreviousItem()
{
    QTreeWidgetItem *item = itemAbove( selectedItems().first() );

    if( !item )
        return;

    disconnect( this, SIGNAL(itemSelectionChanged()), 0, 0 ); // avoid backfireing

    for( int i=0; i<selectedFiles.count(); i++ )
    {
        selectedFiles.at(i)->setSelected( false );
    }

    item->setSelected( true );
    scrollToItem( item );

    connect( this, SIGNAL(itemSelectionChanged()), this, SLOT(itemsSelected()) );

    itemsSelected();
}

void FileList::selectNextItem()
{
    QTreeWidgetItem *item = itemBelow( selectedItems().last() );

    if( !item )
        return;

    disconnect( this, SIGNAL(itemSelectionChanged()), 0, 0 ); // avoid backfireing

    for( int i=0; i<selectedFiles.count(); i++ )
    {
        selectedFiles.at(i)->setSelected( false );
    }

    item->setSelected( true );
    scrollToItem( item );

    connect( this, SIGNAL(itemSelectionChanged()), this, SLOT(itemsSelected()) );

    itemsSelected();
}

void FileList::removeSelectedItems()
{
    FileListItem *item;
    QList<QTreeWidgetItem*> items = selectedItems();
    for( int i=0; i<items.size(); i++ )
    {
        item = (FileListItem*)items.at(i);
        if( item && item->isSelected() &&
            ( item->state == FileListItem::WaitingForConversion ||
              item->state == FileListItem::WaitingForAlbumGain ||
              item->state == FileListItem::Stopped ||
              item->state == FileListItem::Failed
            )
          )
        {
            emit timeChanged( -item->length );
            config->conversionOptionsManager()->removeConversionOptions( item->conversionOptionsId );
            emit itemRemoved( item );
            if( item->tags )
                delete item->tags;
            delete item;
        }
    }
    emit fileCountChanged( topLevelItemCount() );

    itemsSelected();
}

void FileList::convertSelectedItems()
{
    bool started = false;
    FileListItem *item;
    QList<QTreeWidgetItem*> items = selectedItems();
    for( int i=0; i<items.size(); i++ )
    {
        item = (FileListItem*)items.at(i);
        if( item->state == FileListItem::WaitingForConversion ||
            item->state == FileListItem::Stopped ||
            item->state == FileListItem::BackendNeedsConfiguration ||
            item->state == FileListItem::DiscFull ||
            item->state == FileListItem::Failed
          )
        {
            // emit conversionStarted first because in case the plugin reports an error the conversion stop immediately
            // and the itemFinished slot gets called. If conversionStarted was emitted at the end of this function
            // it might broadcast the message that the conversion has started after the conversion already stopped
            if( !started )
                emit conversionStarted();

            emit convertItem( (FileListItem*)items.at(i) );
            started = true;
        }
    }
}

void FileList::killSelectedItems()
{
    FileListItem *item;
    QList<QTreeWidgetItem*> items = selectedItems();
    for( int i=0; i<items.size(); i++ )
    {
        item = (FileListItem*)items.at(i);
        if( item->state == FileListItem::Converting ||
            item->state == FileListItem::Ripping ||
            item->state == FileListItem::ApplyingReplayGain
          )
            emit killItem( item );
    }
}

void FileList::itemsSelected()
{
    selectedFiles.clear();

    QList<QTreeWidgetItem*> items = selectedItems();
    for( int i=0; i<items.size(); i++ )
    {
        selectedFiles.append( (FileListItem*)items.at(i) );
    }

    if( selectedFiles.count() > 0 )
    {
        emit setPreviousItemEnabled( itemAbove(selectedFiles.first()) != 0 );
        emit setNextItemEnabled( itemBelow(selectedFiles.last()) != 0 );
    }
    else
    {
        emit setPreviousItemEnabled( false );
        emit setNextItemEnabled( false );
    }

    emit editItems( selectedFiles );
}

void FileList::load( bool user )
{
    if( topLevelItemCount() > 0 )
    {
        const int ret = KMessageBox::questionYesNo( this, i18n("Do you want to overwrite the current file list?\n\nIf not, the saved file list will be appended.") );
        if( ret == KMessageBox::Yes )
        {
            FileListItem *item;
            for( int i=0; i<topLevelItemCount(); i++ )
            {
                item = topLevelItem(i);
                if( item->state == FileListItem::WaitingForConversion ||
                    item->state == FileListItem::WaitingForAlbumGain ||
                    item->state == FileListItem::Stopped ||
                    item->state == FileListItem::BackendNeedsConfiguration ||
                    item->state == FileListItem::DiscFull ||
                    item->state == FileListItem::Failed
                  )
                {
                    config->conversionOptionsManager()->removeConversionOptions( item->conversionOptionsId );
                    emit itemRemoved( item );
                    if( item->tags )
                        delete item->tags;
                    delete item;
                    i--;
                }
            }
        }
    }

    QMap<int,int> conversionOptionsIds;
    QString fileName = user ? "filelist.xml" : "filelist_autosave.xml";
    QFile listFile( KStandardDirs::locateLocal("data","soundkonverter/"+fileName) );
    if( listFile.open( QIODevice::ReadOnly ) )
    {
        QDomDocument list("soundkonverter_filelist");
        if( list.setContent( &listFile ) )
        {
            QDomElement root = list.documentElement();
            if( root.nodeName() == "soundkonverter" && root.attribute("type") == "filelist" )
            {
                QDomNodeList conversionOptions = root.elementsByTagName("conversionOptions");
                for( int i=0; i<conversionOptions.count(); i++ )
                {
                    CodecPlugin *plugin = (CodecPlugin*)config->pluginLoader()->backendPluginByName( conversionOptions.at(i).toElement().attribute("pluginName") );
                    if( !plugin )
                        continue;

                    conversionOptionsIds[conversionOptions.at(i).toElement().attribute("id").toInt()] = config->conversionOptionsManager()->addConversionOptions( plugin->conversionOptionsFromXml(conversionOptions.at(i).toElement()) );
                }
                QDomNodeList files = root.elementsByTagName("file");
                for( int i=0; i<files.count(); i++ )
                {
                    QDomElement file = files.at(i).toElement();
                    FileListItem *item = new FileListItem( this );
                    item->url = KUrl(file.attribute("url"));
                    item->outputUrl = KUrl(file.attribute("outputUrl"));
                    if( item->url == item->outputUrl )
                        item->state = FileListItem::WaitingForAlbumGain;
                    item->codecName = file.attribute("codecName");
                    item->conversionOptionsId = conversionOptionsIds[file.attribute("conversionOptionsId").toInt()];
                    item->local = file.attribute("local").toInt();
                    item->track = file.attribute("track").toInt();
                    item->tracks = file.attribute("tracks").toInt();
                    item->device = file.attribute("device");
                    item->length = file.attribute("time").toInt();
                    item->notifyCommand = file.attribute("notifyCommand");
                    config->conversionOptionsManager()->increaseReferences( item->conversionOptionsId );
                    if( file.elementsByTagName("tags").count() > 0 )
                    {
                        QDomElement tags = file.elementsByTagName("tags").at(0).toElement();
                        item->tags = new TagData();
                        item->tags->artist = tags.attribute("artist");
                        item->tags->composer = tags.attribute("composer");
                        item->tags->album = tags.attribute("album");
                        item->tags->title = tags.attribute("title");
                        item->tags->genre = tags.attribute("genre");
                        item->tags->comment = tags.attribute("comment");
                        item->tags->track = tags.attribute("track").toInt();
                        item->tags->disc = tags.attribute("disc").toInt();
                        item->tags->year = tags.attribute("year").toInt();
                        item->tags->track_gain = tags.attribute("track_gain").toFloat();
                        item->tags->album_gain = tags.attribute("album_gain").toFloat();
                        item->tags->length = tags.attribute("length").toInt();
                        item->tags->fileSize = tags.attribute("fileSize").toInt();
                        item->tags->bitrate = tags.attribute("bitrate").toInt();
                        item->tags->samplingRate = tags.attribute("samplingRate").toInt();
                    }
                    addTopLevelItem( item );
                    updateItem( item );
                    emit timeChanged( item->length );
                }
            }
        }
        listFile.close();
        emit fileCountChanged( topLevelItemCount() );
    }
}

void FileList::save( bool user )
{
    // assume it's a misclick if there's nothing to save
    if( user && topLevelItemCount() == 0 )
        return;

    QTime time;
    time.start();

    QDomDocument list("soundkonverter_filelist");
    QDomElement root = list.createElement("soundkonverter");
    root.setAttribute("type","filelist");
    list.appendChild(root);

    QList<int> ids = config->conversionOptionsManager()->getAllIds();
    for( int i=0; i<ids.size(); i++ )
    {
        if( !config->conversionOptionsManager()->getConversionOptions(ids.at(i)) )
        {
            // FIXME error message, null pointer for conversion options
            continue;
        }
        QDomElement conversionOptions = config->conversionOptionsManager()->getConversionOptions(ids.at(i))->toXml(list);
        conversionOptions.setAttribute("id",ids.at(i));
        root.appendChild(conversionOptions);
    }

    FileListItem *item;
    for( int i=0; i<topLevelItemCount(); i++ )
    {
        item = topLevelItem(i);

        QDomElement file = list.createElement("file");
        file.setAttribute("url",item->url.pathOrUrl());
        file.setAttribute("outputUrl",item->outputUrl.pathOrUrl());
        file.setAttribute("codecName",item->codecName);
        file.setAttribute("conversionOptionsId",item->conversionOptionsId);
        file.setAttribute("local",item->local);
        file.setAttribute("track",item->track);
        file.setAttribute("tracks",item->tracks);
        file.setAttribute("device",item->device);
        file.setAttribute("time",item->length);
        file.setAttribute("notifyCommand",item->notifyCommand);
        root.appendChild(file);
        if( item->tags )
        {
            QDomElement tags = list.createElement("tags");
            tags.setAttribute("artist",item->tags->artist);
            tags.setAttribute("composer",item->tags->composer);
            tags.setAttribute("album",item->tags->album);
            tags.setAttribute("title",item->tags->title);
            tags.setAttribute("genre",item->tags->genre);
            tags.setAttribute("comment",item->tags->comment);
            tags.setAttribute("track",item->tags->track);
            tags.setAttribute("disc",item->tags->disc);
            tags.setAttribute("year",item->tags->year);
            tags.setAttribute("track_gain",item->tags->track_gain);
            tags.setAttribute("album_gain",item->tags->album_gain);
            tags.setAttribute("length",item->tags->length);
            tags.setAttribute("fileSize",item->tags->fileSize);
            tags.setAttribute("bitrate",item->tags->bitrate);
            tags.setAttribute("samplingRate",item->tags->samplingRate);
            file.appendChild(tags);
        }
    }

    const QString fileName = user ? "filelist.xml" : "filelist_autosave.xml";
    QFile listFile( KStandardDirs::locateLocal("data","soundkonverter/"+fileName) );
    if( listFile.open( QIODevice::WriteOnly ) )
    {
        QTextStream stream(&listFile);
        stream << list.toString();
        listFile.close();
    }

    logger->log( 1000, QString("Saving the file list took %1 ms").arg(time.elapsed()) );
}


