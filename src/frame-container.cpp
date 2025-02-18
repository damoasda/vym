#include "frame-container.h"

#include <QColor>
#include <QDebug>
#include <QGraphicsScene>

#include "misc.h" //for roof function

/////////////////////////////////////////////////////////////////
// FrameContainer
/////////////////////////////////////////////////////////////////
FrameContainer::FrameContainer()
{
    //qDebug() << "Constr FrameContainer this=" << this;
    init();
}

FrameContainer::~FrameContainer()
{
    //qDebug() << "Destr FrameContainer";
    clear();
}

void FrameContainer::init()
{
    containerType = Frame;
    frameTypeInt = NoFrame;
    clear();
    framePen.setColor(Qt::black);
    framePen.setWidth(1);
    frameBrush.setColor(Qt::white);
    frameBrush.setStyle(Qt::SolidPattern);

    setLayout(Container::Horizontal);
    setHorizontalDirection(Container::LeftToRight);

    setVisible(true);

    usage = Undefined;

    // Don't consider for sizes or repositioning
    // FIXME-2 still needed? overlay = true;
}

void FrameContainer::clear()
{
    switch (frameTypeInt) {
        case NoFrame:
            break;
        case Rectangle:
            delete rectFrame;
            break;
        case RoundedRectangle:
            delete pathFrame;
            break;
        case Ellipse:
        case Circle:
            delete ellipseFrame;
            break;
        case Cloud:
            delete pathFrame;
            break;
    }
    frameTypeInt = NoFrame;
    framePaddingInt = 0; // No frame requires also no padding
    frameXSize = 0; // FIXME-2 needed? Not in init() anyway...
}

void FrameContainer::repaint()
{
    // Repaint, when e.g. borderWidth has changed or a color
    switch (frameTypeInt) {
        case Rectangle:
            rectFrame->setPen(framePen);
            rectFrame->setBrush(frameBrush);
            break;
        case RoundedRectangle:
            pathFrame->setPen(framePen);
            pathFrame->setBrush(frameBrush);
            break;
        case Ellipse:
        case Circle:
            ellipseFrame->setPen(framePen);
            ellipseFrame->setBrush(frameBrush);
            break;
        case Cloud:
            pathFrame->setPen(framePen);
            pathFrame->setBrush(frameBrush);
            break;
        default:
            qWarning() << "FrameContainer::repaint  unknown frame type " << frameTypeInt;
            break;
    }
}

void FrameContainer::reposition()
{
    // qDebug() << "FC::reposition()";
    Container::reposition();    // FIXME-2 no need to overload without special functionality
}

FrameContainer::FrameType FrameContainer::frameType() { return frameTypeInt; }

FrameContainer::FrameType FrameContainer::frameTypeFromString(const QString &s)
{
    if (s == "Rectangle")
        return Rectangle;
    else if (s == "RoundedRectangle")
        return RoundedRectangle;
    else if (s == "Ellipse")
        return Ellipse;
    else if (s == "Circle")
        return Circle;
    else if (s == "Cloud")
        return Cloud;
    return NoFrame;
}

QString FrameContainer::frameTypeString()
{
    switch (frameTypeInt) {
    case Rectangle:
        return "Rectangle";
    case RoundedRectangle:
        return "RoundedRectangle";
    case Ellipse:
        return "Ellipse";
    case Circle:
        return "Circle";
    case Cloud:
        return "Cloud";
    case NoFrame:
        return "NoFrame";
    default:
        qWarning() << "FrameContainer::setFrameType  unknown frame type " << frameTypeInt;
        break;
    }
}

void FrameContainer::setFrameType(const FrameType &t)
{
    if (t != frameTypeInt) {
        clear();
        frameTypeInt = t;
        switch (frameTypeInt) {
            case NoFrame:
                break;
            case Rectangle:
                rectFrame = new QGraphicsRectItem;  // FIXME-3 Use my own Container rect!
                rectFrame->setPen(framePen);
                rectFrame->setBrush(frameBrush);
                rectFrame->setFlag(ItemStacksBehindParent, true);
                rectFrame->setParentItem(this);
                rectFrame->show();
                break;
            case Ellipse:
            case Circle:
                ellipseFrame = scene()->addEllipse(QRectF(0, 0, 0, 0),
                                                   framePen, frameBrush);
                ellipseFrame->setPen(framePen);
                ellipseFrame->setBrush(frameBrush);
                ellipseFrame->setFlag(ItemStacksBehindParent, true);
                ellipseFrame->setParentItem(this);
                ellipseFrame->show();
                break;
            case RoundedRectangle: {
                QPainterPath path;
                pathFrame = new QGraphicsPathItem;
                pathFrame->setPen(framePen);
                pathFrame->setBrush(frameBrush);
                pathFrame->setFlag(ItemStacksBehindParent, true);
                pathFrame->setParentItem(this);
                pathFrame->show();
            } break;
            case Cloud: {
                QPainterPath path;
                pathFrame = scene()->addPath(path, framePen, frameBrush);
                pathFrame->setPen(framePen);
                pathFrame->setBrush(frameBrush);
                pathFrame->setFlag(ItemStacksBehindParent, true);
                pathFrame->setParentItem(this);
                pathFrame->show();
            }
            break;
            default:
                qWarning() << "FrameContainer::setframeType  unknown frame type " << frameTypeInt;
                break;
        }
    }
    // reposition() is called in vymmodel for all containers
}

void FrameContainer::setFrameType(const QString &t)
{
    if (t == "Rectangle")
        setFrameType(Rectangle);
    else if (t == "RoundedRectangle")
        setFrameType(RoundedRectangle);
    else if (t == "Ellipse")
        setFrameType(Ellipse);
    else if (t == "Circle")
        setFrameType(Circle);
    else if (t == "Cloud")
        setFrameType(Cloud);
    else
        setFrameType(NoFrame);
}

QRectF FrameContainer::frameRect()
{
    return frameRectInt;
}

void FrameContainer::setFrameRect(const QRectF &frameSize)
{
    //qDebug() << "FC::setFrameRect t=" << frameTypeInt << " r=" << qrectFToString(frameSize, 0);
    frameRectInt = frameSize;
    switch (frameTypeInt) {
        case NoFrame:
            break;

        case Rectangle:
            rectFrame->setRect(frameRectInt);
            //qDebug() << "  FC  rect: " << rectFrame << "vis=" << rectFrame->isVisible();
            break;

        case RoundedRectangle: {
            QPointF tl = frameRectInt.topLeft();
            QPointF tr = frameRectInt.topRight();
            QPointF bl = frameRectInt.bottomLeft();
            QPointF br = frameRectInt.bottomRight();
            QPainterPath path;

            qreal n = 10;
            path.moveTo(tl.x() + n / 2, tl.y());

            // Top path
            path.lineTo(tr.x() - n, tr.y());
            path.arcTo(tr.x() - n, tr.y(), n, n, 90, -90);
            path.lineTo(br.x(), br.y() - n);
            path.arcTo(br.x() - n, br.y() - n, n, n, 0, -90);
            path.lineTo(bl.x() + n, br.y());
            path.arcTo(bl.x(), bl.y() - n, n, n, -90, -90);
            path.lineTo(tl.x(), tl.y() + n);
            path.arcTo(tl.x(), tl.y(), n, n, 180, -90);
            pathFrame->setPath(path);
        } break;
        case Ellipse:
            ellipseFrame->setRect(
                QRectF(frameRectInt.x(), frameRectInt.y(), frameRectInt.width(), frameRectInt.height()));
            frameXSize = 20; // max(frameRectInt.width(), frameRectInt.height()) / 4;
            break;
        case Circle: {
            qreal r = max(frameRectInt.width(), frameRectInt.height());
            ellipseFrame->setRect(
                QRectF(frameRectInt.x(), frameRectInt.y(), r, r));
            frameXSize = 20; // r / 4
            }
            break;
        case Cloud: {
            QPointF tl = frameRectInt.topLeft();
            QPointF tr = frameRectInt.topRight();
            QPointF bl = frameRectInt.bottomLeft();
            QPainterPath path;
            path.moveTo(tl);

            float w = frameRectInt.width();
            float h = frameRectInt.height();
            int n = w / 40;          // number of intervalls
            float d = w / n;         // width of interwall

            // Top path
            for (float i = 0; i < n; i++) {
                path.cubicTo(tl.x() + i * d, tl.y() - 100 * roof((i + 0.5) / n),
                             tl.x() + (i + 1) * d,
                             tl.y() - 100 * roof((i + 0.5) / n),
                             tl.x() + (i + 1) * d + 20 * roof((i + 1) / n),
                             tl.y() - 50 * roof((i + 1) / n));
            }
            // Right path
            n = h / 20;
            d = h / n;
            for (float i = 0; i < n; i++) {
                path.cubicTo(tr.x() + 100 * roof((i + 0.5) / n), tr.y() + i * d,
                             tr.x() + 100 * roof((i + 0.5) / n),
                             tr.y() + (i + 1) * d, tr.x() + 60 * roof((i + 1) / n),
                             tr.y() + (i + 1) * d);
            }
            n = w / 60;
            d = w / n;
            // Bottom path
            for (float i = n; i > 0; i--) {
                path.cubicTo(bl.x() + i * d, bl.y() + 100 * roof((i - 0.5) / n),
                             bl.x() + (i - 1) * d,
                             bl.y() + 100 * roof((i - 0.5) / n),
                             bl.x() + (i - 1) * d + 20 * roof((i - 1) / n),
                             bl.y() + 50 * roof((i - 1) / n));
            }
            // Left path
            n = h / 20;
            d = h / n;
            for (float i = n; i > 0; i--) {
                path.cubicTo(tl.x() - 100 * roof((i - 0.5) / n), tr.y() + i * d,
                             tl.x() - 100 * roof((i - 0.5) / n),
                             tr.y() + (i - 1) * d, tl.x() - 60 * roof((i - 1) / n),
                             tr.y() + (i - 1) * d);
            }
            pathFrame->setPath(path);
            frameXSize = 50;
        }
            break;
        default:
            qWarning() << "FrameContainer::setFrameRect  unknown frame type " << frameTypeInt;
            break;
    }
}

void FrameContainer::setFramePos(const QPointF &p)
{
    switch (frameTypeInt) {
        case NoFrame:
            break;
        case Rectangle:
            rectFrame->setPos(p);
            break;
        case RoundedRectangle:
            pathFrame->setPos(p);
            break;
        case Ellipse:
        case Circle:
            ellipseFrame->setPos(p);
            break;
        case Cloud:
            pathFrame->setPos(p);
            break;
        default:
            qWarning() << "FrameContainer::setFramePos unknown frame type " << frameTypeInt;
            break;
    }
}

int FrameContainer::framePadding()
{
    if (frameTypeInt == NoFrame)
        return 0;
    else
        return framePaddingInt;
}

void FrameContainer::setFramePadding(const int &i) { framePaddingInt = i; }  // FIXME-2 not supported yet

qreal FrameContainer::frameTotalPadding() { return frameXSize + framePaddingInt + framePen.width(); }

qreal FrameContainer::frameXPadding() { return frameXSize; }

int FrameContainer::framePenWidth() { return framePen.width(); }

void FrameContainer::setFramePenWidth(const int &i)
{
    framePen.setWidth(i);
    repaint();
}

QColor FrameContainer::framePenColor() { return framePen.color(); }

void FrameContainer::setFramePenColor(const QColor &col)
{
    framePen.setColor(col);
    repaint();
}

QColor FrameContainer::frameBrushColor() { return frameBrush.color(); }

void FrameContainer::setFrameBrushColor(const QColor &col)
{
    frameBrush.setColor(col);
    repaint();
}

void FrameContainer::setUsage(FrameUsage u)
{
    usage = u;
}

QString FrameContainer::saveFrame()
{
    QStringList attrList;
    if (usage == InnerFrame)
        attrList << attribut("frameUsage", "innerFrame");
    else if (usage == OuterFrame)
        attrList << attribut("frameUsage", "outerFrame");

    attrList <<  attribut("frameType", frameTypeString());

    if (frameTypeInt != NoFrame) {
        attrList <<  attribut("penColor", framePen.color().name(QColor::HexArgb));
        attrList <<  attribut("brushColor", frameBrush.color().name(QColor::HexArgb));
        attrList <<  attribut("padding", QString::number(framePaddingInt));
        attrList <<  attribut("penWidth", QString::number(framePen.width()));
    }

    return singleElement("frame", attrList.join(" "));
}

