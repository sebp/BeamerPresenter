// SPDX-FileCopyrightText: 2022 Valentin Bruch <software@vbruch.eu>
// SPDX-License-Identifier: GPL-3.0-or-later OR AGPL-3.0-or-later

#ifndef MEDIAPLAYER_H
#define MEDIAPLAYER_H

#include <QTimer>
#include <QMediaPlayer>
#include "src/config.h"

/**
 * @brief MediaPlayer class, extension of QMediaPlayer
 *
 * adds function setPositionSoft(int) to QMediaPlayer, which can be called
 * repeatedly without making the program freeze.
 *
 * When changing the position by user interaction (e.g. by a slider),
 * setPositionSoft first changes only seekpos to the desired position.
 * A timer is triggered to actually set the position seekpos after 50ms.
 * This avoids many repeated calls to QMediaPlayer::setPosition.
 * The video position will only be changed if setPositionSoft is not called
 * for at least 50ms.
 *
 * @see MediaSlider
 * @see PdfDocument::MediaAnnotation
 */
class MediaPlayer : public QMediaPlayer
{
    Q_OBJECT

    /// Position (in ms) requested by user interaction or -1
    qint64 seekpos = -1;
    /// Single shot timer for triggering calls to QMediaPlayer::setPosition.
    QTimer *timer;

public:
    /// Constructor. Initialize timer for checkPosition.
    explicit MediaPlayer(QObject *parent = nullptr);
    /// Destructor. Delete timer.
    ~MediaPlayer() noexcept {delete timer;}

private slots:
    /// Check if new seek position has been set and change position if necessary.
    void checkPosition();

public slots:
    /// Start playing when end of file is reached. Connect this to
    /// QMediaPlayer::mediaStatusChanged to play a video repeatedly.
    void repeatIfFinished(QMediaPlayer::MediaStatus status) noexcept
    {if (status == QMediaPlayer::EndOfMedia) play();}

    /// Soft version of setPosition: can be called repeatedly without
    /// blocking the programm.
    void setPositionSoft(int position) noexcept
    {seekpos = qint64(position); timer->start(50);}
};

#endif // MEDIAPLAYER_H
