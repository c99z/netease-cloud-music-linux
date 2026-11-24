#pragma once

#include <QAbstractListModel>
#include <QSet>
#include <QtQml/qqmlregistration.h>

// A song list exposed to QML as a real model, so views only create the visible rows and
// appending a page or toggling a heart updates rows in place instead of rebuilding all.
class TrackModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Supplied by Library")
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)

public:
    enum Role { DataRole = Qt::UserRole + 1, LikedRole, IdRole };

    explicit TrackModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return int(m_visible.size()); }
    QString filter() const { return m_filter; }
    void setFilter(const QString &filter);

    // Full list (unfiltered) in source order.
    const QVariantList &tracks() const { return m_tracks; }
    void setTracks(const QVariantList &tracks);
    void append(const QVariantList &tracks);
    void removeId(const QString &id);
    void setLikedIds(const QSet<QString> *liked);
    void likesChanged(const QString &id = {});

    // Rows as currently shown (after filtering), for "play this list from row N".
    Q_INVOKABLE QVariantList visibleTracks() const;
    Q_INVOKABLE QVariantMap at(int row) const;

signals:
    void countChanged();
    void filterChanged();

private:
    bool matches(const QVariant &track) const;
    void rebuild();

    QVariantList m_tracks;
    QList<int> m_visible;     // indexes into m_tracks
    QString m_filter;
    const QSet<QString> *m_liked = nullptr;
};
