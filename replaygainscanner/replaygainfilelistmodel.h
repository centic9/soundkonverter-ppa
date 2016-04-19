
#ifndef REPLAYGAINFILELISTMODEL_H
#define REPLAYGAINFILELISTMODEL_H

#include <QAbstractItemModel>

#include <QFuture>
#include <QFutureWatcher>
#include <QUrl>
#include <QPointer>

class ReplayGainFileListItem;
class Config;
class TagData;
class DirectoryCrawler;

class ReplayGainFileListModel : public QAbstractItemModel
{
    Q_OBJECT

    struct ReadTagsItem {
        QUrl url;
        QString codecName;
        QFuture<TagData*> *future;
        QFutureWatcher<TagData*> *watcher;
    };

public:
    ReplayGainFileListModel(Config *config, QObject *parent);
    ~ReplayGainFileListModel();
    int rowCount(const QModelIndex &parent=QModelIndex()) const;
    int columnCount(const QModelIndex &parent=QModelIndex()) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
//     Qt::DropActions supportedDropActions() const;
    QModelIndex index(int row, int column, const QModelIndex &parent=QModelIndex()) const;
    QModelIndex parent(const QModelIndex &index) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const;

    /**
     * Get a pointer to colleciton tree item given its index. It is not safe to
     * cache this pointer unless QWeakPointer is used.
     */
    ReplayGainFileListItem *treeItem(const QModelIndex &index) const;

    /**
     * Get (create) index for a collection tree item. The caller must ensure this
     * item is in this model. Invalid model index is returned on null or root item.
     */
    QModelIndex itemIndex(ReplayGainFileListItem *item) const;

public slots:
    bool addFileConcurrent(const QUrl& url, const QString& codecName="");
    bool addDirectoryConcurrent(const QUrl& directory, bool recursive, const QStringList& codecList);

private:
    ReplayGainFileListItem *rootItem;

    Config *config;

    QList<ReadTagsItem> readTagsQueue;

//     QPointer<ProgressLayer> progressLayer;

    QPointer<DirectoryCrawler> directoryCrawler;

private slots:
    void directoryCrawlerFileCountFinished();
    void directoryCrawlerFilesFound(const QStringList &fileList);
    void directoryCrawlerFinished();

    void readTagsFinished();

signals:
    void expandIndex(const QModelIndex &index);
};

#endif // REPLAYGAINFILELISTMODEL_H
