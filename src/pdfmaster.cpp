#include "src/pdfmaster.h"
#ifdef INCLUDE_POPPLER
#include "src/rendering/popplerdocument.h"
#endif
#ifdef INCLUDE_MUPDF
#include "src/rendering/mupdfdocument.h"
#endif

PdfMaster::PdfMaster(const QString &filename)
{
    // Load the document
#ifdef INCLUDE_POPPLER
#ifdef INCLUDE_MUPDF
    switch (preferences()->pdf_engine)
    {
    case PdfDocument::PopplerEngine:
        document = new PopplerDocument(filename);
        break;
    case PdfDocument::MuPdfEngine:
        document = new MuPdfDocument(filename);
        break;
    }
#else
    document = new PopplerDocument(filename);
#endif
#else
    document = new MuPdfDocument(filename);
#endif
    if (document == NULL || !document->isValid())
        qFatal("Loading document failed");

    // This is mainly for testing:
    document->loadOutline();
}

PdfMaster::~PdfMaster()
{
    qDeleteAll(paths);
    paths.clear();
    delete document;
}

const QSizeF PdfMaster::getPageSize(const int page_number) const
{
    return document->pageSize(page_number);
}

bool PdfMaster::loadDocument()
{
    return document->loadDocument();
}

void PdfMaster::receiveAction(const Action action)
{
    switch (action)
    {
    case ReloadFiles:
        if (loadDocument())
            // TODO: implement update
            emit update();
        break;
    case UndoDrawing:
    case UndoDrawing | PagePart::LeftHalf:
    case UndoDrawing | PagePart::RightHalf:
    {
        const int page = preferences()->page | (action ^ UndoDrawing);
        PathContainer* const path = pathContainer(page);
        if (path)
        {
            debug_msg(DebugDrawing) << "undo:" << path;
            auto scene_it = scenes.cbegin();
            while ( scene_it != scenes.cend() && ( (*scene_it)->getPage() | (*scene_it)->pagePart() ) != page)
                ++scene_it;
            if (scene_it == scenes.cend())
                path->undo();
            else
                path->undo(*scene_it);
        }
        break;
    }
    case RedoDrawing:
    case RedoDrawing | PagePart::LeftHalf:
    case RedoDrawing | PagePart::RightHalf:
    {
        const int page = preferences()->page | (action ^ RedoDrawing);
        PathContainer* const path = pathContainer(page);
        if (path)
        {
            debug_msg(DebugDrawing) << "redo:" << path;
            auto scene_it = scenes.cbegin();
            while ( scene_it != scenes.cend() && ( (*scene_it)->getPage() | (*scene_it)->pagePart() ) != page)
                ++scene_it;
            if (scene_it == scenes.cend())
                path->redo();
            else
                path->redo(*scene_it);
        }
        break;
    }
    case ClearDrawing:
    case ClearDrawing | PagePart::LeftHalf:
    case ClearDrawing | PagePart::RightHalf:
    {
        PathContainer* const path = pathContainer(preferences()->page | (action ^ ClearDrawing));
        debug_msg(DebugDrawing) << "clear:" << path;
        if (path)
            path->clearPaths();
        break;
    }
    default:
        break;
    }
}

void PdfMaster::resolveLink(const int page, const QPointF &position, const QPointF &startpos) const
{
    // First try to resolve navigation link.
    const PdfLink link = document->linkAt(page, position);
    debug_msg(DebugDrawing) << "resolve link" << page << position << startpos << link.area;
    if ( (startpos.isNull() || link.area.contains(startpos)) && (link.type >= 0 && link.type < document->numberOfPages()) )
        emit navigationSignal(link.type);
    // Next try to handle multimedia annotation.
    const VideoAnnotation annotation = document->annotationAt(page, position);
    if (annotation.mode != VideoAnnotation::Invalid)
        debug_msg(DebugMedia) << annotation.file << annotation.mode << annotation.rect;
}

void PdfMaster::receiveNewPath(int page, QGraphicsItem *item)
{
    if (preferences()->overlay_mode == PerLabel)
        page = document->overlaysShifted((page & ~NotFullPage), FirstOverlay) | (page & NotFullPage);
    if (!paths.contains(page))
        paths[page] = new PathContainer();
    paths[page]->append(item);
}

void PdfMaster::distributeNavigationEvents(const int page) const
{
    // Map (shifted) page numbers with page parts to slide scenes.
    // Like this it can be detected if multiple scenes want to show the same
    // page. In this case the SlideViews showing the same page will all be
    // connected to the same scene.
    QMap<int, SlideScene*> scenemap;
    if (preferences()->overlay_mode == PerLabel)
    {
        for (const auto scene : qAsConst(scenes))
        {
            int shift = scene->getShift();
            const int scenepage = overlaysShifted(page, shift) | scene->pagePart();
            const int indexpage = overlaysShifted(page, (shift & ~AnyOverlay) | FirstOverlay) | scene->pagePart();
            if (scenemap.contains(indexpage))
                scene->navigationEvent(scenepage & ~NotFullPage, scenemap[indexpage]);
            else
            {
                scenemap[indexpage] = scene;
                scene->navigationEvent(scenepage & ~NotFullPage);
            }
        }
    }
    else
    {
        for (const auto scene : qAsConst(scenes))
        {
            const int scenepage = overlaysShifted(page, scene->getShift()) | scene->pagePart();
            if (scenemap.contains(scenepage))
                scene->navigationEvent(scenepage & ~NotFullPage, scenemap[scenepage]);
            else
            {
                scenemap[scenepage] = scene;
                scene->navigationEvent(scenepage & ~NotFullPage);
            }
        }
    }
}


void PdfMaster::saveXopp(const QString &filename) const
{
    QBuffer buffer;
    buffer.open(QBuffer::WriteOnly);
    QXmlStreamWriter writer(&buffer);
    writer.setAutoFormatting(true);
    writer.setAutoFormattingIndent(0);
    writer.writeStartDocument();

    writer.writeStartElement("xournal");
    writer.writeAttribute("creator", "beamerpresenter " APP_VERSION);
    writer.writeTextElement("title", "Xournal(++)-compatible document - see https://github.com/xournalpp/xournalpp");

    // TODO: implement

    writer.writeEndElement();
    writer.writeEndDocument();
    if (writer.hasError())
        qWarning() << "Writing document resulted in error! Resulting document is probably corrupt.";

    gzFile file = gzopen(filename.toUtf8(), "wb");
    gzwrite(file, buffer.data().data(), buffer.data().length());
    gzflush(file, Z_FINISH);
}

void PdfMaster::loadXopp(const QString &filename)
{
    // TODO: implement
}

void PdfMaster::requestPathContainer(PathContainer **container, int page)
{
    switch (preferences()->overlay_mode)
    {
    case PerPage:
        *container = paths.value(page, NULL);
        break;
    case PerLabel:
        page = document->overlaysShifted((page & ~NotFullPage), FirstOverlay) | (page & NotFullPage);
        *container = paths.value(page, NULL);
        debug_msg(DebugDrawing) << "container per label:" << *container << page;
        break;
    case Cumulative:
        *container = paths.value(page, NULL);
        if (*container == NULL || (*container)->isPlainCopy())
        {
            const int page_part = page & NotFullPage;
            int basic_page = page ^ page_part;
            const int start_overlay = document->overlaysShifted(basic_page, FirstOverlay);
            PathContainer *copy_container;
            while (basic_page-- > start_overlay)
            {
                copy_container = paths.value(basic_page | page_part, NULL);
                if (copy_container)
                {
                    delete *container;
                    *container = copy_container->copy();
                    debug_msg(DebugDrawing) << "container copied from page" << basic_page;
                    paths[page] = *container;
                    break;
                }
            }
            debug_msg(DebugDrawing) << "cumulative overlays: nothing found" << basic_page << page;
        }
        break;
    }
}

void PdfMaster::clearHistory(const int page, const int remaining_entries) const
{
    PathContainer *container = paths.value(page, NULL);
    if (container)
        container->clearHistory(remaining_entries);
}

PathContainer *PdfMaster::pathContainer(int page)
{
    switch (preferences()->overlay_mode)
    {
    case PerPage:
        return paths.value(page, NULL);
    case PerLabel:
        page = document->overlaysShifted((page & ~NotFullPage), FirstOverlay) | (page & NotFullPage);
        return paths.value(page, NULL);
    case Cumulative:
        PathContainer *container = paths.value(page);
        if (container && !container->isPlainCopy())
            return container;
        const int page_part = page & NotFullPage;
        int basic_page = page ^ page_part;
        const int start_overlay = document->overlaysShifted(basic_page, FirstOverlay);
        PathContainer *copy_container;
        while (basic_page-- > start_overlay)
        {
            copy_container = paths.value(basic_page | page_part, NULL);
            if (copy_container)
            {
                delete container;
                container = copy_container->copy();
                paths[page] = container;
                return container;
            }
        }
        return NULL;
    }
}
