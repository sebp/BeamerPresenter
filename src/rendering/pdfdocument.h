#ifndef PDFDOCUMENT_H
#define PDFDOCUMENT_H

#include <QDateTime>
#include <QUrl>
#include <QRectF>
#include <QVector>

/// Abstract class for handling PDF documents.
/// This class is inherited by classes specific for PDF engines.
class PdfDocument
{
public:
    /// PDF engine
    enum PdfEngine {
#ifdef INCLUDE_POPPLER
        /// Poppler PDF engine
        PopplerEngine = 0,
#endif
#ifdef INCLUDE_MUPDF
        /// MuPDF PDF engine
        MuPdfEngine = 1,
#endif
    };

    /// Unified type of PDF media annotations for all PDF engines.
    struct MediaAnnotation {
        /// URL for (external) media file.
        QUrl file;
        /// Media type. Embedded types are currently not supported.
        enum Type
        {
            InvalidAnnotation = 0,
            HasAudio = 1 << 0,
            HasVideo = 1 << 1,
            Embedded = 1 << 2,
            VideoEmbedded = 7,
            AudioEmbedded = 5,
            VideoExternal = 3,
            AudioExternal = 1,
        };
        /// Media type.
        int type = InvalidAnnotation;

        /// Play modes of media.
        enum Mode
        {
            InvalidMode = -1,
            Once = 0,
            Open,
            Palindrome,
            Repeat,
        }
        /// Play mode
        mode = Once;

        /// Audio volume of media.
        float volume = 1.;
        /// Position of media on slide.
        QRectF rect;
        /// Trivial constructor.
        MediaAnnotation() : file(), type(InvalidAnnotation), mode(InvalidMode), rect() {}
        /// Constructor for full initialization.
        MediaAnnotation(const QUrl &url, const bool hasvideo, const QRectF &rect) : file(url), type(hasvideo ? 3 : 1), mode(Once), rect(rect) {}
        /// Constructor without file.
        MediaAnnotation(const int type, const QRectF &rect) : file(QUrl()), type(type), mode(Once), rect(rect) {}
        /// Trivial destructor.
        virtual ~MediaAnnotation() {}
        /// Comparison by media type, file, mode, and rect.
        virtual bool operator==(const MediaAnnotation &other) const noexcept
        {return     type == other.type
                    && file == other.file
                    && mode == other.mode
                    && rect == other.rect;}
    };

    /// Embedded media file.
    /// Quite useless because currently these objects cannot be played.
    /// @todo implement embedded media files.
    struct EmbeddedMedia : MediaAnnotation {
        /// Data stream.
        QByteArray data;
        /// Audio sampling rate
        int sampling_rate;
        /// Audio channels
        int channels = 1;
        /// Bit per sample
        int bit_per_sample = 8;

        /// Audio encoding modes
        enum Encoding {
            SoundEncodingRaw,
            SoundEncodingSigned,
            SoundEncodingMuLaw,
            SoundEncodingALaw,
        }
        /// Audio encoding
        encoding = SoundEncodingRaw;

        /// Stream compression modes
        enum Compression {
            Uncompressed,
        }
        /// Stream compression
        compression = Uncompressed;

        /// Constructor
        EmbeddedMedia(const QByteArray &data, int sampling_rate, const QRectF &rect) : MediaAnnotation(5, rect), data(data), sampling_rate(sampling_rate) {}
        /// Trivial destructor
        virtual ~EmbeddedMedia() {}
        /// Comparison by all properties, including data.
        virtual bool operator==(const MediaAnnotation &other) const noexcept override
        {return     type == other.type
                    && file == other.file
                    && data.data() == static_cast<const EmbeddedMedia&>(other).data.data()
                    && mode == other.mode
                    && rect == other.rect;}
    };

    /// Unified type of PDF links for all PDF engines.
    struct PdfLink {
        /// Types of links in PDF.
        /// These are all negative, because positive values are interpreted as page
        /// numbers for internal navigation links.
        enum LinkType
        {
            NoLink = -1,
            NavigationLink = -2,
            ExternalLink = -3,
            MovieLink = -4,
            SoundLinx = -5,
        };

        /// Positive values of type are interpreted as page numbers.
        /// Negative values are interpreted as LinkType.
        int type = NoLink;
        /// Link area on slide
        QRectF area;
    };

    /// PDF outline (table of contents, TOC) entry for storing a tree in a list.
    struct PdfOutlineEntry {
        /// Title of the entry.
        QString title;
        /// Page index in the PDF (start counting from 0, destination resolved by the PDF engine)
        int page;
        /// Index of next outline on the same level in some data structure.
        int next;
    };

    /// Unified type of slide transition for all PDF engines.
    struct SlideTransition {
        /// Slide tansition Types.
        enum Type
        {
            Invalid = -1,
            // The numbers used here are the same as in fz_transition::type and
            // in Poppler::PageTransition::Type.
            Replace = 0,
            Split = 1,
            Blinds = 2,
            Box = 3,
            Wipe = 4,
            Dissolve = 5,
            Glitter = 6,
            Fly = 7,
            Push = 8,
            Cover = 9,
            Uncover = 10,
            Fade = 11,
            FlyRectangle = 12,
        };

        /// Direction controlled by 2 bits for outwards and vertical.
        enum Properties
        {
            Outwards = 1,
            Vertical = 2,
        };

        /// Type of the slide transition.
        qint8 type = Replace;

        /// Direction of the transition.
        /// first bit: inward (0) or outward (1) direction.
        /// second bit: horizontal (0) or vertical (1) direction.
        qint8 properties = 0;

        /// Angle in degrees of the direction of the direction.
        qint16 angle = 0;

        /// Transition duration in s.
        float duration = 0.;

        /// Only relevant for Fly and FlyRectangle, in [0,1].
        /// Starting point for "flying" relative to the usual "fly" path.
        float scale = 1.;

        /// Create time-reverse of slide transition (in place)
        void invert()
        {properties ^= Outwards; angle = (angle + 180) % 360;}
    };

protected:
    /// Modification time of the PDF file.
    QDateTime lastModified;

    /// Path to the PDF file.
    QString path;

    /// 0 if all pages have equal size, 1 otherwise, -1 if unknown.
    qint8 flexible_page_sizes = -1;

    /**
     * @brief list representing the outline tree
     *
     * The outline tree is stored as a list of PdfOutlineEntries. Each entry
     * has a property "next" which contains the index of the next entry on the
     * same outline level or minus the index of the next entry on a higher
     * level if no further entries exist on the same level. The first child of
     * the entry (if it has any children) follows immediately after the entry
     * in the list.
     *
     * The first entry is always a dummy entry with page -1 and empty title.
     * This list may never be empty or some functions will cause segmentation
     * faults.
     */
    QVector<PdfOutlineEntry> outline = {{"", -1, 1}};

public:
    /// Constructor: only initialize filename.
    explicit PdfDocument(const QString &filename) : path(filename) {}

    /// Trivial destructor.
    virtual ~PdfDocument() {}

    /// Load or reload the PDF document if the file has been modified since
    /// it was loaded. Return true if the document was reloaded.
    virtual bool loadDocument() = 0;

    /// Size of page in points (point = inch/72).
    virtual const QSizeF pageSize(const int page) const = 0;

    /// Number of pages in PDF file.
    virtual int numberOfPages() const = 0;

    /// Label of page with given index.
    virtual const QString pageLabel(const int page) const = 0;

    /// Label of page with given index.
    virtual int pageIndex(const QString &page) const = 0;

    /// Starting from page start, get the number (index) of the page shifted
    /// by shift_overlay.
    /// If shift is an int and overlay is of type ShiftOverlays:
    /// shift_overlay = (shift & ~AnyOverlay) | overlay
    /// overlay = shift & AnyOverlay
    /// shift = shift >= 0 ? shift & ~AnyOverlay : shift | AnyOverlay
    virtual int overlaysShifted(const int start, const int shift_overlay) const = 0;

    /// List of indices, at which slide labels change. An empty list indicates
    /// that all consecutive slides have different labels.
    virtual QList<int> overlayIndices() const noexcept = 0;

    /// Check whether a file has been loaded successfully.
    virtual bool isValid() const = 0;

    /// Load the PDF outline, fill PdfDocument::outline.
    virtual void loadOutline() = 0;

    /// get function for outline
    const QVector<PdfOutlineEntry> &getOutline() const noexcept {return outline;}

    /// Return outline entry at given page.
    const PdfOutlineEntry &outlineEntryAt(const int page) const;

    /// Link at given position (in point = inch/72)
    virtual const PdfLink linkAt(const int page, const QPointF &position) const = 0;

    /// List all video annotations on given page. Returns NULL if list is
    /// empty.
    virtual QList<MediaAnnotation>* annotations(const int page) const = 0;

    /// Path to PDF file.
    const QString & getPath() const {return path;}

    /// Slide transition when reaching the given page.
    virtual const SlideTransition transition(const int page) const = 0;

    /// Return true if not all pages in the PDF have the same size.
    virtual bool flexiblePageSizes() noexcept = 0;

    /// Duration of given page in secons. Default value is -1 is interpreted as infinity.
    virtual qreal duration(const int page) const noexcept = 0;
};

#endif // PDFDOCUMENT_H
