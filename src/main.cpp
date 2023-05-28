// SPDX-FileCopyrightText: 2022 Valentin Bruch <software@vbruch.eu>
// SPDX-License-Identifier: GPL-3.0-or-later OR AGPL-3.0-or-later

#include <QApplication>
#include <QSettings>
#include <QIcon>
#include <QFileInfo>
#include <QCommandLineParser>
#include "src/config.h"
#include "src/preferences.h"
#include "src/master.h"
#include "src/rendering/pngpixmap.h"
#include "src/drawing/tool.h"
#include "src/log.h"

#ifdef USE_TRANSLATIONS
#include <QTranslator>
#endif

#ifdef USE_POPPLER
#if (QT_VERSION_MAJOR == 6)
#if __has_include(<poppler/qt6/poppler-version.h>)
#include <poppler/qt6/poppler-version.h>
#else
#define POPPLER_VERSION "OLD"
#endif
#elif (QT_VERSION_MAJOR == 5)
#if __has_include(<poppler/qt5/poppler-version.h>)
#include <poppler/qt5/poppler-version.h>
#else
#define POPPLER_VERSION "OLD"
#endif
#endif // QT_VERSION_MAJOR
#endif // USE_POPPLER
#ifdef USE_MUPDF
extern "C"
{
#include <mupdf/fitz/version.h>
}
#endif

int main(int argc, char *argv[])
{
    // Set format for debugging output, warnings etc.
    // To overwrite this you can set the environment variable QT_MESSAGE_PATTERN.
    qSetMessagePattern("%{time process} %{if-debug}D%{endif}%{if-info}INFO%{endif}%{if-warning}WARNING%{endif}%{if-critical}CRITICAL%{endif}%{if-fatal}FATAL%{endif}%{if-category} %{category}%{endif} %{file}:%{line} - %{message}%{if-fatal} from %{backtrace [depth=3]}%{endif}");
    // Register meta types (required for connections).
    qRegisterMetaType<const PngPixmap*>("const PngPixmap*");
    qRegisterMetaType<Tool*>("Tool*");

    // Set up the application.
    QApplication app(argc, argv);
    app.setApplicationName("BeamerPresenter");

    QString fallback_root = QCoreApplication::applicationDirPath();
        if (fallback_root.contains(UNIX_LIKE))
            fallback_root.remove(UNIX_LIKE);
    // Load the icon
    {
        QIcon icon(ICON_FILEPATH);
        if (icon.isNull())
            icon = QIcon(fallback_root + ICON_FILEPATH);
        app.setWindowIcon(icon);
    }

    // Set app version. The string APP_VERSION is defined in src/config.h.
    app.setApplicationVersion(
                APP_VERSION
#ifdef USE_POPPLER
                " poppler=" POPPLER_VERSION
#endif
#ifdef USE_MUPDF
                " mupdf=" FZ_VERSION
#endif
                " Qt=" QT_VERSION_STR
#ifdef QT_DEBUG
                " debugging"
#endif
            );

#ifdef USE_TRANSLATIONS
    QTranslator translator;
    {
        QString translation_path = TRANSLATION_PATH;
        if (!QFileInfo::exists(translation_path))
            translation_path = fallback_root + translation_path;
        for (auto &lang : QLocale().uiLanguages())
        {
            qDebug() << translation_path + lang.replace('-', '_') + "/LC_MESSAGES";
            if (translator.load("beamerpresenter.qm", translation_path + lang.replace('-', '_') + "/LC_MESSAGES"))
            {
                app.installTranslator(&translator);
                break;
            }
        }
    }
#endif

    // Set up command line argument parser.
    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("main", "Modular multi screen PDF presenter"));

    // Define command line options.
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addPositionalArgument("<slides.pdf>", QCoreApplication::translate("main", "Slides for a presentation"));
    parser.addOption({{"c", "config"}, QCoreApplication::translate("main", "settings / configuration file"), QCoreApplication::translate("main", "file")});
    parser.addOption({{"g", "gui-config"}, QCoreApplication::translate("main", "user interface configuration file"), QCoreApplication::translate("main", "file")});
    parser.addOption({{"t", "time"}, QCoreApplication::translate("main", "timer total time in minutes"), QCoreApplication::translate("main", "number")});
    parser.addOption({"log", QCoreApplication::translate("main", "log slide changes to standard output")});
    parser.addOption({"nocache", QCoreApplication::translate("main", "disable cache")});
    parser.addOption({"renderer", QCoreApplication::translate("main", "available PDF renderers:") +
#ifdef USE_MUPDF
                      " MuPDF"
#endif
#ifdef USE_POPPLER
                      " Poppler"
#endif
#ifdef USE_QTPDF
                      " QtPDF"
#endif
#ifdef USE_EXTERNAL_RENDERER
#ifdef USE_MUPDF
                      " external-MuPDF"
#endif
#ifdef USE_POPPLER
                      " external-Poppler"
#endif
#ifdef USE_QTPDF
                      " external-QtPDF"
#endif
#endif // USE_EXTERNAL_RENDERER
                      ,
                      QCoreApplication::translate("main", "name")});
#ifdef QT_DEBUG
    // debugging options and messages are not translated.
    parser.addOption({"debug", "debug flags, comma-separated", "flags"});
#endif
    parser.process(app);

    // Initialize global preferences object.
    if (parser.isSet("c"))
        // parser.value("c") should be the name of a configuration file.
        // If that does not exist or cannot be read, the program can start
        // anyway, if a valid gui config file is defined in parser.value("g").
        __global_preferences = new Preferences(parser.value("c"));
    else
        __global_preferences = new Preferences();
    __global_preferences->master = new Master();
#ifdef QT_DEBUG
    writable_preferences()->loadDebugFromParser(parser);
#endif
    writable_preferences()->loadSettings();
    writable_preferences()->loadFromParser(parser);

    {
        // Create the user interface.
        const QString gui_config_file = parser.value("g").isEmpty() ? preferences()->gui_config_file : parser.value("g");
        Master::Status status = master()->readGuiConfig(gui_config_file);
        if (status == Master::ReadConfigFailed || status == Master::ParseConfigFailed)
        {
            status = master()->readGuiConfig(DEFAULT_GUI_CONFIG_PATH);
            if (status != Master::Success)
                status = master()->readGuiConfig(fallback_root + DEFAULT_GUI_CONFIG_PATH);
            if (status == Master::Success)
                preferences()->showErrorMessage(
                            Master::tr("Error while loading GUI config"),
                            Master::tr("Loading GUI config file failed for filename \"")
                            + gui_config_file
                            + Master::tr("\". Using fallback GUI config file.")
                        );
        }
        if (status != Master::Success)
        {
            qCritical() << QCoreApplication::translate("main", "Parsing the GUI configuration failed with error code") << status;
            delete master();
            delete preferences();
            // Show help and exit.
            parser.showHelp(status);
        }
    }
    // Show all windows.
    master()->showAll();
    // Navigate to first page.
    master()->navigateToPage(0);
    // Distribute cache memory.
    master()->distributeMemory();
    QObject::connect(preferences(), &Preferences::distributeMemory, master(), &Master::distributeMemory);
    // Run the program.
    const int status = app.exec();
    // Clean up. preferences() must be deleted after everything else.
    // Deleting master may take some time since this requires the interruption
    // and deletion of multiple threads.
    delete master();
    delete preferences();
    return status;
}
