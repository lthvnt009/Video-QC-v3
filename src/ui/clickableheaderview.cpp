#include "clickableheaderview.h"
#include <QMouseEvent>
#include <QPainter>

ClickableHeaderView::ClickableHeaderView(Qt::Orientation orientation, QWidget *parent)
    : QHeaderView(orientation, parent)
{
}

// Hàm mới để tính toán vùng chữ nhật của mũi tên
QRect ClickableHeaderView::getArrowRect(const QRect &rect, int logicalIndex) const
{
    if (logicalIndex != 0) {
        return QRect(); // Trả về hình chữ nhật rỗng cho các cột khác
    }
    // Định nghĩa kích thước của vùng có thể click cho mũi tên
    const int arrowWidth = 15; // Tăng độ rộng để dễ click hơn
    const int arrowHeight = rect.height();
    // Đặt nó ở cuối bên phải của ô, căn giữa theo chiều dọc
    return QRect(rect.right() - arrowWidth,
                 rect.y(),
                 arrowWidth,
                 arrowHeight);
}


void ClickableHeaderView::mousePressEvent(QMouseEvent *e)
{
    int logicalIndex = logicalIndexAt(e->pos());
    if (logicalIndex != -1) {
        // SỬA LỖI: QHeaderView không có sectionRect.
        // Lấy vị trí và kích thước riêng lẻ để tạo hình chữ nhật.
        int sectionPos = sectionPosition(logicalIndex);
        int sectionW = sectionSize(logicalIndex);
        QRect sectionRect(sectionPos, 0, sectionW, height());
        
        QRect arrowRect = getArrowRect(sectionRect, logicalIndex);

        // Kiểm tra xem cú click có nằm trong vùng chữ nhật của mũi tên không
        if (arrowRect.contains(e->pos())) {
            emit sectionClickedWithPos(logicalIndex, mapToGlobal(e->pos()));
            // "Nuốt" sự kiện click này để ngăn các hành động mặc định khác (như sắp xếp)
            return;
        }
    }

    // Nếu cú click không nằm trên mũi tên, chuyển nó cho lớp cơ sở
    // để xử lý các hành động thông thường (như thay đổi kích thước cột).
    QHeaderView::mousePressEvent(e);
}

void ClickableHeaderView::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const
{
    painter->save();
    // Để lớp cơ sở vẽ mọi thứ (nền, văn bản, v.v.)
    QHeaderView::paintSection(painter, rect, logicalIndex);
    painter->restore();

    // Nếu đây là cột đầu tiên, vẽ thêm biểu tượng mũi tên
    if (logicalIndex == 0)
    {
        QRect arrowRect = getArrowRect(rect, logicalIndex);
        // Vẽ mũi tên căn giữa trong vùng chữ nhật của nó
        painter->drawText(arrowRect, Qt::AlignCenter, "▼");
    }
}

