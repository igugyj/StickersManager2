#ifndef HIGHLIGHTMANAGER_H
#define HIGHLIGHTMANAGER_H

#include <QString>
#include <QMap>

class StickerCell;

class HighlightManager {
public:
    explicit HighlightManager(QMap<QString, StickerCell *> &cellMap)
        : m_cellMap(cellMap) {}

    void setHighlightedPath(const QString &path) {
        qDebug() << "[HLMGR] setHighlightedPath:" << path << "old:" << m_highlightedPath;
        if (m_highlightedPath == path) return;
        m_highlightedPath = path;
        refresh();
    }

    void clearHighlight() {
        m_highlightedPath.clear();
        refresh();
    }

    QString highlightedPath() const { return m_highlightedPath; }

    void refresh() {
        qDebug() << "[HLMGR] refresh:" << m_highlightedPath << "cellMap size" << m_cellMap.size();
        for (auto it = m_cellMap.begin(); it != m_cellMap.end(); ++it) {
            bool should = (it.key() == m_highlightedPath);
            if (should) qDebug() << "[HLMGR]   HIGHLIGHT:" << it.key();
            it.value()->setHighlighted(should);
        }
    }

private:
    QString m_highlightedPath;
    QMap<QString, StickerCell *> &m_cellMap;
};

#endif // HIGHLIGHTMANAGER_H