
#ifndef REPLAYGAINFILELISTVIEW_H
#define REPLAYGAINFILELISTVIEW_H

#include <QTreeView>
#include <QPointer>

class ReplayGainFileListModel;
class Config;

class ReplayGainFileListView : public QTreeView
{
    Q_OBJECT

public:
    explicit ReplayGainFileListView(QWidget *parent);
    ~ReplayGainFileListView();

    void init(Config *config);

    void setModel(ReplayGainFileListModel *model);
    ReplayGainFileListModel *model();

private:
    QPointer<Config> config;

    void dragEnterEvent(QDragEnterEvent *event);
    void dragMoveEvent(QDragMoveEvent *event);
    void dropEvent(QDropEvent *event);
};

#endif // REPLAYGAINFILELISTVIEW_H
