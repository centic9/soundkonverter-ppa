
// http://qt-project.org/doc/qt-4.8/itemviews-simpletreemodel.html
// http://doc.qt.io/qt-5/qtwidgets-itemviews-puzzle-example.html

#include "replaygainfilelistmodel.h"

#include "replaygainfilelistitem.h"
#include "config.h"
#include "directorycrawler.h"

#include <KLocalizedString>
#include <QtConcurrent/QtConcurrentRun>
#include <QFileInfo>

ReplayGainFileListModel::ReplayGainFileListModel(Config *config, QObject *parent) :
    QAbstractItemModel(parent),
    config(config)
{
    rootItem = new ReplayGainFileListItem();
}

ReplayGainFileListModel::~ReplayGainFileListModel()
{
    delete rootItem;
}

int ReplayGainFileListModel::rowCount(const QModelIndex &parent) const
{
    ReplayGainFileListItem *parentItem;

    if(parent.column() > 0)
        return 0;

    if(!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<ReplayGainFileListItem *>(parent.internalPointer());

    return parentItem->childCount();
}

int ReplayGainFileListModel::columnCount(const QModelIndex &parent) const
{
    if(parent.isValid())
        return static_cast<ReplayGainFileListItem *>(parent.internalPointer())->columnCount();
    else
        return rootItem->columnCount();
}

Qt::ItemFlags ReplayGainFileListModel::flags(const QModelIndex &index) const
{
    if(!index.isValid())
        return 0;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
}

// Qt::DropActions ReplayGainFileListModel::supportedDropActions() const
// {
//     return Qt::MoveAction;
// }

QModelIndex ReplayGainFileListModel::index(int row, int column, const QModelIndex &parent) const
{
    if(!hasIndex(row, column, parent))
        return QModelIndex();

    ReplayGainFileListItem *parentItem;

    if(!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<ReplayGainFileListItem *>(parent.internalPointer());

    ReplayGainFileListItem *childItem = parentItem->child(row);

    if(childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}

QModelIndex ReplayGainFileListModel::parent(const QModelIndex &index) const
{
    if(!index.isValid())
        return QModelIndex();

    ReplayGainFileListItem *childItem = static_cast<ReplayGainFileListItem *>(index.internalPointer());
    ReplayGainFileListItem *parentItem = childItem->parent();

    if(parentItem == rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

QVariant ReplayGainFileListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch(section)
        {
            case 0:
            {
                return i18n("File");
            }
            case 1:
            {
                return i18n("Track");
            }
            case 2:
            {
                return i18n("Album");
            }
        }
    }

    return QVariant();
}

QVariant ReplayGainFileListModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    if(role != Qt::DisplayRole && role != Qt::ToolTipRole)
        return QVariant();

    ReplayGainFileListItem *item = static_cast<ReplayGainFileListItem *>(index.internalPointer());

    return item->data(index.column(), role);
}

bool ReplayGainFileListModel::addFileConcurrent(const QUrl &url, const QString &codecName)
{
    ReadTagsItem readTagsItem;
    readTagsItem.url = url;

    if(!codecName.isEmpty())
    {
        readTagsItem.codecName = codecName;
    }
    else
    {
        const bool canReplayGainAac = config->pluginLoader()->canReplayGain("m4a/aac");
        const bool canReplayGainAlac = config->pluginLoader()->canReplayGain("m4a/alac");
        const bool checkM4a = (!canReplayGainAac || !canReplayGainAlac) && canReplayGainAac != canReplayGainAlac;

        readTagsItem.codecName = config->pluginLoader()->getCodecFromFile(url, 0, checkM4a);

//         if(!config->pluginLoader()->canReplayGain(readTagsItem.codecName, 0))
//         {
//             return false;
//         }
    }

    readTagsItem.future = new QFuture<TagData*>;
    readTagsItem.watcher = new QFutureWatcher<TagData*>;

    connect(readTagsItem.watcher, SIGNAL(finished()), this, SLOT(readTagsFinished()));

//     qDebug() << "reading tags for: " << url.url(QUrl::PreferLocalFile);
    *readTagsItem.future = QtConcurrent::run(config->tagEngine(), &TagEngine::readTags, url);
    readTagsItem.watcher->setFuture(*readTagsItem.future);

    readTagsQueue.append(readTagsItem);

    return true;
}

bool ReplayGainFileListModel::addDirectoryConcurrent(const QUrl &directory, bool recursive, const QStringList &codecList)
{
//     tScanStatus.start();
//
//     if( !progressLayer )
//     {
//         progressLayer = new ProgressLayer(this);
//         progressLayer->hide();
//         layerGrid->addWidget(progressLayer, 0, 0);
//     }
//
//     progressLayer->setMessage(i18n("Searching for files ..."));
//     progressLayer->setValue(0);
//     progressLayer->setMaximum(0);
//     progressLayer->fadeIn();

    directoryCrawler = new DirectoryCrawler(QStringList() << directory.toLocalFile(), recursive);

//     connect(progressLayer, SIGNAL(canceled()), directoryCrawler, SLOT(abort()));

    connect(directoryCrawler, SIGNAL(fileCountFinished()),            this, SLOT(directoryCrawlerFileCountFinished()));
    connect(directoryCrawler, SIGNAL(filesFound(const QStringList&)), this, SLOT(directoryCrawlerFilesFound(const QStringList&)));
    connect(directoryCrawler, SIGNAL(finished()),                     this, SLOT(directoryCrawlerFinished()));
//     connect(directoryCrawler, SIGNAL(fileCountChanged(int)),          progressLayer, SLOT(setMaximum(int)));
//     connect(directoryCrawler, SIGNAL(fileProgressChanged(int)),       progressLayer, SLOT(setValue(int)));

    directoryCrawler->start();

    return true;
}

void ReplayGainFileListModel::directoryCrawlerFileCountFinished()
{
//     progressLayer->setMessage(i18n("Adding files ..."));
}

void ReplayGainFileListModel::directoryCrawlerFilesFound(const QStringList &fileList)
{
    foreach( const QString& file, fileList )
    {
        addFileConcurrent(QUrl(file));
    }
}

void ReplayGainFileListModel::directoryCrawlerFinished()
{
//     progressLayer->fadeOut();

//     emit fileCountChanged(topLevelItemCount());

//     if( queue )
//         convertNextItem();
}

void ReplayGainFileListModel::readTagsFinished()
{
    for(int i=0; i<readTagsQueue.count(); i++)
    {
        const ReadTagsItem& readTagsItem = readTagsQueue.at(i);

        if(readTagsItem.watcher == QObject::sender())
        {
            ReplayGainFileListItem *parentItem = rootItem;

            TagData *tags = readTagsItem.future->result();

            if(tags && !tags->album.simplified().isEmpty())
            {
                // search for an existing album
                foreach(const ReplayGainFileListItem * album, rootItem->children())
                {
                    if(
                        album->tags &&
                        album->type == ReplayGainFileListItem::Album &&
                        album->codecName == readTagsItem.codecName &&
                        album->samplingRate() == tags->samplingRate
                    )
                    {
                        QFileInfo fileInfo(readTagsItem.url.path());

                        if(
                            (
                                config->data.general.replayGainGrouping == Config::Data::General::AlbumDirectory &&
                                album->tags->album == tags->album &&
                                album->url == QUrl(fileInfo.absolutePath())
                            ) ||
                            (
                                config->data.general.replayGainGrouping == Config::Data::General::Album &&
                                album->tags->album == tags->album
                            ) ||
                            (
                                config->data.general.replayGainGrouping == Config::Data::General::Directory &&
                                album->url == QUrl(fileInfo.absolutePath())
                            )
                        )
                        {
                            parentItem = album;
                            break;
                        }
                    }
                }

                // no existing album found
                if(parentItem == rootItem)
                {
                    TagData *albumTags = new TagData();
                    albumTags->album = tags->album;
                    albumTags->samplingRate = tags->samplingRate;
                    QFileInfo fileInfo(readTagsItem.url.path());
                    parentItem = new ReplayGainFileListItem(QUrl(fileInfo.absolutePath()), readTagsItem.codecName, albumTags, ReplayGainFileListItem::Album, rootItem);
                    beginInsertRows(QModelIndex(), rootItem->childCount(), rootItem->childCount());
                    rootItem->appendChild(parentItem);
                    endInsertRows();
                    emit expandIndex(itemIndex(parentItem));
                }
            }

            ReplayGainFileListItem *fileItem = new ReplayGainFileListItem(readTagsItem.url, readTagsItem.codecName, tags, ReplayGainFileListItem::Track, parentItem);
            beginInsertRows(itemIndex(parentItem), parentItem->childCount(), parentItem->childCount());
            parentItem->appendChild(fileItem);
            endInsertRows();

            delete readTagsItem.future;
            delete readTagsItem.watcher;
            readTagsQueue.removeAt(i);

            break;
        }
    }
}

ReplayGainFileListItem *ReplayGainFileListModel::treeItem(const QModelIndex &index) const
{
    if(!index.isValid() || index.model() != this)
        return 0;

    return static_cast<ReplayGainFileListItem *>(index.internalPointer());
}

QModelIndex ReplayGainFileListModel::itemIndex(ReplayGainFileListItem *item) const
{
    if(!item || item == rootItem)
        return QModelIndex();

    return createIndex(item->row(), 0, item);
}
