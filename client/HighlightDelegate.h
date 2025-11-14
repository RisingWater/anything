#ifndef HIGHLIGHTDELEGATE_H
#define HIGHLIGHTDELEGATE_H

#include <QStyledItemDelegate>
#include <QColor>

class HighlightDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit HighlightDelegate(const QString& pattern = "", 
                              const QColor& color = Qt::red, 
                              bool bold = true, 
                              QObject* parent = nullptr);
    
    void setPattern(const QString& pattern);
    void setHighlightColor(const QColor& color);
    void setHighlightBold(bool bold);
    
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    void drawHighlightedText(QPainter* painter, const QRect& rect, const QString& text, 
                            const QString& pattern, const QColor& color, bool bold,
                            const QFont& font, const QPalette& palette, QStyle::State state) const;

private:
    QString pattern_;
    QColor color_;
    bool bold_;
};

#endif // HIGHLIGHTDELEGATE_H