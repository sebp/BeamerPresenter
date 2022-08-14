// SPDX-FileCopyrightText: 2022 Valentin Bruch <software@vbruch.eu>
// SPDX-License-Identifier: GPL-3.0-or-later OR AGPL-3.0-or-later

#ifndef LINEGRAPHICSITEM_H
#define LINEGRAPHICSITEM_H

#include <QPointF>
#include <QLineF>
#include <QGraphicsLineItem>
#include "src/drawing/drawtool.h"
#include "src/drawing/basicgraphicspath.h"

class QWidget;
class QPainter;
class QStyleOptionGraphicsItem;

/**
 * @brief LineGraphicsItem: QGraphicsLineItem with a tool that can be converted to BasicGraphicsPath
 */
class LineGraphicsItem : public QGraphicsLineItem
{
    /// DrawTool for this path.
    const DrawTool tool;

public:
    /// QGraphicsItem type for this subclass
    enum {Type = UserType + 8};

    /// Constructor for initializing QGraphicsLineItem
    /// @param pos origin of the rectangle. This coordinate is always fixed.
    LineGraphicsItem(const DrawTool &tool, const QPointF &pos, QGraphicsItem *parent = NULL) :
        QGraphicsLineItem(QLineF(pos, pos), parent),
        tool(tool)
    {setPen(tool.pen());}

    /// Trivial destructor.
    ~LineGraphicsItem() {}

    /// @return custom QGraphicsItem type.
    int type() const noexcept override
    {return Type;}

    /// Change the flexible coordinate of the line.
    void setSecondPoint(const QPointF &pos);

    /// Convert to a BasicGraphicsPath for simpler erasing.
    BasicGraphicsPath *toPath() const;

    /// Paint line to painter.
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = NULL) override;
};

#endif // LINEGRAPHICSITEM_H
