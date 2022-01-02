#include <QFileDialog>
#include "src/preferences.h"
#include "src/rendering/pdfdocument.h"
#include "src/drawing/texttool.h"
#include "src/names.h"


Tool *createTool(const QJsonObject &obj, const int default_device)
{
    const Tool::BasicTool base_tool = string_to_tool.value(obj.value("tool").toString());
    Tool *tool;
    switch (base_tool)
    {
    case Tool::Pen:
    {
        const QColor color(obj.value("color").toString("black"));
        const float width = obj.value("width").toDouble(2.);
        if (width <= 0.)
            return NULL;
        const Qt::PenStyle style = string_to_pen_style.value(obj.value("style").toString(), Qt::SolidLine);
        debug_msg(DebugSettings, "creating pen" << color << width);
        tool = new DrawTool(Tool::Pen, default_device, QPen(color, width, style, Qt::RoundCap, Qt::RoundJoin));
        break;
    }
    case Tool::Highlighter:
    {
        const QColor color(obj.value("color").toString("yellow"));
        const float width = obj.value("width").toDouble(20.);
        if (width <= 0.)
            return NULL;
        const Qt::PenStyle style = string_to_pen_style.value(obj.value("style").toString(), Qt::SolidLine);
        debug_msg(DebugSettings, "creating highlighter" << color << width);
        tool = new DrawTool(Tool::Highlighter, default_device, QPen(color, width, style, Qt::RoundCap, Qt::RoundJoin), QPainter::CompositionMode_Darken);
        break;
    }
    case Tool::Eraser:
    {
        debug_msg(DebugSettings, "creating eraser");
        const QColor color(obj.value("color").toString("#c0808080"));
        const float linewidth = obj.value("linewidth").toDouble(0.5);
        tool = new PointingTool(Tool::Eraser, obj.value("size").toDouble(10.), QBrush(color), default_device, linewidth);
        break;
    }
    case Tool::Pointer:
    {
        const QColor color(obj.value("color").toString("red"));
        const float size = obj.value("size").toDouble(5.);
        if (size <= 0.)
            return NULL;
        debug_msg(DebugSettings, "creating pointer" << color << size);
        tool = new PointingTool(Tool::Pointer, size, color, default_device);
        static_cast<PointingTool*>(tool)->initPointerBrush();
        break;
    }
    case Tool::Torch:
    {
        const QColor color(obj.value("color").toString("#80000000"));
        const float size = obj.value("size").toDouble(80.);
        if (size <= 0.)
            return NULL;
        debug_msg(DebugSettings, "creating torch" << color << size);
        tool = new PointingTool(Tool::Torch, size, color, default_device);
        break;
    }
    case Tool::Magnifier:
    {
        const QColor color(obj.value("color").toString("#80c0c0c0"));
        const float size = obj.value("size").toDouble(120.);
        const float scale = obj.value("scale").toDouble(2.);
        debug_msg(DebugSettings, "creating magnifier" << color << size << scale);
        tool = new PointingTool(Tool::Magnifier, size, color, default_device, scale < 0.1 ? 0.1 : scale > 10. ? 5. : scale);
        break;
    }
    case Tool::TextInputTool:
    {
        QFont font(obj.value("font").toString("black"));
        if (obj.contains("font size"))
            font.setPointSizeF(obj.value("font size").toDouble(12.));
        const QColor color(obj.value("color").toString("black"));
        debug_msg(DebugSettings, "creating text tool" << color << font);
        tool = new TextTool(font, color, default_device);
        break;
    }
    case Tool::InvalidTool:
        debug_msg(DebugSettings, "tried to create invalid tool" << obj.value("tool"));
        return NULL;
    default:
        debug_msg(DebugSettings, "creating default tool" << obj.value("tool"));
        if (base_tool & Tool::AnyDrawTool)
            // Shouldn't happen, but would lead to segmentation faults if it was not handled.
            tool = new DrawTool(base_tool, default_device, QPen());
        else if (base_tool & Tool::AnyPointingTool)
            // Shouldn't happen, but would lead to segmentation faults if it was not handled.
            tool = new PointingTool(base_tool, 10., Qt::black, default_device);
        else
            tool = new Tool(base_tool, default_device);
    }
    const QJsonValue dev_obj = obj.value("device");
    int device = 0;
    if (dev_obj.isDouble())
        device = dev_obj.toInt();
    else if (dev_obj.isString())
        device |= string_to_input_device.value(dev_obj.toString());
    else if (dev_obj.isArray())
        for (const auto &dev : static_cast<const QJsonArray>(obj.value("device").toArray()))
            device |= string_to_input_device.value(dev.toString());
    debug_msg(DebugSettings, "device:" << device);
    if (device)
        tool->setDevice(device);
    return tool;
}

void toolToJson(const Tool *tool, QJsonObject &obj)
{
    if (!tool)
        return;
    obj.insert("tool", string_to_tool.key(tool->tool()));
    obj.insert("device", tool->device());
    if (tool->tool() & Tool::AnyDrawTool)
    {
        obj.insert("width", static_cast<const DrawTool*>(tool)->width());
        obj.insert("color", static_cast<const DrawTool*>(tool)->color().name());
        obj.insert("style", string_to_pen_style.key(static_cast<const DrawTool*>(tool)->pen().style()));
    }
    else if (tool->tool() & Tool::AnyPointingTool)
    {
        obj.insert("size", static_cast<const PointingTool*>(tool)->size());
        obj.insert("color", static_cast<const PointingTool*>(tool)->color().name());
    }
    else if (tool->tool() == Tool::TextInputTool)
    {
        obj.insert("color", static_cast<const TextTool*>(tool)->color().name());
        obj.insert("font", static_cast<const TextTool*>(tool)->font().toString());
    }
}

Preferences::Preferences(QObject *parent) :
    QObject(parent),
    settings(QSettings::NativeFormat, QSettings::UserScope, "beamerpresenter", "beamerpresenter")
{
    settings.setFallbacksEnabled(false);
    settings.setDefaultFormat(QSettings::IniFormat);
    // If settings is empty, copy system scope config to user space file.
    if (settings.allKeys().isEmpty() && settings.isWritable())
    {
        QSettings globalsettings(QSettings::NativeFormat, QSettings::SystemScope, "beamerpresenter", "beamerpresenter");
        for (const auto &key : static_cast<const QList<QString>>(globalsettings.allKeys()))
            settings.setValue(key, globalsettings.value(key));
    }
}

Preferences::Preferences(const QString &file, QObject *parent) :
    QObject(parent),
    settings(file, QSettings::NativeFormat)
{
    settings.setDefaultFormat(QSettings::IniFormat);
}

Preferences::~Preferences()
{
    while (!current_tools.isEmpty())
        delete current_tools.takeLast();
    qDeleteAll(key_tools);
    key_tools.clear();
}

void Preferences::loadSettings()
{
    debug_msg(DebugSettings, "Loading settings:" << settings.fileName());
    debug_msg(DebugSettings, settings.allKeys());
    bool ok;

    // GENERAL SETTINGS
    {
        // Paths to required files / directories
        gui_config_file = settings.value("gui config", DEFAULT_GUI_CONFIG_PATH).toString();
        manual_file = settings.value("manual", DOC_PATH "README.html").toString();
        icon_path = settings.value("icon path", DEFAULT_ICON_PATH).toString();
        const QString icontheme = settings.value("icon theme").toString();
        if (!icontheme.isEmpty())
            QIcon::setThemeName(icontheme);
        const QStringList iconthemepaths = settings.value("icon theme paths").toStringList();
        if (!iconthemepaths.isEmpty())
            QIcon::setThemeSearchPaths(iconthemepaths);
    }
#ifdef QT_DEBUG
    // Only check settings for debug information if was not set on the command line.
    if (debug_level == 0)
    {
        const QStringList debug_flags = settings.value("debug").toStringList();
        for (const auto &flag : debug_flags)
            debug_level |= string_to_debug_flags.value(flag, NoLog);
    }
#endif
    if (settings.contains("log") && settings.value("log", false).toBool())
        global_flags |= LogSlideChanges;
    const int frame_time = settings.value("frame time").toInt(&ok);
    if (ok && frame_time > 0)
        slide_duration_animation = frame_time;
    if (!settings.value("automatic slide changes", true).toBool())
        global_flags &= ~AutoSlideChanges;
    if (!settings.value("gestures", true).toBool())
        gesture_actions.clear();

    // DRAWING
    settings.beginGroup("drawing");
    int value = settings.value("history length visible").toUInt(&ok);
    if (ok)
        history_length_visible_slides = value;
    value = settings.value("history length hidden").toUInt(&ok);
    if (ok)
        history_length_hidden_slides = value;
    overlay_mode = string_to_overlay_mode.value(settings.value("mode").toString(), PdfMaster::Cumulative);
    settings.endGroup();

    // RENDERING
    settings.beginGroup("rendering");
    // page_part threshold
    float threshold = settings.value("page part threshold").toReal(&ok);
    if (ok)
        page_part_threshold = threshold;
    { // renderer
        rendering_command = settings.value("rendering command").toString();
        rendering_arguments = settings.value("rendering arguments").toStringList();
        const QString renderer_str = settings.value("renderer").toString().toLower();
        debug_msg(DebugSettings, renderer_str);
        if (!renderer_str.isEmpty())
        {
            bool understood_renderer = false;
#ifdef USE_MUPDF
            if (renderer_str.count("mupdf") > 0)
            {
                renderer = AbstractRenderer::MuPDF;
                pdf_engine = PdfDocument::MuPdfEngine;
                understood_renderer = true;
            }
#endif
#ifdef USE_POPPLER
            if (renderer_str.count("poppler") > 0)
            {
                renderer = AbstractRenderer::Poppler;
                pdf_engine = PdfDocument::PopplerEngine;
                understood_renderer = true;
            }
#endif
            if (renderer_str.count("extern") > 0)
            {
                if (rendering_command.isEmpty() || rendering_arguments.isEmpty())
                {
                    qWarning() << "External renderer requested but no command or no arguments given. Falling back to Poppler.";
                    qInfo() << "Note that both \"rendering command\" and \"rendering arguments\" are required.";
                    understood_renderer = true;
                }
                else
                    renderer = AbstractRenderer::ExternalRenderer;
            }
            if (!understood_renderer)
                qWarning() << "Invalid renderer argument in settings:" << renderer_str;
        }
    }
    settings.endGroup();

    // cache
    const qreal memory = settings.value("memory").toFloat(&ok);
    if (ok)
        max_memory = memory;
    const int npages = settings.value("cache pages").toInt(&ok);
    if (ok)
        max_cache_pages = npages;

    // INTERACTION
    // Default tools associated to devices
    settings.beginGroup("tools");
    QStringList allKeys = settings.allKeys();
    QList<Action> actions;
    if (!allKeys.isEmpty())
    {
        qDeleteAll(current_tools);
        current_tools.clear();
        for (const auto& dev : allKeys)
            parseActionsTools(settings.value(dev), actions, current_tools, string_to_input_device.value(dev, Tool::AnyNormalDevice));
        actions.clear();
    }
    settings.endGroup();
    // Keyboard shortcuts
    settings.beginGroup("keys");
    allKeys = settings.allKeys();
    if (!allKeys.isEmpty())
    {
        QList<Tool*> tools;
        key_actions.clear();
        for (const auto& key : allKeys)
        {
            const QKeySequence seq(key);
            if (seq.isEmpty())
                qWarning() << "Unknown key sequence in config:" << key;
            else
            {
                parseActionsTools(settings.value(key), actions, tools);
                for (const auto tool : tools)
                    key_tools.insert(seq, tool);
                for (const auto action : actions)
                    key_actions.insert(seq, action);
                actions.clear();
                tools.clear();
            }
        }
    }
    settings.endGroup();
}

void Preferences::parseActionsTools(const QVariant &input, QList<Action> &actions, QList<Tool*> &tools, const int default_device)
{
    // First try to interpret sequence as json object.
    // Here the way how Qt changes the string is not really optimal.
    // In the input quotation marks must be escaped. For
    // convenience it is allowed to use single quotation marks
    // instead.
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(input.toStringList().join(",").replace("'", "\"").toUtf8(), &error);
    QJsonArray array;
    if (error.error == QJsonParseError::NoError)
    {
        if (doc.isArray())
            array = doc.array();
        else if (doc.isObject())
            array.append(doc.object());
    }
    if (array.isEmpty())
    {
        Action action;
        for (const auto &action_str : static_cast<const QStringList>(input.toStringList()))
        {
            action = string_to_action_map.value(action_str.toLower(), Action::InvalidAction);
            if (action == InvalidAction)
                qWarning() << "Unknown action in config" << action_str << "as part of input" << input;
            else
                actions.append(action);
        }
        return;
    }
    for (const auto &value : qAsConst(array))
    {
        if (value.isString())
        {
            const Action action = string_to_action_map.value(value.toString().toLower(), Action::InvalidAction);
            if (action == InvalidAction)
                qWarning() << "Unknown action in config:" << value << "as part of input" << input;
            else
                actions.append(action);
        }
        else if (value.isObject())
        {
            const QJsonObject object = value.toObject();
            Tool *tool = createTool(object, default_device);
            if (tool)
            {
                debug_msg(DebugSettings|DebugDrawing, "Adding tool" << tool << tool->tool() << tool->device());
                tools.append(tool);
            }
        }
    }
}

#ifdef QT_DEBUG
void Preferences::loadDebugFromParser(const QCommandLineParser &parser)
{
    // set debug flags
    if (parser.isSet("debug"))
    {
        debug_level = 0;
        for (const auto &flag : static_cast<const QStringList>(parser.value("debug").split(",")))
            debug_level |= string_to_debug_flags.value("debug " + flag, NoLog);
    }
}
#endif

void Preferences::loadFromParser(const QCommandLineParser &parser)
{
    // presentation file from positional arguments
    const QStringList arguments = parser.positionalArguments();
    if (!arguments.isEmpty())
    {
        file_alias["presentation"] = arguments.first();
        if (!file_alias.contains("notes"))
        {
            if (arguments.length() > 1)
                file_alias["notes"] = arguments[1];
            else
                file_alias["notes"] = arguments.first();
        }
    }

    // timer total time
    if (parser.isSet("t"))
        msecs_total = 60000 * parser.value("t").toDouble();

    // Log slide changes
    if (parser.isSet("log"))
        global_flags |= LogSlideChanges;

    // renderer and pdf engine
    if (parser.isSet("renderer"))
    {
        QString const &renderer_str = parser.value("renderer");
        bool understood_renderer = false;
        debug_msg(DebugSettings, "renderer" << renderer_str);
#ifdef USE_MUPDF
        if (renderer_str.count("mupdf", Qt::CaseInsensitive) > 0)
        {
            renderer = AbstractRenderer::MuPDF;
            pdf_engine = PdfDocument::MuPdfEngine;
            understood_renderer = true;
        }
#endif
#ifdef USE_POPPLER
        if (renderer_str.count("poppler", Qt::CaseInsensitive) > 0)
        {
            renderer = AbstractRenderer::Poppler;
            pdf_engine = PdfDocument::PopplerEngine;
            understood_renderer = true;
        }
#endif
        if (renderer_str.count("extern", Qt::CaseInsensitive) > 0)
        {
            rendering_command = settings.value("rendering command").toString();
            rendering_arguments = settings.value("rendering arguments").toStringList();
            if (rendering_command.isEmpty() || rendering_arguments.isEmpty())
            {
                qWarning() << "External renderer requested but no command or no arguments given. Falling back to Poppler.";
                qInfo() << "Note that both \"rendering command\" and \"rendering arguments\" are required.";
                understood_renderer = true;
            }
            else
                renderer = AbstractRenderer::ExternalRenderer;
        }
        if (!understood_renderer)
            qWarning() << "Invalid renderer argument on command line:" << renderer_str;
    }

#ifdef QT_DEBUG
    // (Re)load debug info from command line.
    if (settings.contains("debug"))
        loadDebugFromParser(parser);
#endif
}

void Preferences::addKeyAction(const QKeySequence sequence, const Action action)
{
    if (!key_actions.contains(sequence) || !key_actions.values(sequence).contains(action))
        key_actions.insert(sequence, action);
    const QString keycode = sequence.toString();
    if (!keycode.isEmpty())
    {
        settings.beginGroup("keys");
        QStringList list = settings.value(keycode).toStringList();
        const QString &string = string_to_action_map.key(action);
        if (!list.contains(string))
        {
            list.append(string);
            settings.setValue(keycode, list);
        }
        settings.endGroup();
    }
}

void Preferences::removeKeyAction(const QKeySequence sequence, const Action action)
{
    key_actions.remove(sequence, action);
    settings.beginGroup("keys");
    const QString keycode = sequence.toString();
    if (!keycode.isEmpty() && settings.contains(keycode))
    {
        QStringList list = settings.value(keycode).toStringList();
        list.removeAll(string_to_action_map.key(action));
        if (list.isEmpty())
            settings.remove(keycode);
        else
            settings.setValue(keycode, list);
    }
    settings.endGroup();
}

void Preferences::setMemory(double new_memory)
{
    max_memory = 1048596*new_memory;
    settings.setValue("memory", QString::number(max_memory));
    emit distributeMemory();
}

void Preferences::setCacheSize(const int new_size)
{
    max_cache_pages = new_size;
    settings.setValue("cache pages", QString::number(max_cache_pages));
    emit distributeMemory();
}

void Preferences::setRenderer(const QString &string)
{
    const QString &new_renderer = string.toLower();
#ifdef USE_MUPDF
    if (new_renderer == "mupdf")
    {
        settings.beginGroup("rendering");
        settings.setValue("renderer", "mupdf");
        settings.endGroup();
        return;
    }
#endif
#ifdef USE_POPPLER
    if (new_renderer == "poppler")
    {
        settings.beginGroup("rendering");
        settings.setValue("renderer", "poppler");
        settings.endGroup();
        return;
    }
#endif
#ifdef USE_MUPDF
    if (new_renderer == "mupdf + external")
    {
        settings.beginGroup("rendering");
        settings.setValue("renderer", "mupdf external");
        settings.endGroup();
        return;
    }
#endif
#ifdef USE_POPPLER
    if (new_renderer == "poppler + external")
    {
        settings.beginGroup("rendering");
        settings.setValue("renderer", "poppler external");
        settings.endGroup();
        return;
    }
#endif
}

Tool *Preferences::currentTool(const int device) const noexcept
{
    for (const auto tool : preferences()->current_tools)
        if (tool && (tool->device() & device))
            return tool;
    return NULL;
}

void Preferences::removeKeyTool(const Tool *tool, const bool remove_from_settings)
{
    if (remove_from_settings)
        settings.beginGroup("keys");
    for (auto it = key_tools.begin(); it != key_tools.end();)
    {
        if (*it == tool)
        {
            emit stopDrawing();
            if (remove_from_settings)
            {
                const QString keycode = QKeySequence(it.key()).toString();
                if (!keycode.isEmpty())
                    settings.remove(keycode);
            }
            it = key_tools.erase(it);
        }
        else
            ++it;
    }
    if (remove_from_settings)
        settings.endGroup();
}

void Preferences::replaceKeyToolShortcut(const QKeySequence oldkeys, const QKeySequence newkeys, Tool *tool)
{
    key_tools.remove(oldkeys, tool);
    settings.beginGroup("keys");
    const QString oldcode = QKeySequence(oldkeys).toString();
    if (!oldcode.isEmpty())
        settings.remove(oldcode);
    if (!newkeys.isEmpty() && tool)
    {
        key_tools.insert(newkeys, tool);
        QJsonObject obj;
        toolToJson(tool, obj);
        /* Convert JSON object to string for config file. This makes
         * it human-readable and circumvents a bug in old Qt versions. */
        settings.setValue(
                    QKeySequence(newkeys).toString(),
                    QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact))
                );
    }
    settings.endGroup();
}

void Preferences::setPagePartThreshold(const double threshold)
{
    page_part_threshold = threshold;
    settings.beginGroup("rendering");
    settings.setValue("page part threshold", page_part_threshold);
    settings.endGroup();
}

void Preferences::setHistoryVisibleSlide(const int length)
{
    if (length >= 0)
        history_length_visible_slides = length;
    settings.beginGroup("drawing");
    settings.setValue("history length visible", history_length_visible_slides);
    settings.endGroup();
}

void Preferences::setHistoryHiddenSlide(const int length)
{
    if (length >= 0)
        history_length_hidden_slides = length;
    settings.beginGroup("drawing");
    settings.setValue("history length hidden", history_length_hidden_slides);
    settings.endGroup();
}

void Preferences::setLogSlideChanges(const bool log)
{
    if (log)
    {
        global_flags |= LogSlideChanges;
        settings.setValue("log", true);
    }
    else
    {
        global_flags &= ~LogSlideChanges;
        settings.remove("log");
    }
}

void Preferences::setRenderingCommand(const QString &string)
{
    rendering_command = string;
    settings.beginGroup("rendering");
    settings.setValue("rendering command", rendering_command);
    settings.endGroup();
}

void Preferences::setRenderingArguments(const QString &string)
{
    rendering_arguments = string.split(",");
    settings.beginGroup("rendering");
    settings.setValue("rendering arguments", rendering_command);
    settings.endGroup();
}

void Preferences::setOverlayMode(const QString &string)
{
    overlay_mode = string_to_overlay_mode.value(string);
    settings.beginGroup("drawing");
    settings.setValue("mode", string);
    settings.endGroup();
}

void Preferences::setAutoSlideChanges(const bool show)
{
    if (show)
        global_flags |= AutoSlideChanges;
    else
        global_flags &= ~AutoSlideChanges;
    settings.setValue("automatic slide changes", show);
}

void Preferences::showErrorMessage(const QString &title, const QString &text) const
{
    qCritical() << text;
    emit sendErrorMessage(title, text);
}

bool Preferences::setGuiConfigFile(const QString &file)
{
    if (file == gui_config_file)
        return false;
    if (QFileInfo(file).isFile())
    {
        settings.setValue("gui config", file);
        gui_config_file = file;
        return true;
    }
    showErrorMessage(tr("Invalid file"), tr("GUI config file not set because it is not a valid file: ") + file);
    return false;
}
