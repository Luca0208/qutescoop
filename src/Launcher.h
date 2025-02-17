/**************************************************************************
 This file is part of QuteScoop. See README for license
 **************************************************************************/

#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <QLabel>
#include <QProgressBar>
#include <QWidget>
#include <QtGui>
#include <QtNetwork>

class Launcher : public QWidget {
        Q_OBJECT
    public:
        static Launcher* instance(bool createIfNoInstance = true);
        void fireUp();
    public slots:
        void checkData();
    protected:
        void mouseMoveEvent(QMouseEvent *);
        void mousePressEvent(QMouseEvent *);
    signals:
        void dataChecked();
    private slots:
        void dataVersionsDownloaded();
        void dataFileDownloaded();
        void loadNavdata();
    private:
        Launcher(QWidget *parent = 0);
        ~Launcher();
        QPixmap _map;
        QLabel *_image, *_text;
        QProgressBar *_progress;
        QPoint _dragPosition;
        QNetworkReply *_replyDataVersionsAndFiles;
        QList<QString> _dataFilesToDownload;
        QHash<QString, int> _localDataVersionsList, _serverDataVersionsList;
};

#endif // LAUNCHER_H
