#include "TrackModel.h"

#include <climits>

TrackModel::TrackModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TrackModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_visible.size());
}

QVariant TrackModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_visible.size())
        return {};
    const auto &track = m_tracks.at(m_visible.at(index.row()));
    switch (role) {
    case DataRole:
        return track;
    case IdRole:
        return track.toMap().value("id");
    case LikedRole:
        return m_liked && m_liked->contains(track.toMap().value("id").toString());
    }
    return {};
}

QHash<int, QByteArray> TrackModel::roleNames() const
{
    return {{DataRole, "modelData"}, {LikedRole, "liked"}, {IdRole, "trackId"}};
}

bool TrackModel::matches(const QVariant &value) const
{
    if (m_filter.isEmpty())
        return true;
    const auto track = value.toMap();
    const QString haystack = track.value("title").toString() + u' ' + track.value("artist").toString()
                             + u' ' + track.value("album").toString();
    return haystack.contains(m_filter, Qt::CaseInsensitive);
}

void TrackModel::rebuild()
{
    beginResetModel();
    m_visible.clear();
    for (int i = 0; i < m_tracks.size(); ++i) {
        if (matches(m_tracks.at(i)))
            m_visible.append(i);
    }
    endResetModel();
    emit countChanged();
}

void TrackModel::setFilter(const QString &filter)
{
    const auto cleaned = filter.trimmed();
    if (cleaned == m_filter)
        return;
    m_filter = cleaned;
    emit filterChanged();
    // Apply as row removals/insertions rather than a reset: a reset recreates the view's
    // header, which would take keyboard focus away from the search field inside it.
    QList<int> wanted;
    for (int i = 0; i < m_tracks.size(); ++i) {
        if (matches(m_tracks.at(i)))
            wanted.append(i);
    }
    int row = 0;
    int next = 0;
    while (row < m_visible.size() || next < wanted.size()) {
        const int have = row < m_visible.size() ? m_visible.at(row) : INT_MAX;
        const int want = next < wanted.size() ? wanted.at(next) : INT_MAX;
        if (have == want) {
            ++row;
            ++next;
        } else if (have < want) {
            int end = row;
            while (end + 1 < m_visible.size() && m_visible.at(end + 1) < want)
                ++end;
            beginRemoveRows({}, row, end);
            m_visible.remove(row, end - row + 1);
            endRemoveRows();
        } else {
            int last = next;
            while (last + 1 < wanted.size() && wanted.at(last + 1) < have)
                ++last;
            beginInsertRows({}, row, row + last - next);
            for (int k = next; k <= last; ++k)
                m_visible.insert(row + k - next, wanted.at(k));
            endInsertRows();
            row += last - next + 1;
            next = last + 1;
        }
    }
    emit countChanged();
}

void TrackModel::setTracks(const QVariantList &tracks)
{
    m_tracks = tracks;
    rebuild();
}

void TrackModel::append(const QVariantList &tracks)
{
    if (tracks.isEmpty())
        return;
    const int base = int(m_tracks.size());
    m_tracks.append(tracks);
    QList<int> added;
    for (int i = base; i < m_tracks.size(); ++i) {
        if (matches(m_tracks.at(i)))
            added.append(i);
    }
    if (added.isEmpty())
        return;
    const int first = int(m_visible.size());
    beginInsertRows({}, first, first + int(added.size()) - 1);
    m_visible.append(added);
    endInsertRows();
    emit countChanged();
}

void TrackModel::removeId(const QString &id)
{
    for (int source = 0; source < m_tracks.size(); ++source) {
        if (m_tracks.at(source).toMap().value("id").toString() != id)
            continue;
        const int row = int(m_visible.indexOf(source));
        if (row >= 0)
            beginRemoveRows({}, row, row);
        m_tracks.removeAt(source);
        if (row >= 0)
            m_visible.removeAt(row);
        for (auto &index : m_visible) {
            if (index > source)
                --index;
        }
        if (row >= 0) {
            endRemoveRows();
            emit countChanged();
        }
        return;
    }
}

void TrackModel::setLikedIds(const QSet<QString> *liked)
{
    m_liked = liked;
}

void TrackModel::likesChanged(const QString &id)
{
    if (m_visible.isEmpty())
        return;
    if (id.isEmpty()) {
        emit dataChanged(index(0), index(int(m_visible.size()) - 1), {LikedRole});
        return;
    }
    for (int row = 0; row < m_visible.size(); ++row) {
        if (m_tracks.at(m_visible.at(row)).toMap().value("id").toString() == id)
            emit dataChanged(index(row), index(row), {LikedRole});
    }
}

QVariantList TrackModel::visibleTracks() const
{
    if (m_filter.isEmpty())
        return m_tracks;
    QVariantList result;
    result.reserve(m_visible.size());
    for (const int index : m_visible)
        result.append(m_tracks.at(index));
    return result;
}

QVariantMap TrackModel::at(int row) const
{
    return row >= 0 && row < m_visible.size() ? m_tracks.at(m_visible.at(row)).toMap() : QVariantMap{};
}
