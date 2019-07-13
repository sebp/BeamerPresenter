/*
 * This file is part of BeamerPresenter.
 * Copyright (C) 2019  stiglers-eponym

 * BeamerPresenter is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * BeamerPresenter is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with BeamerPresenter. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef MEDIASLIDE_H
#define MEDIASLIDE_H

#include "previewslide.h"
#include "videowidget.h"
#include "embedapp.h"
#include <QSlider>
#include <QDesktopServices>
#include <QMediaPlayer>
#include <QTimer>

class MediaSlide : public PreviewSlide
{
    Q_OBJECT
public:
    explicit MediaSlide(QWidget* parent=nullptr);
    explicit MediaSlide(Poppler::Page* page, QWidget* parent=nullptr);
    ~MediaSlide() override {clearAll();}
    void renderPage(Poppler::Page* page, bool const hasDuration, QPixmap const* pixmap=nullptr);
    void startAllEmbeddedApplications(int const index);
    void initEmbeddedApplications(Poppler::Page const* page);
    void avoidMultimediaBug();
    void setCacheVideos(bool const cache) {cacheVideos=cache;}
    void setMultimediaSliders(QList<QSlider*> sliderList);
    void setEmbedFileList(const QStringList& files) {embedFileList=files;}
    bool hasActiveMultimediaContent() const;
    void updateCacheVideos(Poppler::Page const* page);
    virtual void clearAll() override;

protected:
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void followHyperlinks(QPoint const& pos);
    virtual void clearLists();
    QList<VideoWidget*> videoWidgets;
    QList<EmbedApp*> embedApps;
    QList<VideoWidget*> cachedVideoWidgets;
    QList<QRect> videoPositions;
    QList<QMediaPlayer*> soundPlayers;
    QList<QRect> soundPositions;
    QMap<int,QMediaPlayer*> soundLinkPlayers;
    QMap<int,QSlider*> videoSliders;
    QMap<int,QSlider*> soundSliders;
    QMap<int,QSlider*> soundLinkSliders;
    QMap<int,QMap<int,int>> embedMap;
    QList<QRect> embedPositions;
    QTimer* const autostartTimer = new QTimer(this);
    QTimer* const autostartEmbeddedTimer = new QTimer(this);
    QStringList embedFileList;
    QString pid2wid;
    double autostartDelay = -1.; // delay for starting multimedia content in s
    double autostartEmbeddedDelay = -1.; // delay for starting embedded applications in s
    bool cacheVideos = true;
    bool isOverlay = false;
    virtual void animate() {}
    virtual void endAnimation() {}
    virtual void setDuration() {}

public slots:
    void pauseAllMultimedia();
    void startAllMultimedia();
    void receiveEmbedApp(EmbedApp* app);
    void setAutostartDelay(double const delay) {autostartDelay=delay;}
    void setAutostartEmbeddedDelay(double const delay) {autostartEmbeddedDelay=delay;}
    void setPid2Wid(QString const & program) {pid2wid=program;}

signals:
    void requestMultimediaSliders(int const n);
};

#endif // MEDIASLIDE_H