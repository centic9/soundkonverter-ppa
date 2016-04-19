
#include "replaygainfilelistview.h"

#include "replaygainfilelistitem.h"
#include "replaygainfilelistmodel.h"
#include "config.h"

#include <KLocalizedString>

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMessageBox>

ReplayGainFileListView::ReplayGainFileListView(QWidget *parent) :
    QTreeView(parent)
{
}

ReplayGainFileListView::~ReplayGainFileListView()
{
}

void ReplayGainFileListView::init(Config *config)
{
    this->config = config;
}

void ReplayGainFileListView::setModel(ReplayGainFileListModel *model)
{
    QTreeView::setModel(model);
}

ReplayGainFileListModel *ReplayGainFileListView::model()
{
    return static_cast<ReplayGainFileListModel*>(QTreeView::model());
}

void ReplayGainFileListView::dragEnterEvent(QDragEnterEvent *event)
{
    if(event->source() == this || event->mimeData()->hasFormat("text/uri-list"))
        event->accept(); // was acceptProposedAction()
}

void ReplayGainFileListView::dragMoveEvent(QDragMoveEvent *event)
{
    const QModelIndex index = indexAt(event->pos());
    ReplayGainFileListItem *destinationItem = model()->treeItem(index);

    if(event->source() == this)
    {
        bool canDrop = true;

        if(event->source() == this && destinationItem)
        {
            if(destinationItem->type == ReplayGainFileListItem::Album)
            {
                foreach(const QModelIndex& selectedIndex, selectedIndexes())
                {
                    ReplayGainFileListItem *selectedItem = model()->treeItem(selectedIndex);

                    if(selectedItem->codecName != destinationItem->codecName || selectedItem->samplingRate() != destinationItem->samplingRate())
                    {
                        canDrop = false;
                        break;
                    }
                }
            }
            else
            {
                canDrop = false;
            }
        }

        if(canDrop)
        {
            event->setDropAction(Qt::MoveAction);
            event->accept();
        }
        else
        {
            event->ignore();
        }
    }
    else
    {
        event->setDropAction(Qt::CopyAction);
        event->accept();
    }
}

void ReplayGainFileListView::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
//     QMap<QString, QList<QStringList>> problems; // codec => [files, solutions]

    if(event->source() == this)
    {
        event->setDropAction(Qt::MoveAction);
        event->accept();

//         const QModelIndex index = indexAt(event->pos());
//         ReplayGainFileListItem *destinationItem = model()->treeItem(index);
//
//         bool canDrop = true;
//         bool showMessage = false;
//
//         if(destinationItem)
//         {
//             if(destinationItem->type == ReplayGainFileListItem::Album)
//             {
//                 foreach(const QModelIndex& selectedIndex, selectedIndexes())
//                 {
//                     ReplayGainFileListItem *selectedItem = model()->treeItem(selectedIndex);
//
//                     if(selectedItem->codecName != destinationItem->codecName || selectedItem->samplingRate() != destinationItem->samplingRate())
//                     {
//                         canDrop = false;
//                         showMessage = true;
//                         break;
//                     }
//                 }
//             }
//             else
//             {
//                 canDrop = false;
//             }
//         }
//
//         if(canDrop)
//         {
//             event->setDropAction(Qt::MoveAction);
//             event->accept();
//         }
//         else if(showMessage)
//         {
//             QMessageBox::critical(this, "soundKonverter", i18n("Some tracks can't be added to the album because either the codec or the sampling rate is different."));
//             event->ignore();
//         }

//         // remove album, if last child was dragged out of it
//         for( int i=0; i<topLevelItemCount(); i++ )
//         {
//             ReplayGainFileListItem *item = topLevelItem(i);
//             if( item->type == ReplayGainFileListItem::Album )
//             {
//                 if( item->childCount() == 0 )
//                 {
//                     delete item;
//                     i--;
//                 }
//             }
//         }
    }
    else
    {
        const bool canReplayGainAac = config->pluginLoader()->canReplayGain("m4a/aac");
        const bool canReplayGainAlac = config->pluginLoader()->canReplayGain("m4a/alac");
        const bool checkM4a = (!canReplayGainAac || !canReplayGainAlac) && canReplayGainAac != canReplayGainAlac;

        const QStringList codecList = config->pluginLoader()->formatList(PluginLoader::ReplayGain, PluginLoader::CompressionType(PluginLoader::InferiorQuality | PluginLoader::Lossy | PluginLoader::Lossless | PluginLoader::Hybrid));

        foreach(const QUrl& url, urls)
        {
            QStringList errorList;

            QString mimeType;
            QString codecName = config->pluginLoader()->getCodecFromFile(url, &mimeType, checkM4a);

            if(mimeType == "inode/directory")
            {
                model()->addDirectoryConcurrent(url, true, codecList);
            }
            else if(config->pluginLoader()->canReplayGain(codecName, 0, &errorList))
            {
                model()->addFileConcurrent(url, codecName);
            }
            else
            {
//                 if(codecName.isEmpty() && !mimeType.startsWith("audio") && !mimeType.startsWith("video") && !mimeType.startsWith("application"))
//                     continue;
//
//                 if(
//                     mimeType == "application/x-ole-storage" || // Thumbs.db
//                     mimeType == "application/x-wine-extension-ini" ||
//                     mimeType == "application/x-cue" ||
//                     mimeType == "application/x-k3b" ||
//                     mimeType == "application/pdf" ||
//                     mimeType == "application/x-trash" ||
//                     mimeType.startsWith("application/vnd.oasis.opendocument") ||
//                     mimeType.startsWith("application/vnd.openxmlformats-officedocument") ||
//                     mimeType.startsWith("application/vnd.sun.xml")
//                 )
//                     continue;
//
//                 const QString fileName = url.url(QUrl::PreferLocalFile);
//
//                 if(codecName.isEmpty())
//                     codecName = mimeType;
//
//                 if(codecName.isEmpty())
//                     codecName = fileName.right(fileName.length() - fileName.lastIndexOf(".") - 1);
//
//                 if(problems.value(codecName).count() < 2)
//                 {
//                     problems[codecName] += QStringList();
//                     problems[codecName] += QStringList();
//                 }
//
//                 problems[codecName][0] += fileName;
//
//                 if(!errorList.isEmpty())
//                 {
//                     problems[codecName][1] += errorList;
//                 }
//                 else
//                 {
//                     problems[codecName][1] += i18n("This file type is unknown to soundKonverter.\nMaybe you need to install an additional soundKonverter plugin.\nYou should have a look at your distribution's package manager for this.");
//                 }
            }
        }

//         QList<CodecProblems::Problem> problemList;
//         for( int i=0; i<problems.count(); i++ )
//         {
//             CodecProblems::Problem problem;
//             problem.codecName = problems.keys().at(i);
//             if( problem.codecName != "wav" )
//             {
//                 problems[problem.codecName][1].removeDuplicates();
//                 problem.solutions = problems.value(problem.codecName).at(1);
//                 if( problems.value(problem.codecName).at(0).count() <= 3 )
//                 {
//                     problem.affectedFiles = problems.value(problem.codecName).at(0);
//                 }
//                 else
//                 {
//                     problem.affectedFiles += problems.value(problem.codecName).at(0).at(0);
//                     problem.affectedFiles += problems.value(problem.codecName).at(0).at(1);
//                     problem.affectedFiles += i18n("... and %1 more files",problems.value(problem.codecName).at(0).count()-3);
//                 }
//                 problemList += problem;
//             }
//         }
//
//         if( problemList.count() > 0 )
//         {
//             CodecProblems *problemsDialog = new CodecProblems( CodecProblems::ReplayGain, problemList, this );
//             problemsDialog->exec();
//         }

        event->acceptProposedAction();
    }
}
