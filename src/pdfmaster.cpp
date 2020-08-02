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
    switch (preferences().pdf_backend)
    {
    case PdfDocument::PopplerBackend:
        document = new PopplerDocument(filename);
        break;
    case PdfDocument::MuPdfBackend:
        document = new MuPdfDocument(filename);
        break;
    }
#else
    document = new PopplerDocument(filename);
#endif
#else
    document = new MuPdfDocument(filename);
#endif
    if (document == nullptr || !document->isValid())
        qFatal("Loading document failed");
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
    default:
        break;
    }
}

void PdfMaster::resolveLink(const int page, const QPointF &position) const
{
    const PdfLink link = document->linkAt(page, position);
    if (link.type >= 0 && link.type < document->numberOfPages())
    {
        writable_preferences().page = link.type;
        emit nagivationSignal(link.type);
    }
}

void PdfMaster::receiveNewPath(const int page, QGraphicsItem *item)
{
    if (!paths.contains(page))
        paths[page] = new PathContainer(this);
    paths[page]->append(item);
}

void PdfMaster::distributeNavigationEvents(const int page) const
{
    QMap<int, SlideScene*> scenemap;
    for (const auto scene : scenes)
    {
        const int scenepage = overlaysShifted(page, scene->getShift()) | scene->pagePart();
        if (scenemap.contains(scenepage))
            emit scene->navigationEvent(scenepage & ~NotFullPage, scenemap[scenepage]);
        else
        {
            scenemap[scenepage] = scene;
            emit scene->navigationEvent(scenepage & ~NotFullPage);
        }
    }
}