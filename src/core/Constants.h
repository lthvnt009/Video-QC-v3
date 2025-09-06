// src/core/Constants.h
#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <QString>

/**
 * @brief Namespace chứa các hằng số toàn cục cho ứng dụng.
 * Việc tập trung các hằng số vào một nơi giúp dễ dàng quản lý,
 * tránh lỗi chính tả và đảm bảo tính nhất quán trên toàn bộ dự án.
 */
namespace AppConstants {

// Thông tin ứng dụng, sử dụng cho QSettings
constexpr const char* ORG_NAME = "MyCompany";
constexpr const char* APP_NAME = "VideoQCTool_ControlPanel";

// CẢI TIẾN: Thêm khóa mới cho đường dẫn qcli.exe
// Khóa cài đặt -> Đường dẫn
constexpr const char* K_QCTOOLS_PATH = "qctoolsPath";
constexpr const char* K_QCCLI_PATH = "qcliPath";


// Khóa cài đặt -> Cấu hình phát hiện lỗi
constexpr const char* K_DETECT_BLACK_FRAMES = "detectBlackFrames";
constexpr const char* K_DETECT_BLACK_BORDERS = "detectBlackBorders";
constexpr const char* K_DETECT_ORPHAN_FRAMES = "detectOrphanFrames";
constexpr const char* K_BLACK_FRAME_THRESH = "blackFrameThreshold";
constexpr const char* K_BORDER_THRESH = "borderThreshold";
constexpr const char* K_ORPHAN_THRESH = "orphanThreshold";
constexpr const char* K_SCENE_THRESH = "sceneThreshold";
constexpr const char* K_HAS_TRANSITIONS = "hasTransitions";

// Khóa cài đặt -> Cài đặt nâng cao
constexpr const char* K_USE_HW_ACCEL = "useHwAccel";
constexpr const char* K_HW_ACCEL_TYPE = "hwAccelType";

// Các loại lỗi (dùng để hiển thị và lọc)
const QString ERR_BLACK_FRAME = QStringLiteral("Frame Đen");
const QString ERR_BLACK_BORDER = QStringLiteral("Viền Đen");
const QString ERR_ORPHAN_FRAME = QStringLiteral("Frame Dư");

// Các "tag" nội bộ dùng để đánh dấu frame
const QString TAG_IS_BLACK = QStringLiteral("IS_BLACK");
const QString TAG_HAS_BORDER = QStringLiteral("HAS_BORDER");
const QString TAG_IS_SCENE_CUT = QStringLiteral("IS_SCENE_CUT");

} // namespace AppConstants

#endif // CONSTANTS_H
