// SPDX-FileCopyrightText: 2022 Valentin Bruch <software@vbruch.eu>
// SPDX-License-Identifier: GPL-3.0-or-later OR AGPL-3.0-or-later

#include <QTimer>
#include <QThread>
#include <QPixmap>

#include "src/config.h"
#include "src/log.h"
#include "src/rendering/pixcache.h"
#ifdef USE_POPPLER
#include "src/rendering/popplerdocument.h"
#include "src/rendering/popplerrenderer.h"
#endif
#ifdef USE_MUPDF
#include "src/rendering/mupdfdocument.h"
#include "src/rendering/mupdfrenderer.h"
#endif
#ifdef USE_QTPDF
#include "src/rendering/qtdocument.h"
#include "src/rendering/qtrenderer.h"
#endif
#ifdef USE_EXTERNAL_RENDERER
#include "src/rendering/externalrenderer.h"
#endif
#include "src/rendering/pngpixmap.h"
#include "src/rendering/pixcachethread.h"
#include "src/preferences.h"


PixCache::PixCache(PdfDocument *doc, const int thread_number, const PagePart page_part, QObject *parent) noexcept :
    QObject(parent),
    priority({page_part}),
    pdfDoc(doc)
{
    threads = QVector<PixCacheThread*>(doc->flexiblePageSizes() ? 0 : thread_number);
}

void PixCache::init()
{
    const PagePart page_part = static_cast<PagePart>(priority.isEmpty() ? 0 : priority.first());
    priority.clear();
    // Create the renderer without any checks.
    switch (preferences()->renderer)
    {
#ifdef USE_QTPDF
    case renderer::QtPDF:
        renderer = new QtRenderer(static_cast<const QtDocument*>(pdfDoc), page_part);
        break;
#endif
#ifdef USE_POPPLER
    case renderer::Poppler:
        renderer = new PopplerRenderer(static_cast<const PopplerDocument*>(pdfDoc), page_part);
        break;
#endif
#ifdef USE_MUPDF
    case renderer::MuPDF:
        renderer = new MuPdfRenderer(static_cast<const MuPdfDocument*>(pdfDoc), page_part);
        break;
#endif
#ifdef USE_EXTERNAL_RENDERER
    case renderer::ExternalRenderer:
        renderer = new ExternalRenderer(preferences()->rendering_command, preferences()->rendering_arguments, pdfDoc, page_part);
        break;
#endif
    }

    // Check if the renderer is valid
    if (renderer == NULL || !renderer->isValid())
        qCritical() << "Creating renderer failed" << preferences()->renderer;

    // Create threads.
    for (auto &thread : threads)
    {
        thread = new PixCacheThread(pdfDoc, renderer->pagePart(), this);
        connect(thread, &PixCacheThread::sendData, this, &PixCache::receiveData, Qt::QueuedConnection);
        connect(this, &PixCache::setPixCacheThreadPage, thread, &PixCacheThread::setNextPage, Qt::QueuedConnection);
    }

    renderCacheTimer = new QTimer();
    connect(thread(), &QThread::finished, renderCacheTimer, &QTimer::deleteLater);
    renderCacheTimer->setSingleShot(true);
    renderCacheTimer->setInterval(0);
    connect(renderCacheTimer, &QTimer::timeout, this, &PixCache::startRendering, Qt::QueuedConnection);
}

PixCache::~PixCache()
{
    delete renderer;
    // TODO: correctly clean up threads!
    for (const auto &thread : qAsConst(threads))
        thread->quit();
    for (const auto &thread : qAsConst(threads))
    {
        thread->wait(10000);
        delete thread;
    }
    clear();
}

void PixCache::setMaxMemory(const float memory)
{
    maxMemory = memory;
    if (memory < usedMemory && memory >= 0)
        limitCacheSize();
}

void PixCache::setMaxNumber(const int number)
{
    maxNumber = number;
    if (number < usedMemory && number >= 0)
        limitCacheSize();
}

void PixCache::clear()
{
    for (auto it = cache.begin(); it != cache.end(); it=cache.erase(it))
        delete *it;
    usedMemory = 0;
    region = {preferences()->page, preferences()->page};
}

const QPixmap PixCache::pixmap(const int page, qreal resolution)
{
    if (resolution <= 0.)
        resolution = getResolution(page);
    // Try to return a page from cache.
    {
        const auto it = cache.constFind(page);
        if (it != cache.cend() && *it != NULL && abs((*it)->getResolution() - resolution) < MAX_RESOLUTION_DEVIATION)
            return (*it)->pixmap();
    }
    // Check if page number is valid.
    if (page < 0 || page >= pdfDoc->numberOfPages())
        return QPixmap();

    // Check if the renderer is valid
    if (renderer == NULL || !renderer->isValid())
    {
        qCritical() << "Invalid renderer";
        return QPixmap();
    }

    debug_msg(DebugCache, "Rendering in main thread");
    const QPixmap pix = renderer->renderPixmap(page, resolution);

    if (pix.isNull())
    {
        qCritical() << "Rendering page failed" << page << resolution;
        return pix;
    }

    // Write pixmap to cache.
    const PngPixmap *png = new PngPixmap(pix, page, resolution);
    if (png == NULL)
        qWarning() << "Converting pixmap to PNG failed";
    else
    {
        if (cache.value(page, NULL) != NULL)
        {
            usedMemory -= cache[page]->size();
            delete cache[page];
        }
        cache[page] = png;
        usedMemory += png->size();
    }

    return pix;
}

void PixCache::requestRenderPage(const int n)
{
    if (!priority.contains(n) && !cache.contains(n))
        priority.append(n);

    // Start rendering next page.
    renderCacheTimer->start();
}

void PixCache::pageNumberChanged(const int page)
{
    // Update boundaries of the simply connected region.
    if (!cache.contains(page))
    {
        // If current page is not yet in cache: make sure it is first in priority queue.
        if (!priority.isEmpty() && priority.first() != page)
        {
            priority.removeOne(page);
            priority.push_front(page);
        }
        region.first = page;
        region.second = page;
        return;
    }

    // Make sure that current page is inside the region.
    if (region.first > page || region.second < page) {
        region.first = page - 1;
        region.second = page + 1;
    }

    // Extend the region as far as possible by searching for gaps.
    // Use that keys in QMap are sorted and can be accessed by iterators.
    QMap<int, const PngPixmap*>::const_iterator
            left = cache.constFind(region.first),
            right = cache.constFind(region.second);
    if (left != cache.cend())
    {
        const auto limit = std::prev(cache.cbegin());
        while (left != limit && left.key() == region.first)
        {
            --left;
            --region.first;
        }
    }
    while (right != cache.cend() && right.key() == region.second)
    {
        ++right;
        ++region.second;
    }

    // Start rendering next page.
    renderCacheTimer->start();
}

int PixCache::limitCacheSize()
{
    // Check restrictions on memory usage and number of slides.
    if (maxMemory < 0 && maxNumber < 0)
    {
        // Check if all pages are already in memory.
        if (cache.size() == pdfDoc->numberOfPages())
            return 0;
        return INT_MAX >> 1;
    }
    if (maxNumber == 0 || maxMemory == 0)
    {
        clear();
        return 0;
    }

    // Check if region is valid.
    if (region.first > region.second)
    {
        region.first = preferences()->page;
        region.second = preferences()->page;
    }

    // Number of really cached slides:
    // subtract number of currently active threads, for which cache contains a NULL.
    int cached_slides = cache.size();
    for (auto it = threads.cbegin(); it != threads.cend(); ++it)
    {
        if ((*it) && (*it)->isRunning())
            --cached_slides;
    }
    if (cached_slides <= 0)
        return INT_MAX >> 1;

    // Calculate how many slides still fit in available memory
    int allowed_slides = INT_MAX >> 1;
    if (maxMemory > 0)
    {
        // Get currently used memory.
        // Get number of slides which still fit in available memory.
        if (usedMemory > 0 && cached_slides > 0)
            allowed_slides = (maxMemory - usedMemory) * cached_slides / usedMemory;
        else
            allowed_slides = threads.length();
        debug_verbose(DebugCache, "set allowed_slides" << usedMemory << cached_slides << allowed_slides << maxMemory << threads.length());
    }
    if (maxNumber > 0 && allowed_slides + cache.size() > maxNumber)
        allowed_slides = maxNumber - cache.size();

    // If threads.length() pages can be rendered without problems: return
    if (allowed_slides >= threads.length())
        return allowed_slides;

    debug_msg(DebugCache, "prepared deleting from cache" << usedMemory << maxMemory << allowed_slides << cached_slides);


    // Deleting starts from first or last page in cache.
    // The aim is to shrink the cache to a simply connected region
    // around the current page.
    int first = cache.firstKey();
    int last  = cache.lastKey();
    /// Cached page which should be removed.
    const PngPixmap *remove;

    // Delete pages while allowed_slides is negative or too small to
    // allow updates.
    do {
        // If the set of cached pages is simply connected, includes the
        // current page, and lies mostly ahead of the current page,
        // then stop rendering to cache.
        if (((maxNumber < 0 || cache.size() <= maxNumber)
                && (maxMemory < 0 || usedMemory <= maxMemory)
                && last > preferences()->page
                && last - first <= cache.size()
                && 2*last + 3*first > 5*preferences()->page)
                // the case cache.size() < 2 would lead to segfaults.
                || cache.size() < 2)
            return 0;

        // If more than 3/4 of the cached slides lie ahead of current page, clean up last.
        if (last + 3*first > 4*preferences()->page)
        {
            const QMap<int, const PngPixmap*>::iterator it = std::prev(cache.end());
            remove = it.value();
            last = std::prev(cache.erase(it)).key();
        }
        else
        {
            const QMap<int, const PngPixmap*>::iterator it = cache.begin();
            remove = it.value();
            first = cache.erase(it).key();
        }
        // Check if remove is NULL (which means that a thread is just rendering it).
        // TODO: make sure this case is correctly handled when the thread finishes.
        if (remove == NULL)
            continue;
        debug_msg(DebugCache, "removing page from cache" << usedMemory << allowed_slides << cached_slides << remove->getPage());
        // Delete removed cache page and update memory size.
        usedMemory -= remove->size();
        delete remove;
        --cached_slides;

        // Update allowed_slides
        if (usedMemory > 0 && cached_slides > 0)
        {
            allowed_slides = (maxMemory - usedMemory) * cached_slides / usedMemory;
            if (allowed_slides + cache.size() > maxNumber)
                allowed_slides = maxNumber - cache.size();
        }
        else
            allowed_slides = maxNumber - cache.size();

    } while (allowed_slides < threads.length() && cached_slides > 0);

    // Update boundaries of simply connected region
    if (first > region.first + 1)
        region.first = first - 1;
    if (last + 1 < region.second)
        region.second = last + 1;

    return allowed_slides;
}

int PixCache::renderNext()
{
    // Check if priority contains pages which are not yet rendered.
    int page;
    while (!priority.isEmpty())
    {
        page = priority.takeFirst();
        if (!cache.contains(page))
            return page;
    }

    // Check if region is valid.
    if (region.first > region.second)
    {
        region.first = preferences()->page;
        region.second = preferences()->page;
    }

    // Select region.first or region.second for rendering.
    while (true)
    {
        if (region.second + 3*region.first > 4*preferences()->page && region.first >= 0)
        {
            if (!cache.contains(region.first))
                return region.first--;
            --region.first;
        }
        else
        {
            if (!cache.contains(region.second))
                return region.second++;
            ++region.second;
        }
    }
}

void PixCache::startRendering()
{
    debug_verbose(DebugCache, "Start rendering");
    // Clean up cache and check if there is enough space for more cached pages.
    int allowed_pages = limitCacheSize();
    if (allowed_pages <= 0)
        return;
    for (auto thread = threads.cbegin(); thread != threads.cend(); ++thread)
    {
        if (allowed_pages > 0 && *thread && !(*thread)->isRunning())
        {
            const int page = renderNext();
            if (page < 0 || page >= pdfDoc->numberOfPages())
                return;
            emit setPixCacheThreadPage(*thread, page, getResolution(page));
            --allowed_pages;
        }
    }
}

void PixCache::receiveData(const PngPixmap *data)
{
    // If a renderer failed, it should already have sent an error message.
    if (data == NULL || data->isNull())
    {
        delete data;
        return;
    }

    // Check if the received image is still compatible with the current resolution.
    if (abs(getResolution(data->getPage()) - data->getResolution()) > MAX_RESOLUTION_DEVIATION)
    {
        if (cache.value(data->getPage()) == NULL)
            cache.remove(data->getPage());
        delete data;
    }
    else
    {
        if (cache.value(data->getPage(), NULL) != NULL)
        {
            usedMemory -= cache[data->getPage()]->size();
            delete cache[data->getPage()];
        }
        cache[data->getPage()] = data;
        usedMemory += data->size();
    }

    // Start rendering next page.
    renderCacheTimer->start();
}

qreal PixCache::getResolution(const int page) const
{
    // Get page size in points
    QSizeF pageSize = pdfDoc->pageSize(page);
    if (pageSize.isEmpty())
        return -1.;
    if (renderer->pagePart() != FullPage)
        pageSize.rwidth() /= 2;
    if (pageSize.width() * frame.height() > pageSize.height() * frame.width())
    {
        // page is too wide, determine resolution by x direction
        return frame.width() / pageSize.width();
    }
    // page is too high, determine resolution by y direction
    return frame.height() / pageSize.height();
}

void PixCache::updateFrame(const QSizeF &size)
{
    if (frame != size && threads.length() > 0)
    {
        debug_msg(DebugCache, "update frame" << frame << size);
        frame = size;
        clear();
    }
}

void PixCache::requestPage(const int page, const qreal resolution, const bool cache_page)
{
    debug_verbose(DebugCache, "requested page" << page << resolution);
    // Try to return a page from cache.
    {
        const auto it = cache.constFind(page);
        debug_verbose(DebugCache, "searched for page" << page << (it == cache.cend()) << (it != cache.cend() && *it != NULL) << (it == cache.cend() ? -1024 : ((*it)->getResolution() - resolution)));
        if (it != cache.cend() && *it != NULL && abs((*it)->getResolution() - resolution) < MAX_RESOLUTION_DEVIATION)
        {
            emit pageReady((*it)->pixmap(), page);
            return;
        }
    }
    // Check if page number is valid.
    if (page < 0 || page >= pdfDoc->numberOfPages())
        return;

    // Render new page.
    // Check if the renderer is valid
    if (renderer == NULL || !renderer->isValid())
    {
        qCritical() << "Invalid renderer";
        return;
    }

    debug_msg(DebugCache, "Rendering page in PixCache thread" << this);
    const QPixmap pix = renderer->renderPixmap(page, resolution);

    if (pix.isNull())
    {
        qCritical() << "Rendering page failed" << page << resolution;
        return;
    }

    emit pageReady(pix, page);

    if (cache_page)
    {
        // Write pixmap to cache.
        const PngPixmap *png = new PngPixmap(pix, page, resolution);
        if (png == NULL)
            qWarning() << "Converting pixmap to PNG failed";
        else
        {
            if (cache.value(page, NULL) != NULL)
            {
                usedMemory -= cache[page]->size();
                delete cache[page];
            }
            cache[page] = png;
            usedMemory += png->size();
            debug_verbose(DebugCache, "writing page to cache" << page << usedMemory);
        }
    }

    // Start rendering next page.
    renderCacheTimer->start();
}

void PixCache::getPixmap(const int page, QPixmap &target, qreal resolution)
{
    target = pixmap(page, resolution);
}
