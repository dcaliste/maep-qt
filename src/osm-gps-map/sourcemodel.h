/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * Copyright (C) 2017 Damien Caliste <dcaliste@free.fr>
 *
 * This file is part of Maep.
 *
 * Maep is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Maep is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Maep.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SOURCEMODEL_H
#define SOURCEMODEL_H

#include "source.h"

#include <QtCore/QAbstractListModel>
#include <QSortFilterProxyModel>

namespace Maep {

class SourceModel;

class SourceModelFilter : public QSortFilterProxyModel
{
    Q_OBJECT

 public:
    SourceModelFilter(QObject *parent = 0);
    ~SourceModelFilter();

 private:
     bool filterAcceptsRow(int row, const QModelIndex &source_parent) const;
};

class SourceModel : public QAbstractListModel
{
    Q_OBJECT

    Q_ENUMS(SourceId)
    Q_ENUMS(SectionId)

public:
    enum SourceModelRoles {
        Id = Qt::UserRole + 1,
        Label,
        CopyrightNotice,
        CopyrightUrl,
        Section,
        Enabled
    };

    enum SourceId {
        SOURCE_NULL,
        SOURCE_OPENSTREETMAP,
        SOURCE_OPENSTREETMAP_RENDERER,
        SOURCE_OPENAERIALMAP,
        SOURCE_MAPS_FOR_FREE,
        SOURCE_OPENCYCLEMAP,
        SOURCE_OSM_PUBLIC_TRANSPORT,
        SOURCE_GOOGLE_STREET,
        SOURCE_GOOGLE_SATELLITE,
        SOURCE_GOOGLE_HYBRID,
        SOURCE_VIRTUAL_EARTH_STREET,
        SOURCE_VIRTUAL_EARTH_SATELLITE,
        SOURCE_VIRTUAL_EARTH_HYBRID,
        SOURCE_YAHOO_STREET,
        SOURCE_YAHOO_SATELLITE,
        SOURCE_YAHOO_HYBRID,
        SOURCE_OSMC_TRAILS,
        SOURCE_OPENSEAMAP,
        SOURCE_GOOGLE_TRAFFIC,
        SOURCE_MML_PERUSKARTTA,
        SOURCE_MML_ORTOKUVA,
        SOURCE_MML_TAUSTAKARTTA,

        SOURCE_LAST
    };

    enum SectionId {
        SECTION_BASE,
        SECTION_OVERLAY
    };

    explicit SourceModel(QObject *parent = 0);
    virtual ~SourceModel();

    QVariant data(const QModelIndex &index, int role) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Enabled);
    int rowCount(const QModelIndex &parent) const;
    QHash<int, QByteArray> roleNames() const;

    Q_INVOKABLE void addPreset(SourceId id, SectionId section);

private:
    friend SourceModelFilter;

    QHash<int, QByteArray> roles;

    struct Source {
        const MaepSource *source;
        bool enabled;
        SectionId section;

    Source(const MaepSource *source, bool enabled = true, SectionId section = SECTION_BASE) : source(source), enabled(enabled), section(section)
        {
        }
        
        bool operator==(const Source &other) const
        {
            return (source == other.source);
        }
        bool operator>(const Source &other) const
        {
            return (section > other.section) ||
                ((section == other.section) &&
                 (maep_source_get_id(source) > maep_source_get_id(other.source)));
        }
    };

    MaepSourceManager *manager;
    QList<Source> sources;
};

}

#endif // SOURCEMODEL_H
