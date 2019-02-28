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

#include <iostream>
#include <QSettings>
#include <QApplication>
#include <QFileDialog>
#include <QCommandLineParser>
#include "controlscreen.h"
#include "presentationscreen.h"


int main(int argc, char *argv[])
{
    qSetMessagePattern("%{time process} %{if-debug}D%{endif}%{if-info}INFO%{endif}%{if-warning}WARNING%{endif}%{if-critical}CRITICAL%{endif}%{if-fatal}FATAL%{endif}%{if-category} %{category}%{endif}%{if-debug} %{file}:%{line}%{endif} - %{message}%{if-fatal} from %{backtrace [depth=3]}%{endif}");

    QApplication app(argc, argv);
    QApplication::setApplicationName("beamerpresenter");

    // set up command line argument parser
    QCommandLineParser parser;
    parser.setApplicationDescription(
            "\nSimple dual screen pdf presentation software.\n"
            "Shortcuts:\n"
            "  c                Update cache\n"
            "  e                Start all embedded applications\n"
            "  g                Go to page (set focus to page number edit)\n"
            "  m                Play or pause all multimedia content\n"
            "  o                Toggle cursor visbility (only on presentation screen)\n"
            "  p                Pause / continue timer\n"
            "  q                Quit\n"
            "  r                Reset timer\n"
            "  t                Show table of contents on control screen\n"
            "  u                Check if files have changed and reload them if necessary\n"
            "  space            Update layout and start or continue timer\n"
            "  Left, PageUp     Go to previous slide and start or continue timer\n"
            "  Right, PageDown  Go to next slide and start or continue timer\n"
            "  Up               Go to the previous slide until the page label changes.\n"
            "                   In beamer presentations: last overlay of the previous slide.\n"
            "  Down             Go to the next slide until the page label changes.\n"
            "                   In beamer presentations: first overlay of the next slide.\n"
            "  F11, f           Toggle fullscreen (only for current window)\n"
            "  return           Accept the page number from the notes and continue presentation\n"
            "  escape           Go to the note page for the current slide. Hide table of contents.\n"
        );
    parser.addHelpOption();
    parser.addPositionalArgument("<slides.pdf>", "Slides for a presentation");
    parser.addPositionalArgument("<notes.pdf>",  "Notes for the presentation (optional, should have the same number of pages as <slides.pdf>)");
    parser.addOptions({
        {{"a", "autoplay"}, "true, false or number: Start video and audio content when entering a slide.\nA number is interpreted as a delay in seconds, after which multimedia content is started.", "value"},
        {{"c", "cache"}, "Number of slides that will be cached. A negative number is treated as infinity.", "int"},
        {{"d", "tolerance"}, "Tolerance for the presentation time in seconds.\nThe timer will be white <secs> before the timeout, green when the timeout is reached, yellow <secs> after the timeout and red 2*<secs> after the timeout.", "secs"},
        {{"e", "embed"}, "file1,file2,... Mark these files for embedding if an execution link points to them.", "files"},
        {{"l", "toc-depth"}, "Number of levels of the table of contents which are shown.", "int"},
        {{"m", "min-delay"}, "Set minimum time per frame in milliseconds.\nThis is useful when using \\animation in LaTeX beamer.", "ms"},
        {{"M", "memory"}, "Maximum size of cache in MiB. A negative number is treated as infinity.", "int"},
        {{"p", "page-part"}, "Set half of the page to be the presentation, the other half to be the notes. Values are \"l\" or \"r\" for presentation on the left or right half of the page, respectively.\nIf the presentation was created with \"\\setbeameroption{show notes on second screen=right}\", you should use \"--page-part=right\".", "side"},
        {{"r", "renderer"}, "\"poppler\", \"custom\" or command: Command for rendering pdf pages to cached images. This command should write a png image to standard output using the arguments %file (path to file), %page (page number), %width and %height (image size in pixels).", "string"},
        {{"s", "scrollstep"}, "Number of pixels which represent a scroll step for a touch pad scroll signal.", "int"},
        {{"t", "timer"}, "Set timer to <time>.\nPossible formats are \"[m]m\", \"[m]m:ss\" and \"h:mm:ss\".", "time"},
        {{"u", "urlsplit"}, "Character which is used to split links into an url and arguments.", "char"},
        {{"w", "pid2wid"}, "Program that converts a PID to a Window ID.", "file"},
    });
    parser.process(app);

    // set up a settings manager
    // This is only tested on Linux! I don't know whether it will work in Windows or MacOS
    #ifdef Q_OS_MAC
    // untested!
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "beamerpresenter", "beamerpresenter");
    #elif Q_OS_WIN
    // untested!
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "beamerpresenter", "beamerpresenter");
    #else
    QSettings settings(QSettings::NativeFormat, QSettings::UserScope, "beamerpresenter", "beamerpresenter");
    #endif

    // Create the GUI
    ControlScreen * w;
    if (parser.positionalArguments().size() == 1)
        w = new ControlScreen(parser.positionalArguments()[0]);
    else if (parser.positionalArguments().size() == 2)
        w = new ControlScreen(parser.positionalArguments()[0], parser.positionalArguments()[1]);
    else {
        // Open files in a QFileDialog
        QString presentationPath = QFileDialog::getOpenFileName(nullptr, "Open Slides", "", "Documents (*.pdf)");
        if (presentationPath.isEmpty()) {
            qCritical() << "No presentation file specified";
            exit(1);
        }
        QString notesPath = "";
        // Check whether a note file should be expected
        if (parser.value("p").isEmpty()) {
            if (!settings.contains("page-part") || settings.value("page-part").toString()=="none" || settings.value("page-part").toString()=="0")
                notesPath = QFileDialog::getOpenFileName(nullptr, "Open Notes", presentationPath, "Documents (*.pdf)");
        }
        else {
            if (parser.value("p")=="none" || parser.value("p")=="0")
                notesPath = QFileDialog::getOpenFileName(nullptr, "Open Notes", presentationPath, "Documents (*.pdf)");
        }
        w = new ControlScreen(presentationPath, notesPath);
    }

    { // set colors
        QColor bgColor, textColor, presentationColor;
        if (settings.contains("notes background color"))
            bgColor = settings.value("notes background color").value<QColor>();
        else
            bgColor = Qt::gray;
        if (settings.contains("notes text color"))
            textColor = settings.value("notes text color").value<QColor>();
        else
            textColor = Qt::black;
        if (settings.contains("presentation color"))
            presentationColor = settings.value("presentation color").value<QColor>();
        else
            presentationColor = Qt::black;
        w->setColor(bgColor, textColor);
        w->setPresentationColor(presentationColor);
    }

    // Read the arguments in parser.
    // Each argument can have a default value in settings.

    // Split page if necessary
    if (!parser.value("p").isEmpty()) {
        QString value = parser.value("p");
        if ( value == "r" || value == "right" )
            w->setPagePart(1);
        else if ( value == "l" || value == "left" )
            w->setPagePart(-1);
        else if (value == "none" || value == "0") {}
        else
            qCritical() << "option \"" << parser.value("p") << "\" to page-part not understood.";
    }
    else if (settings.contains("page-part")) {
        QString value = settings.value("page-part").toString();
        if ( value == "r" || value == "right" )
            w->setPagePart(1);
        else if ( value == "l" || value == "left" )
            w->setPagePart(-1);
        else if (value == "none" || value == "0") {}
        else
            qCritical() << "option \"" << settings.value("page-part") << "\" to page-part in config not understood.";
    }

    // Set tolerance for presentation time
    if (!parser.value("d").isEmpty()) {
        bool success;
        int tolerance = parser.value("d").toInt(&success);
        if (success)
            emit w->sendTimeoutInterval(tolerance);
        else
            qCritical() << "option \"" << parser.value("d") << "\" to tolerance not understood.";
    }
    else if (settings.contains("tolerance")) {
        bool success;
        int tolerance = settings.value("tolerance").toInt(&success);
        if (success)
            emit w->sendTimeoutInterval(tolerance);
        else
            qCritical() << "option \"" << settings.value("tolerance") << "\" to tolerance in config not understood.";
    }

    // Set presentation time
    if (!parser.value("t").isEmpty())
        emit w->sendTimerString(parser.value("t"));
    else if (settings.contains("timer"))
        emit w->sendTimerString(settings.value("timer").toStringList()[0]);

    // Set minimum time per frame
    if (!parser.value("m").isEmpty()) {
        bool success;
        int delay = parser.value("m").toInt(&success);
        if (success)
            emit w->sendAnimationDelay(delay);
        else
            qCritical() << "option \"" << parser.value("m") << "\" to min-delay not understood.";
    }
    else if (settings.contains("min-delay")) {
        bool success;
        int delay = settings.value("min-delay").toInt(&success);
        if (success)
            emit w->sendAnimationDelay(delay);
        else
            qCritical() << "option \"" << settings.value("min-delay") << "\" to min-delay in config not understood.";
    }

    // Set autostart or delay for multimedia content
    if (!parser.value("a").isEmpty()) {
        double delay = 0.;
        bool success;
        QString a = parser.value("a").toLower();
        delay = a.toDouble(&success);
        if (success)
            emit w->sendAutostartDelay(delay);
        else if (a == "true")
            emit w->sendAutostartDelay(0.);
        else if (a == "false")
            emit w->sendAutostartDelay(-2.);
        else
            qCritical() << "option \"" << parser.value("a") << "\" to autoplay not understood.";
    }
    else if (settings.contains("autoplay")) {
        double delay = 0.;
        bool success;
        QString a = settings.value("autoplay").toString().toLower();
        delay = a.toDouble(&success);
        if (success)
            emit w->sendAutostartDelay(delay);
        else if (a == "true")
            emit w->sendAutostartDelay(0.);
        else if (a == "false")
            emit w->sendAutostartDelay(-2.);
        else
            qCritical() << "option \"" << settings.value("autoplay") << "\" to autoplay in config not understood.";
    }

    // Set files, which will be executed in an embedded widget
    // TODO: Wildcard characters
    if (!parser.value("e").isEmpty()) {
        const QStringList files = parser.value("e").split(",");
        w->setEmbedFileList(files);
    }
    else if (settings.contains("embed")) {
        const QStringList files = settings.value("embed").toStringList();
        w->setEmbedFileList(files);
    }

    // Set program, which will convert PIDs to Window IDs
    if (!parser.value("w").isEmpty()) {
        if (parser.value("w").toLower() != "none")
            w->setPid2WidConverter(parser.value("w"));
    }
    else if (settings.contains("pid2wid"))
        w->setPid2WidConverter(settings.value("pid2wid").toString());

    // Set character, which is used to split links into a file name and arguments
    if (!parser.value("u").isEmpty())
        w->setUrlSplitCharacter(parser.value("u"));
    else if ( settings.contains("urlsplit") )
        w->setUrlSplitCharacter(settings.value("urlsplit").toString());

    // Set scroll step for touch pad input devices
    if (!parser.value("s").isEmpty()) {
        bool success;
        int step = parser.value("s").toInt(&success);
        if (success)
            w->setScrollDelta(step);
        else
            qCritical() << "option \"" << parser.value("s") << "\" to scrollstep not understood.";
    }
    else if (settings.contains("scrollstep")) {
        bool success;
        int step = settings.value("scrollstep").toInt(&success);
        if (success)
            w->setScrollDelta(step);
        else
            qCritical() << "option \"" << settings.value("scrollstep") << "\" to scrollstep in config not understood.";
    }

    // Set maximum number of cached slides
    if (!parser.value("c").isEmpty()) {
        bool success;
        int num = parser.value("c").toInt(&success);
        if (success)
            w->setCacheNumber(num);
        else
            qCritical() << "option \"" << parser.value("c") << "\" to cache not understood.";
    }
    else if (settings.contains("cache")) {
        bool success;
        int num = settings.value("cache").toInt(&success);
        if (success)
            w->setCacheNumber(num);
        else
            qCritical() << "option \"" << settings.value("cache") << "\" to cache in config not understood.";
    }

    // Set maximum cache size
    if (!parser.value("M").isEmpty()) {
        bool success;
        int size = parser.value("M").toInt(&success);
        if (success)
            w->setCacheSize(1048576L * size);
        else
            qCritical() << "option \"" << parser.value("M") << "\" to memory not understood.";
    }
    else if (settings.contains("memory")) {
        bool success;
        int size = settings.value("memory").toInt(&success);
        if (success)
            w->setCacheSize(1048576L * size);
        else
            qCritical() << "option \"" << settings.value("memory") << "\" to memory in config not understood.";
    }

    // Set number of visible TOC levels
    if (!parser.value("l").isEmpty()) {
        bool success;
        int num = parser.value("l").toInt(&success);
        if (success)
            w->setTocLevel(num);
        else
            qCritical() << "option \"" << parser.value("l") << "\" to toc-depth not understood.";
    }
    else if (settings.contains("toc-depth")) {
        bool success;
        int num = settings.value("toc-depth").toInt(&success);
        if (success)
            w->setTocLevel(num);
        else
            qCritical() << "option \"" << settings.value("toc-depth") << "\" to toc-depth in config not understood.";
    }

    // Set custom renderer
    if (!parser.value("r").isEmpty()) {
        if (parser.value("r") == "custom") {
            if (settings.contains("renderer"))
                w->setRenderer(settings.value("renderer").toString().split(" "));
            else
                qCritical() << "Ignored request to use undefined custom renderer";
        }
        else if (parser.value("r") != "poppler")
            w->setRenderer(parser.value("r").split(" "));
    }
    else if (settings.contains("renderer"))
        w->setRenderer(settings.value("renderer").toString().split(" "));


    // show the GUI
    w->show();
    w->activateWindow();
    // Render first page on presentation screen
    emit w->sendNewPageNumber(0);
    // Render first page on control screen
    w->renderPage(0);
    // Here one could update the cache.
    // But you probably first want to adjust the window size and then update it with key shortcut space.
    //emit w->sendUpdateCache();
    //w->updateCache();
    int status = app.exec();
    delete w;
    return status;
}
