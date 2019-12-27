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

#ifndef TOCBOX_H
#define TOCBOX_H

#include <QWidget>
#include <QMenu>
#include <QVBoxLayout>
#include <QDomDocument>
#include <QGuiApplication>
#include <QComboBox>
#include "pdfdoc.h"
#include "tocbutton.h"
#include "tocaction.h"

class TocBox : public QWidget
{
    Q_OBJECT

private:
    QStringList const indentStrings = {"  ", "    ➤ ", "       - ", "          + "};
    QDomDocument const * toc = nullptr;
    quint8 unfoldLevel = 2;
    QVBoxLayout* layout;
    QList<TocButton*> buttons;
    QList<QMenu*> menus;
    QMap<int, int> page_to_button;
    void recursiveTocCreator(QDomNode const& node, quint8 const level);
    bool need_update = true;
    PdfDoc const* pdf = nullptr; // TODO: use this, send pages numbers instead of strings

public:
    TocBox(QWidget* parent = nullptr);
    ~TocBox();
    void setPdf(PdfDoc const* doc) {pdf=doc;}
    void createToc(QDomDocument const* toc);
    void setUnfoldLevel(quint8 const level);
    bool needUpdate() {return need_update;}
    void setOutdated() {need_update=true;}
    bool hasToc() {return toc!=nullptr;}
    void focusCurrent(int const page);

signals:
    //void sendDest(QString const & dest);
    void sendNewPage(int const page);
};

#endif // TOCBOX_H
