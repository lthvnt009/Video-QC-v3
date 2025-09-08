#ifndef CLICKABLEHEADERVIEW_H
#define CLICKABLEHEADERVIEW_H

#include <QHeaderView>

// Lớp Header View tùy chỉnh để xử lý sự kiện click và vẽ thêm biểu tượng
class ClickableHeaderView : public QHeaderView
{
    Q_OBJECT

public:
    explicit ClickableHeaderView(Qt::Orientation orientation, QWidget *parent = nullptr);

signals:
    // Tín hiệu được phát ra khi một section header được click, kèm theo vị trí click
    void sectionClickedWithPos(int logicalIndex, const QPoint &pos);

protected:
    // Ghi đè sự kiện nhấn chuột để phát ra tín hiệu tùy chỉnh
    void mousePressEvent(QMouseEvent *e) override;
    
    // Ghi đè sự kiện vẽ để thêm biểu tượng mũi tên
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;

private:
    // Hàm nội bộ để tính toán vùng chữ nhật của biểu tượng mũi tên
    QRect getArrowRect(const QRect &rect, int logicalIndex) const;
};

#endif // CLICKABLEHEADERVIEW_H

