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

#include "pagelabel.h"

PageLabel::PageLabel(Poppler::Page* page, QWidget* parent) : QLabel(parent)
{
    renderPage(page);
}

PageLabel::PageLabel(QWidget* parent) : QLabel(parent)
{
    page = nullptr;
}

PageLabel::~PageLabel()
{
    delete timer;
    clearLists();
    page = nullptr;
}

void PageLabel::setAutostartDelay(double const delay)
{
    autostartDelay = delay;
}

int PageLabel::pageNumber() const
{
    return page->index();
}

void PageLabel::setAnimationDelay(int const delay_ms)
{
    minimumAnimationDelay = delay_ms;
}

void PageLabel::clearLists(bool const killProcesses)
{
    if (sliders.size() != 0) {
        if (sliders.size() == videoWidgets.size() + soundPlayers.size() + linkSoundPlayers.size()) {
            QList<MediaSlider*>::iterator slider = sliders.begin();
            for (QList<VideoWidget*>::iterator video = videoWidgets.begin(); video!=videoWidgets.end(); video++, slider++) {
                disconnect((*video)->getPlayer(), &QMediaPlayer::durationChanged, *slider, &MediaSlider::setMaximum);
                disconnect(*slider, &MediaSlider::sliderMoved, (*video), &VideoWidget::setPosition);
                disconnect((*video)->getPlayer(), &QMediaPlayer::positionChanged, *slider, &MediaSlider::setValue);
            }
            for (QMap<int,QMediaPlayer*>::iterator it=linkSoundPlayers.begin(); it!=linkSoundPlayers.end(); it++, slider++) {
                disconnect(it.value(), &QMediaPlayer::durationChanged, *slider, &MediaSlider::setMaximum);
                disconnect(*slider,    &MediaSlider::sliderMoved,      it.value(), &QMediaPlayer::setPosition);
                disconnect(it.value(), &QMediaPlayer::positionChanged, *slider, &MediaSlider::setValue);
            }
            for (QList<QMediaPlayer*>::iterator player = soundPlayers.begin(); player!=soundPlayers.end(); player++, slider++) {
                disconnect(*player, &QMediaPlayer::durationChanged, *slider, &MediaSlider::setMaximum);
                disconnect(*slider, &MediaSlider::sliderMoved,      *player, &QMediaPlayer::setPosition);
                disconnect(*player, &QMediaPlayer::positionChanged, *slider, &MediaSlider::setValue);
            }
        }
        else
            qDebug() << "Something unexpected happened while trying to delete up sliders.";
    }
    qDeleteAll(sliders);
    sliders.clear();
    qDeleteAll(links);
    links.clear();
    qDeleteAll(linkPositions);
    linkPositions.clear();
    qDeleteAll(videoPositions);
    videoPositions.clear();
    qDeleteAll(videoWidgets);
    videoWidgets.clear();
    qDeleteAll(soundPositions);
    soundPositions.clear();
    qDeleteAll(soundPlayers);
    soundPlayers.clear();
    qDeleteAll(linkSoundPlayers);
    linkSoundPlayers.clear();
    if (killProcesses) {
        qDeleteAll(pidWidCallers);
        pidWidCallers.clear();
        delete processTimer;
        processTimer = nullptr;
        for(QMap<int,QProcess*>::iterator it=processes.begin(); it!=processes.end(); it++) {
            if (it.value() == nullptr)
                continue;
            it.value()->disconnect();
            if (it.value()->state() == QProcess::Running)
                it.value()->terminate();
            delete it.value();
            it.value() = nullptr;
        }
        processes.clear();
        qDeleteAll(embeddedWidgets);
        embeddedWidgets.clear();
    }
}

void PageLabel::renderPage(Poppler::Page* page, bool const setDuration, bool const killProcesses)
{
    emit slideChange();
    clearLists(killProcesses);
    if (page == nullptr)
        return;

    this->page = page;
    QSize pageSize = page->pageSize();
    int shift_x=0, shift_y=0;
    int pageHeight=pageSize.height(), pageWidth=pageSize.width();
    if (pagePart != 0)
        pageWidth /= 2;
    if ( width() * pageHeight > height() * pageWidth ) {
        // the width of the label is larger than required
        resolution = double( height() ) / pageHeight;
        shift_x = int( width()/2 - resolution/2 * pageWidth );
    }
    else {
        // the height of the label is larger than required
        resolution = double( width() ) / pageWidth;
        shift_y = int( height()/2 - resolution/2 * pageHeight );
    }
    double scale_x=resolution*pageWidth, scale_y=resolution*pageHeight;
    if (pagePart !=0) {
        scale_x *= 2;
        if (pagePart == -1)
            shift_x -= width();
    }
    if (page->index() == cachedIndex)
        setPixmap( cachedPixmap );
    else {
        if (pagePart == 0)
            setPixmap( QPixmap::fromImage( page->renderToImage( 72*resolution, 72*resolution ) ) );
        else {
            QImage image = page->renderToImage( 72*resolution, 72*resolution );
            if (pagePart == 1)
                setPixmap( QPixmap::fromImage( image.copy(0, 0, image.width()/2, image.height()) ) );
            else
                setPixmap( QPixmap::fromImage( image.copy(image.width()/2, 0, image.width()/2, image.height()) ) );
        }
    }

    // Collect link areas in pixels
    links = page->links();
    Q_FOREACH(Poppler::Link* link, links) {
        QRectF relative = link->linkArea();
        QRect * absolute = new QRect(
                    shift_x+int(relative.x()*scale_x),
                    shift_y+int(relative.y()*scale_y),
                    int(relative.width()*scale_x),
                    int(relative.height()*scale_y)
                );
        linkPositions.append(absolute);
    }

    if (setDuration) {
        duration = page->duration();
        if ( duration > 0.01)
            QTimer::singleShot(int(1000*duration), this, &PageLabel::timeoutSignal);
        else if ( duration > -0.01) {
            update();
            QTimer::singleShot(int(minimumAnimationDelay), this, &PageLabel::timeoutSignal);
        }
    }
    Poppler::PageTransition* transition = page->transition();
    if (transition != nullptr && transition->type() != Poppler::PageTransition::Replace)
        qInfo() << "Unsupported page transition of type " << transition->type();

    // Show videos. This part is work in progress.
    // TODO: This is probably inefficient.
    if (showMultimedia) {
        // Video
        QSet<Poppler::Annotation::SubType> videoType = QSet<Poppler::Annotation::SubType>();
        videoType.insert(Poppler::Annotation::AMovie);
        QList<Poppler::Annotation*> videos = page->annotations(videoType);
        for (int i=0; i<videos.size(); i++) {
            Poppler::MovieAnnotation * annotation = (Poppler::MovieAnnotation*) videos[i];
            QRectF relative = annotation->boundary();
            QRect * absolute = new QRect(
                        shift_x+int(relative.x()*scale_x),
                        shift_y+int(relative.y()*scale_y),
                        int(relative.width()*scale_x),
                        int(relative.height()*scale_y)
                    );
            videoPositions.append(absolute);

            VideoWidget * video = new VideoWidget(annotation, this);
            videoWidgets.append(video);
        }
        videos.clear();

        // Audio as annotations (Untested, I don't know whether this is useful for anything)
        QSet<Poppler::Annotation::SubType> soundType = QSet<Poppler::Annotation::SubType>();
        soundType.insert(Poppler::Annotation::ASound);
        QList<Poppler::Annotation*> sounds = page->annotations(soundType);
        for (QList<Poppler::Annotation*>::iterator it = sounds.begin(); it!=sounds.end(); it++) {
            qWarning() << "WARNING: Support for sound in annotations is untested!";
            QRectF relative = (*it)->boundary();
            QRect * absolute = new QRect(
                        shift_x+int(relative.x()*scale_x),
                        shift_y+int(relative.y()*scale_y),
                        int(relative.width()*scale_x),
                        int(relative.height()*scale_y)
                    );
            soundPositions.append(absolute);

            Poppler::SoundObject* sound = ((Poppler::SoundAnnotation*) *it)->sound();
            QMediaPlayer* player = new QMediaPlayer(this);
            QUrl url = QUrl(sound->url());
            if (!url.isValid())
                QUrl url = QUrl::fromLocalFile(sound->url());
            if (url.isRelative())
                url = QUrl::fromLocalFile( QDir(".").absoluteFilePath( url.path()) );
            player->setMedia( url );
            soundPlayers.append(player);
        }
        qDeleteAll(sounds);
        sounds.clear();

        for (int i=0; i<links.size(); i++) {
            // Audio links
            switch (links[i]->linkType())
            {
                case Poppler::Link::Sound:
                    {
                        Poppler::SoundObject * sound = ((Poppler::LinkSound*) links[i])->sound();
                        QMediaPlayer * player = new QMediaPlayer(this);
                        QUrl url = QUrl(sound->url());
                        if (!url.isValid())
                            QUrl url = QUrl::fromLocalFile(sound->url());
                        if (url.isRelative())
                            url = QUrl::fromLocalFile( QDir(".").absoluteFilePath( url.path()) );
                        player->setMedia( url );
                        linkSoundPlayers[i] = player;
                        break;
                    }
                case Poppler::Link::Execute:
                    {
                        Poppler::LinkExecute* link = (Poppler::LinkExecute*) links[i];
                        QStringList splitFileName = QStringList();
                        if (!urlSplitCharacter.isEmpty())
                            splitFileName = link->fileName().split(urlSplitCharacter);
                        else
                            splitFileName.append(link->fileName());
                        QUrl url = QUrl(splitFileName[0], QUrl::TolerantMode);
                        splitFileName.append(link->parameters());
                        if (embedFileList.contains(splitFileName[0]) || embedFileList.contains(url.fileName())) {
                            embeddedWidgets[i] = nullptr;
                            processes[i] = nullptr;
                        }
                        else if (splitFileName.length() > 1) {
                            if (splitFileName.contains("embed")) {
                                embeddedWidgets[i] = nullptr;
                                processes[i] = nullptr;
                            }
                        }
                        break;
                    }
            }
        }

        // Autostart
        if (autostartDelay > 0.1) {
            // autostart with delay
            delete timer;
            timer = new QTimer();
            timer->setSingleShot(true);
            connect(timer, &QTimer::timeout, this, &PageLabel::startAllMultimedia );
            timer->start(int(autostartDelay*1000));
        }
        else if (autostartDelay > -0.1) {
            // autostart without delay
            startAllMultimedia();
        }

        // Add slider
        emit requestMultimediaSliders(videoWidgets.size() + linkSoundPlayers.size() + soundPlayers.size());
    }
}

void PageLabel::updateCache(QPixmap* pixmap, int const index)
{
    cachedIndex = index;
    cachedPixmap = *pixmap;
}

void PageLabel::updateCache(Poppler::Page* nextPage)
{
    if (page == nullptr) {
        page = nextPage;
        cachedIndex = nextPage->index();
        if (pagePart == 0)
            cachedPixmap = QPixmap::fromImage( nextPage->renderToImage( 72*resolution, 72*resolution ) );
        else {
            QImage image = nextPage->renderToImage( 72*resolution, 72*resolution );
            if (pagePart == 1)
                cachedPixmap = QPixmap::fromImage( image.copy(0, 0, image.width()/2, image.height()) );
            else
                cachedPixmap = QPixmap::fromImage( image.copy(image.width()/2, 0, image.width()/2, image.height()) );
        }
        return;
    }
    double const nextDuration = nextPage->duration();
    if (nextPage->index() != cachedIndex && ( nextDuration < -0.01  || nextDuration > 0.1) ) {
        cachedIndex = nextPage->index();
        if (pagePart == 0)
            cachedPixmap = QPixmap::fromImage( nextPage->renderToImage( 72*resolution, 72*resolution ) );
        else {
            QImage image = nextPage->renderToImage( 72*resolution, 72*resolution );
            if (pagePart == 1)
                cachedPixmap = QPixmap::fromImage( image.copy(0, 0, image.width()/2, image.height()) );
            else
                cachedPixmap = QPixmap::fromImage( image.copy(image.width()/2, 0, image.width()/2, image.height()) );
        }
    }
}

QPixmap* PageLabel::getCache()
{
    return &cachedPixmap;
}

int PageLabel::getCacheIndex() const
{
    return cachedIndex;
}

void PageLabel::setMultimediaSliders(QList<MediaSlider*> sliderList)
{
    if (sliders.size() != 0) {
        qWarning() << "Something unexpected happened: There is a problem with the media sliders.";
        return;
    }
    sliders = sliderList;
    QList<MediaSlider*>::iterator slider = sliders.begin();
    for (QList<VideoWidget*>::iterator video = videoWidgets.begin(); video!=videoWidgets.end(); video++, slider++) {
        connect((*video)->getPlayer(), &QMediaPlayer::durationChanged, *slider, &MediaSlider::setMaximum);
        int const duration = int((*video)->getDuration()/100);
        if (duration > 0)
            (*slider)->setMaximum(duration);
        connect(*slider, &MediaSlider::sliderMoved, *video, &VideoWidget::setPosition);
        connect((*video)->getPlayer(), &QMediaPlayer::positionChanged, *slider, &MediaSlider::setValue);
    }
    for (QMap<int,QMediaPlayer*>::iterator it=linkSoundPlayers.begin(); it!=linkSoundPlayers.end(); it++, slider++) {
        (*slider)->setRange(0, int(it.value()->duration()));
        connect(it.value(), &QMediaPlayer::durationChanged, *slider, &MediaSlider::setMaximum);
        int const duration = int(it.value()->duration()/100);
        if (duration > 0)
            (*slider)->setMaximum(duration);
        connect(*slider, &MediaSlider::sliderMoved, it.value(), &QMediaPlayer::setPosition);
        connect(it.value(), &QMediaPlayer::positionChanged, *slider, &MediaSlider::setValue);
    }
    for (QList<QMediaPlayer*>::iterator player = soundPlayers.begin(); player!=soundPlayers.end(); player++, slider++) {
        (*slider)->setRange(0, int((*player)->duration()));
        connect(*player, &QMediaPlayer::durationChanged, *slider, &MediaSlider::setMaximum);
        int const duration = int((*player)->duration()/100);
        if (duration > 0)
            (*slider)->setMaximum(duration);
        connect(*slider, &MediaSlider::sliderMoved, *player, &QMediaPlayer::setPosition);
        connect(*player, &QMediaPlayer::positionChanged, *slider, &MediaSlider::setValue);
    }
    show();
}

void PageLabel::startAllMultimedia()
{
    for (int i=0; i<videoWidgets.size(); i++) {
        videoWidgets[i]->setGeometry(*videoPositions[i]);
        videoWidgets[i]->show();
        videoWidgets[i]->play();
    }
    Q_FOREACH(QMediaPlayer* sound, soundPlayers)
        sound->play();
    Q_FOREACH(QMediaPlayer* sound, linkSoundPlayers)
        sound->play();
}

void PageLabel::pauseAllMultimedia()
{
    Q_FOREACH(VideoWidget* video, videoWidgets)
        video->pause();
    Q_FOREACH(QMediaPlayer* sound, soundPlayers)
        sound->pause();
    Q_FOREACH(QMediaPlayer* sound, linkSoundPlayers)
        sound->pause();
}

bool PageLabel::hasActiveMultimediaContent() const
{
    Q_FOREACH(VideoWidget* video, videoWidgets) {
        if (video->state() == QMediaPlayer::PlayingState)
            return true;
    }
    Q_FOREACH(QMediaPlayer* sound, soundPlayers) {
        if (sound->state() == QMediaPlayer::PlayingState)
            return true;
    }
    Q_FOREACH(QMediaPlayer* sound, linkSoundPlayers) {
        if (sound->state() == QMediaPlayer::PlayingState)
            return true;
    }
    return false;
}

void PageLabel::setPresentationStatus(bool const isPresentation)
{
    this->isPresentation = isPresentation;
}

void PageLabel::setShowMultimedia(const bool showMultimedia)
{
    this->showMultimedia = showMultimedia;
}

double PageLabel::getDuration() const
{
    return duration;
}

void PageLabel::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        for (int i=0; i<links.size(); i++) {
            if ( linkPositions[i]->contains(event->pos()) ) {
                switch ( links[i]->linkType() )
                {
                    case Poppler::Link::Goto:
                        emit sendNewPageNumber( ((Poppler::LinkGoto*) links[i])->destination().pageNumber() - 1 );
                    break;
                    case Poppler::Link::Execute:
                        {
                            Poppler::LinkExecute* link = (Poppler::LinkExecute*) links[i];
                            QStringList splitFileName = QStringList();
                            if (!urlSplitCharacter.isEmpty())
                                splitFileName = link->fileName().split(urlSplitCharacter);
                            else
                                splitFileName.append(link->fileName());
                            QUrl url = QUrl(splitFileName[0], QUrl::TolerantMode);
                            splitFileName.append(link->parameters());
                            QString fileName = splitFileName[0];
                            splitFileName.pop_front();
                            if (embeddedWidgets.contains(i)) {
                                if (embeddedWidgets[i] != nullptr)
                                    break;
                                if (processes[i] != nullptr)
                                    break;
                                qWarning() << "This feature is experimental: embedding external applications.";
                                if (pid2wid.isEmpty()) {
                                    QProcess* process = new QProcess(this);
                                    connect(process, &QProcess::readyReadStandardOutput, this, &PageLabel::createEmbeddedWindow);
                                    connect(process, SIGNAL(finished(int)), this, SLOT(clearProcesses(int const)));
                                    process->start(fileName, splitFileName);
                                    processes[i] = process;
                                    qDebug() << "Started process:" << process->program();
                                }
                                else {
                                    QProcess* process = new QProcess(this);
                                    connect(process, SIGNAL(finished(int)), this, SLOT(clearProcesses(int const)));
                                    process->start(fileName, splitFileName);
                                    processes[i] = process;
                                    qDebug() << "Started process:" << process->program();
                                    // Wait some time before trying to get the window ID
                                    // The window has to be created first.
                                    processTimer = new QTimer(this);
                                    processTimer->start(100);
                                    connect(processTimer, &QTimer::timeout, this, &PageLabel::createEmbeddedWindowsFromPID);
                                }
                            }
                            else {
                                // TODO: handle arguments
                                QDesktopServices::openUrl(url);
                            }
                        }
                    break;
                    case Poppler::Link::Browse:
                        QDesktopServices::openUrl( QUrl( ((Poppler::LinkBrowse*) links[i])->url(), QUrl::TolerantMode ) );
                    break;
                    case Poppler::Link::Action:
                        {
                            Poppler::LinkAction* link = (Poppler::LinkAction*) links[i];
                            switch (link->actionType())
                            {
                                case Poppler::LinkAction::Quit:
                                case Poppler::LinkAction::Close:
                                    emit sendCloseSignal();
                                    break;
                                case Poppler::LinkAction::Print:
                                    qInfo() << "Unsupported link action: print.";
                                    break;
                                case Poppler::LinkAction::GoToPage:
                                    emit focusPageNumberEdit();
                                    break;
                                case Poppler::LinkAction::PageNext:
                                    emit sendNewPageNumber( pageNumber() + 1 );
                                    break;
                                case Poppler::LinkAction::PagePrev:
                                    emit sendNewPageNumber( pageNumber() - 1 );
                                    break;
                                case Poppler::LinkAction::PageFirst:
                                    emit sendNewPageNumber(0);
                                    break;
                                case Poppler::LinkAction::PageLast:
                                    emit sendNewPageNumber(-1);
                                    break;
                                case Poppler::LinkAction::Find:
                                    // TODO: implement this
                                    qInfo() << "Unsupported link action: find.";
                                    break;
                                case Poppler::LinkAction::Presentation:
                                    // untested
                                    emit sendShowFullscreen();
                                    break;
                                case Poppler::LinkAction::EndPresentation:
                                    // untested
                                    emit sendEndFullscreen();
                                    break;
                                case Poppler::LinkAction::HistoryBack:
                                    // TODO: implement this
                                    qInfo() << "Unsupported link action: history back.";
                                    break;
                                case Poppler::LinkAction::HistoryForward:
                                    // TODO: implement this
                                    qInfo() << "Unsupported link action: history forward.";
                                    break;
                            };
                        }
                    break;
                    case Poppler::Link::Sound:
                        {
                            Poppler::LinkSound * link = (Poppler::LinkSound*) links[i];
                            Poppler::SoundObject * sound = link->sound();
                            if (sound->soundType() == Poppler::SoundObject::External) {
                                // This is inefficient, but usually linkSoundPlayerPositions.size()==1
                                if (linkSoundPlayers[i]->state() == QMediaPlayer::PlayingState)
                                    linkSoundPlayers[i]->pause();
                                else
                                    linkSoundPlayers[i]->play();
                                break;
                            }
                            else { // untested
                                // Most of the code in this scope is copied from stackoverflow.
                                // I have not tested it, because I don't have a pdf file with embedded sound.
                                // Sound can also be embedded as optional content, which is not supported by this PDF viewer.
                                qWarning() << "Playing embedded sound files is VERY EXPERIMENTAL.\n"
                                           << "Controling the playback is only possible with external files.";
                                QByteArray data = sound->data();
                                QBuffer * audio_buffer = new QBuffer(&data);
                                sound->soundEncoding();
                                QAudioFormat format;
                                // The function names indicate that I can translate the coding to from sound to format:
                                format.setSampleSize(sound->bitsPerSample());
                                format.setSampleRate(sound->samplingRate());
                                format.setChannelCount(sound->channels());
                                // These seem to be some default options... Just try if it works.
                                format.setCodec("audio/pcm");
                                format.setByteOrder(QAudioFormat::BigEndian);
                                switch (sound->soundEncoding())
                                {
                                    case Poppler::SoundObject::SoundEncoding::Raw:
                                        format.setSampleType(QAudioFormat::UnSignedInt);
                                        break;
                                    case Poppler::SoundObject::SoundEncoding::Signed:
                                        format.setSampleType(QAudioFormat::SignedInt);
                                        break;
                                    case Poppler::SoundObject::SoundEncoding::ALaw:
                                    case Poppler::SoundObject::SoundEncoding::muLaw:
                                        // I have no idea what this means...
                                        break;
                                }
                                QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
                                if (!info.isFormatSupported(format)) {
                                    qWarning() << "Audio format of embedded sound not supported by backend.";
                                    return;
                                }
                                QAudioOutput * output = new QAudioOutput(info, format);
                                output->start(audio_buffer);
                                connect(this, &PageLabel::slideChange, this, [&](){ delete output; delete audio_buffer; });
                            }
                        }
                    break;
                    case Poppler::Link::Movie:
                        {
                            qInfo() << "Unsupported link of type video.";
                            Poppler::LinkMovie * link = (Poppler::LinkMovie*) links[i];
                            Q_FOREACH(VideoWidget * video, videoWidgets) {
                                if (link->isReferencedAnnotation(video->getAnnotation()))
                                    video->play();
                            }
                        }
                    break;
                    case Poppler::Link::Rendition:
                        qInfo() << "Unsupported link of type rendition";
                    break;
                    case Poppler::Link::JavaScript:
                        qInfo() << "Unsupported link of type JavaScript";
                    break;
                    case Poppler::Link::OCGState:
                        if ( linkPositions[i]->contains(event->pos()) )
                            qInfo() << "Unsupported link of type OCGState";
                    break;
                    case Poppler::Link::Hide:
                        qInfo() << "Unsupported link of type hide";
                    break;
                    case Poppler::Link::None:
                        qInfo() << "Unsupported link of type none";
                    break;
                }
            }
        }
        for (int i=0; i<soundPositions.size(); i++) {
            if (soundPositions[i]->contains(event->pos())) {
                if (soundPlayers[i]->state() == QMediaPlayer::PlayingState)
                    soundPlayers[i]->pause();
                else
                    soundPlayers[i]->play();
            }
        }
    }
    event->accept();
}

void PageLabel::togglePointerVisibility()
{
    if ( pointer_visible ) {
        pointer_visible = false;
        setMouseTracking(false);
        setCursor( Qt::BlankCursor );
    }
    else {
        pointer_visible = true;
        setMouseTracking(true);
        setCursor( Qt::ArrowCursor );
    }
}

void PageLabel::mouseMoveEvent(QMouseEvent* event)
{
    if (!pointer_visible)
        return;
    bool is_arrow_pointer = cursor() == Qt::ArrowCursor;
    Q_FOREACH(QRect* link_rect, linkPositions) {
        if (link_rect->contains(event->pos())) {
            if (is_arrow_pointer)
                setCursor(Qt::PointingHandCursor);
            return;
        }
    }
    Q_FOREACH(QRect* sound_rect, soundPositions) {
        if (sound_rect->contains(event->pos())) {
            if (is_arrow_pointer)
                setCursor(Qt::PointingHandCursor);
            return;
        }
    }
    if (!is_arrow_pointer)
        setCursor(Qt::ArrowCursor);
    event->accept();
}

Poppler::Page* PageLabel::getPage()
{
    return page;
}

void PageLabel::clearCache()
{
    cachedIndex = -1;
}

void PageLabel::setPagePart(int const state)
{
    pagePart = state;
}

void PageLabel::setEmbedFileList(const QStringList &files)
{
    embedFileList = files;
}

void PageLabel::createEmbeddedWindow()
{
    for(QMap<int,QProcess*>::iterator it=processes.begin(); it!=processes.end(); it++) {
        if (it.value() == nullptr)
            continue;
        char output[64];
        qint64 outputLength = it.value()->readLine(output, sizeof(output));
        if (outputLength != -1) {
            qDebug() << "Creating embedded window from program standard output:" << output;
            QString winIdString(output);
            bool success;
            WId wid = (WId) winIdString.toLongLong(&success, 10);
            if (!success) {
                qWarning() << "Could not read window id";
                continue;
            }
            QRect winGeometry = *linkPositions[it.key()];
            if (winGeometry.height() < 0) {
                winGeometry.setY(winGeometry.y() + winGeometry.height());
                winGeometry.setHeight(-linkPositions[it.key()]->height());
            }
            QWindow* newWindow = QWindow::fromWinId(wid);
            QWidget* newWidget = createWindowContainer(newWindow, this);
            newWidget->setMinimumSize(winGeometry.width(), winGeometry.height());
            newWidget->setMaximumSize(winGeometry.width(), winGeometry.height());
            newWidget->show();
            newWidget->setGeometry(winGeometry);
            embeddedWidgets[it.key()] = newWidget;
            return;
        }
        else
            qDebug() << "Problem when reading program standard output";
    }
    qWarning() << "No standard output found in any process";
}

void PageLabel::createEmbeddedWindowsFromPID()
{
    if (pid2wid.isEmpty()) {
        qCritical() << "No program for translation PID -> window ID specified";
        return;
    }
    bool anyCandidates = false;
    for(QMap<int,QProcess*>::iterator it=processes.begin(); it!=processes.end(); it++) {
        if (it.value() != nullptr && embeddedWidgets[it.key()] == nullptr) {
            PidWidCaller* pidWidCaller = new PidWidCaller(pid2wid, it.value()->pid(), it.key(), this);
            connect(pidWidCaller, &PidWidCaller::sendWid, this, &PageLabel::receiveWid);
            pidWidCallers.insert(pidWidCaller);
            anyCandidates = true;
        }
    }
    // TODO: Stop the timer also if something goes wrong
    if (!anyCandidates)
        processTimer->stop();
    else
        processTimer->setInterval(1.5*processTimer->interval());
}

void PageLabel::setPid2Wid(QString const & program)
{
    pid2wid = program;
}

void PageLabel::receiveWid(WId const wid, int const index)
{
    qDebug() << "Received WID:" << wid;
    if (embeddedWidgets[index] != nullptr || processes[index] == nullptr) {
        qWarning() << "Something strange happened with embedded processes. This is a bug.";
        return;
    }
    QRect winGeometry = *linkPositions[index];
    if (winGeometry.height() < 0) {
        winGeometry.setY(winGeometry.y() + winGeometry.height());
        winGeometry.setHeight(-linkPositions[index]->height());
    }
    QWindow* newWindow = QWindow::fromWinId(wid);
    QWidget* newWidget = createWindowContainer(newWindow, this);
    newWidget->setMinimumSize(winGeometry.width(), winGeometry.height());
    newWidget->setMaximumSize(winGeometry.width(), winGeometry.height());
    newWidget->show();
    newWidget->setGeometry(winGeometry);
    embeddedWidgets[index] = newWidget;
}

void PageLabel::startAllEmbeddedApplications()
{
    qWarning() << "This feature is experimental: embedding external applications.";
    for(QMap<int,QProcess*>::iterator it=processes.begin(); it!=processes.end(); it++) {
        Poppler::LinkExecute* link = (Poppler::LinkExecute*) links[it.key()];
        QStringList splitFileName = QStringList();
        if (!urlSplitCharacter.isEmpty())
            splitFileName = link->fileName().split(urlSplitCharacter);
        else
            splitFileName.append(link->fileName());
        QUrl url = QUrl(splitFileName[0], QUrl::TolerantMode);
        splitFileName.append(link->parameters());
        QString fileName = splitFileName[0];
        splitFileName.pop_front();
        if (embeddedWidgets[it.key()] != nullptr)
            break;
        if (it.value() != nullptr)
            break;
        if (pid2wid.isEmpty()) {
            QProcess* process = new QProcess(this);
            connect(process, &QProcess::readyReadStandardOutput, this, &PageLabel::createEmbeddedWindow);
            process->start(fileName, splitFileName);
            it.value() = process;
            qDebug() << "Started process:" << process->program();
        }
        else {
            QProcess* process = new QProcess(this);
            process->start(fileName, splitFileName);
            it.value() = process;
            qDebug() << "Started process:" << process->program();
            // Wait some time before trying to get the window ID
            // The window has to be created first.
            processTimer = new QTimer(this);
            processTimer->start(100);
            connect(processTimer, &QTimer::timeout, this, &PageLabel::createEmbeddedWindowsFromPID);
        }
    }
}

void PageLabel::clearProcesses(int const exitCode)
{
    for(QMap<int,QProcess*>::iterator it=processes.begin(); it!=processes.end(); it++) {
        if (it.value() != nullptr && it.value()->state() == QProcess::NotRunning) {
            qDebug() << "Deleting process";
            if (it.value()->exitCode() != 0)
                qWarning() << "Embedded application finished with exit code" << it.value()->exitCode();
            delete it.value();
            it.value() = nullptr;
            if (embeddedWidgets[it.key()] != nullptr) {
                embeddedWidgets[it.key()]->close();
                delete embeddedWidgets[it.key()];
                embeddedWidgets[it.key()] = nullptr;
            }
        }
    }
}
