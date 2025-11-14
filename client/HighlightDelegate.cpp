#include "HighlightDelegate.h"
#include <QPainter>
#include <QApplication>
#include <QStyle>
#include <QRegExp>
#include <QDebug>

HighlightDelegate::HighlightDelegate(const QString& pattern, const QColor& color, bool bold, QObject* parent)
    : QStyledItemDelegate(parent)
    , pattern_(pattern)
    , color_(color)
    , bold_(bold)
{
}

void HighlightDelegate::setPattern(const QString& pattern)
{
    pattern_ = pattern;
}

void HighlightDelegate::setHighlightColor(const QColor& color)
{
    color_ = color;
}

void HighlightDelegate::setHighlightBold(bool bold)
{
    bold_ = bold;
}

void HighlightDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (pattern_.isEmpty()) {
        // 没有关键字时使用默认绘制
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    
    painter->save();
    
    // 绘制背景
    if (opt.state & QStyle::State_Selected) {
        painter->fillRect(opt.rect, opt.palette.highlight());
    } else {
        painter->fillRect(opt.rect, opt.palette.base());
    }
    
    // 绘制文本（带高亮）
    drawHighlightedText(painter, opt.rect, opt.text, pattern_, color_, bold_, opt.font, opt.palette, opt.state);
    
    painter->restore();
}

void HighlightDelegate::drawHighlightedText(QPainter* painter, const QRect& rect, const QString& text, 
                                          const QString& pattern, const QColor& color, bool bold,
                                          const QFont& font, const QPalette& palette, QStyle::State state) const
{
    if (text.isEmpty()) return;
    
    bool isSelected = state & QStyle::State_Selected;
    QColor normalColor = isSelected ? palette.highlightedText().color() : palette.text().color();
    
    QFont normalFont = font;
    QFont highlightFont = font;
    if (bold) {
        highlightFont.setBold(true);
    }
    
    QFontMetrics fmNormal(normalFont);
    QFontMetrics fmHighlight(highlightFont);
    
    // 判断是否是通配符模式（包含 * 或 ?）
    bool isWildcard = pattern.contains('*') || pattern.contains('?');
    
    int lastPos = 0;
    
    painter->setFont(normalFont);
    painter->setPen(normalColor);
    
    if (isWildcard) {
        // 通配符模式：使用 QRegExp 进行匹配
        QRegExp regex(pattern, Qt::CaseInsensitive, QRegExp::Wildcard);
        int pos = 0;
        
        while ((pos = regex.indexIn(text, pos)) != -1) {
            int length = regex.matchedLength();
            
            // 绘制匹配前的文本
            if (pos > lastPos) {
                QString before = text.mid(lastPos, pos - lastPos);
                int beforeWidth = fmNormal.width(before);
                QRect beforeRect = rect.adjusted(fmNormal.width(text.left(lastPos)), 0, 0, 0);
                beforeRect.setWidth(beforeWidth);
                painter->drawText(beforeRect, Qt::AlignLeft | Qt::AlignVCenter, before);
            }
            
            // 绘制高亮文本
            QString highlightText = text.mid(pos, length);
            int highlightWidth = fmHighlight.width(highlightText);
            QRect highlightRect = rect.adjusted(fmNormal.width(text.left(pos)), 0, 0, 0);
            highlightRect.setWidth(highlightWidth);
            
            painter->setFont(highlightFont);
            painter->setPen(color);
            painter->drawText(highlightRect, Qt::AlignLeft | Qt::AlignVCenter, highlightText);
            
            pos += length;
            lastPos = pos;
            
            // 恢复普通字体
            painter->setFont(normalFont);
            painter->setPen(normalColor);
        }
    } else {
        // 普通文本模式：直接搜索所有出现的位置
        QString searchText = pattern;
        int pos = 0;
        
        while ((pos = text.indexOf(searchText, pos, Qt::CaseInsensitive)) != -1) {
            int length = searchText.length();
            
            // 绘制匹配前的文本
            if (pos > lastPos) {
                QString before = text.mid(lastPos, pos - lastPos);
                int beforeWidth = fmNormal.width(before);
                QRect beforeRect = rect.adjusted(fmNormal.width(text.left(lastPos)), 0, 0, 0);
                beforeRect.setWidth(beforeWidth);
                painter->drawText(beforeRect, Qt::AlignLeft | Qt::AlignVCenter, before);
            }
            
            // 绘制高亮文本
            QString highlightText = text.mid(pos, length);
            int highlightWidth = fmHighlight.width(highlightText);
            QRect highlightRect = rect.adjusted(fmNormal.width(text.left(pos)), 0, 0, 0);
            highlightRect.setWidth(highlightWidth);
            
            painter->setFont(highlightFont);
            painter->setPen(color);
            
            painter->drawText(highlightRect, Qt::AlignLeft | Qt::AlignVCenter, highlightText);
            
            pos += length;
            lastPos = pos;
            
            // 恢复普通字体
            painter->setFont(normalFont);
            painter->setPen(normalColor);
        }
    }
    
    // 绘制剩余的文本
    if (lastPos < text.length()) {
        QString after = text.mid(lastPos);
        int afterWidth = fmNormal.width(after);
        QRect afterRect = rect.adjusted(fmNormal.width(text.left(lastPos)), 0, 0, 0);
        afterRect.setWidth(afterWidth);
        painter->drawText(afterRect, Qt::AlignLeft | Qt::AlignVCenter, after);
    }
    
    // 如果没有匹配到任何内容，绘制整个文本
    if (lastPos == 0) {
        painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, text);
    }
}