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

#ifndef TOCACTION_H
#define TOCACTION_H

#include <QtDebug>
#include <QAction>

/// Action in a drop down menu.
/// These are the labels shown in a drop down menu of the table of contents.
class TocAction : public QAction
{
    Q_OBJECT

private:
    int dest;

public:
    TocAction(QString const& prefix = "", QString const& text = "", int const dest = 0, QWidget* parent = nullptr);

signals:
    void activated(int const dest);
};

#endif // TOCACTION_H
