// src/core/Constants.h
#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <QString>

namespace AppConstants {

// Application Info, used for QSettings
constexpr const char* ORG_NAME = "MyCompany";
constexpr const char* APP_NAME = "VideoQCTool_ControlPanel";

// Settings Keys -> Paths
constexpr const char* K_QCTOOLS_PATH = "qctoolsPath";
constexpr const char* K_QCCLI_PATH = "qcliPath";

// Settings Keys -> Error Detection Config
constexpr const char* K_DETECT_BLACK_FRAMES = "detectBlackFrames";
constexpr const char* K_DETECT_BLACK_BORDERS = "detectBlackBorders";
constexpr const char* K_DETECT_ORPHAN_FRAMES = "detectOrphanFrames";
constexpr const char* K_BLACK_FRAME_THRESH = "blackFrameThreshold";
constexpr const char* K_BORDER_THRESH = "borderThreshold";
constexpr const char* K_ORPHAN_THRESH = "orphanThreshold";
constexpr const char* K_SCENE_THRESH = "sceneThreshold";
constexpr const char* K_HAS_TRANSITIONS = "hasTransitions";
constexpr const char* K_REWIND_FRAMES = "rewindFrames";

// Settings Keys -> Advanced
constexpr const char* K_USE_HW_ACCEL = "useHwAccel";
constexpr const char* K_HW_ACCEL_TYPE = "hwAccelType";

// Error Types (for display and filtering)
const QString ERR_BLACK_FRAME = QStringLiteral("Frame Đen");
const QString ERR_BLACK_BORDER = QStringLiteral("Viền Đen");
const QString ERR_ORPHAN_FRAME = QStringLiteral("Frame Dư");

// Internal "tags" for marking frames
const QString TAG_IS_BLACK = QStringLiteral("IS_BLACK");
const QString TAG_HAS_BORDER = QStringLiteral("HAS_BORDER");
const QString TAG_IS_SCENE_CUT = QStringLiteral("IS_SCENE_CUT");

} // namespace AppConstants

#endif // CONSTANTS_H

