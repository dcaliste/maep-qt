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

#include "sourcemodel.h"

#include "../conf.h"
#define MAEP_CONF_KEY_LIST       "source-list"

Maep::SourceModel::SourceModel(QObject *parent)
: QAbstractListModel(parent), manager(maep_source_manager_get_instance())
{
    guint *values;
    gsize i, ln;
    
    roles.insert(Id, "sourceId");
    roles.insert(Label, "label");
    roles.insert(CopyrightNotice, "copyrightNotice");
    roles.insert(CopyrightUrl, "copyrightUrl");
    roles.insert(Section, "section");
    roles.insert(Enabled, "enabled");
    roles.insert(Active, "active");

    values = maep_conf_get_uint_list(MAEP_CONF_KEY_LIST, &ln);
    if (values)
        for (i = 0; i < ln; i++) {
            g_message("Source list contains %d.", values[i]);
            confList.append(values[i]);
        }
    g_free(values);
}

Maep::SourceModel::~SourceModel()
{
    guint *ids;
    int i, j;

    ids = static_cast<guint*>(g_malloc(sizeof(guint) * sources.length()));
    for (i = 0, j = 0; i < sources.length(); i++)
        if (sources.at(i).enabled)
            ids[j++] = maep_source_get_id(sources.at(i).source);
    maep_conf_set_uint_list(MAEP_CONF_KEY_LIST, ids, j);
    g_free(ids);
}

QHash<int, QByteArray> Maep::SourceModel::roleNames() const
{
    return roles;
}

void Maep::SourceModel::addPreset(Maep::SourceModel::SourceId id,
                                  Maep::SourceModel::SectionId section)
{
    Maep::SourceModel::Source source(maep_source_manager_getById(manager, guint(id)),
                                     (confList.isEmpty() ||
                                      confList.contains(guint(id))), section);
    if (sources.contains(source))
        return;

    // Insert id sorted.
    int i;
    for (i = 0; i < sources.count(); i++) {
        if (sources.at(i) > source)
            break;
    }
    beginInsertRows(QModelIndex(), i, i);
    sources.insert(i, source);
    endInsertRows();
}

QVariant Maep::SourceModel::data(const QModelIndex& index, int role) const
{
    QVariant result;
    if (index.isValid()) {
        int row = index.row();
        if (row > -1 && row < sources.count()) {
            switch(role)
            {
            case Id:
              result.setValue<int>(maep_source_get_id(sources.at(row).source));
              break;
            case Label:
              result.setValue<QString>(maep_source_get_friendly_name(sources.at(row).source));
              break;
            case CopyrightNotice:
              const gchar *notice;
              maep_source_get_repo_copyright(sources.at(row).source, &notice, NULL);
              result.setValue<QString>(notice);
              break;
            case CopyrightUrl:
              const gchar *url;
              maep_source_get_repo_copyright(sources.at(row).source, NULL, &url);
              result.setValue<QString>(url);
              break;
            case Active:
              result.setValue<bool>(maep_source_is_valid(sources.at(row).source));
              break;
            case Enabled:
              result.setValue<bool>(sources.at(row).enabled);
              break;
            case Section:
                result.setValue<int>(int(sources.at(row).section));
              break;
            default:
                result.setValue<QString>(QString("Unknown role: %1").arg(role));
                break;
            }
        }
    }
    return result;
}

bool Maep::SourceModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Maep::SourceModel::Enabled)
        return false;
    if (!index.isValid())
        return false;

    int row = index.row();
    if (row < 0 || row >= sources.count())
        return false;

    bool enabled(value.toBool());
    if (enabled == sources.at(row).enabled)
        return false;

    sources[row].enabled = enabled;
    emit dataChanged(index, index, QVector<int>() << int(Maep::SourceModel::Enabled));

    return true;
}

int Maep::SourceModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return sources.count();
}

Maep::SourceModelFilter::SourceModelFilter(QObject *parent): QSortFilterProxyModel(parent)
{
    setFilterRole(Maep::SourceModel::Enabled);
}

Maep::SourceModelFilter::~SourceModelFilter()
{
}

bool Maep::SourceModelFilter::filterAcceptsRow(int row, const QModelIndex &source_parent) const
{
    Q_UNUSED(source_parent);
    SourceModel *model = static_cast<Maep::SourceModel*>(QAbstractProxyModel::sourceModel());

    if (!model || row < 0 || row >= model->sources.count())
        return false;

    return model->sources.at(row).enabled;
}
