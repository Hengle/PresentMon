#ifndef _COLOR_PALETTE_H_
#define _COLOR_PALETTE_H_

#include <QtGui/QPainter>
#define PALETTE_BLUE_HUE 1

// Green hue
#ifdef GREEN_HUE
const QColor kAvgFpsColor = QColor::fromRgb(0x2d, 0x5e, 0x9e);
const QColor kActualFlipColor = QColor::fromRgb(0x2d, 0x5e, 0x9e);
const QColor kFrameTimeColor = QColor::fromRgb(0xdc, 0xec, 0xc9);
const QColor kDisplayQueueTimeColor = QColor::fromRgb(0xb3, 0xdd, 0xcc);
const QColor kGpuTimeColor = QColor::fromRgb(0x8a, 0xcd, 0xce);
const QColor kDriverTimeColor = QColor::fromRgb(0x3d, 0x91, 0xbe);
const QColor kPresentApiTimeColor = QColor::fromRgb(0x3d, 0x91, 0xbe);
const QColor kSimStartColor = QColor::fromRgb(0x2d, 0x5e, 0x9e);
#endif  // GREEN_HUE

#ifdef WARM_HUE
// Warm hue
const QColor kAnimationErrorColor = QColor::fromRgb(0xa6, 0x46, 0x29);
const QColor kAvgFpsColor = QColor::fromRgb(0xe7, 0x6f, 0x34);
const QColor kMovingAvgFpsColor = QColor::fromRgb(0x9d, 0x44, 0x29);
const QColor kActualFlipColor = QColor::fromRgb(0xe2, 0x53, 0x28);
const QColor kFrameTimeColor = QColor::fromRgb(0xfd, 0xed, 0x86);
const QColor kDisplayQueueTimeColor = QColor::fromRgb(0xfc, 0xe3, 0x6b);
const QColor kGpuTimeColor = QColor::fromRgb(0xf7, 0xc6, 0x5d);
const QColor kDriverTimeColor = QColor::fromRgb(0xf1, 0xa8, 0x4f);
const QColor kPresentApiTimeColor = QColor::fromRgb(0xec, 0x8c, 0x41);
const QColor kSimStartColor = QColor::fromRgb(0x4c, 0x34, 0x30);
#endif  // WARM_HUE

// Blue Hue
#ifdef BLUE_HUE
// #290c5c
// # 600063
// # 8f0062
// #b8015b
// #d82d4e
// #f0543d
// #fd7d26
// #ffa600
const QColor kAnimationErrorColor("#ffa600");
const QColor kAvgFpsColor("#de425b");
const QColor k99FpsColor("#5a4d73");
const QColor k95FpsColor("#ab98b6");
const QColor k90FpsColor("#ffeaff");
const QColor k1FpsColor("#f6b5d9");
const QColor kMovingAvgFpsColor("#050f37");
const QColor kFpsColor("#f07da1");
const QColor kActualFlipColor("#fd7d26");
const QColor kFrameTimeColor("#ffedd0");
const QColor kDisplayQueueTimeColor("#d82d4e");
const QColor kGpuTimeColor("#b8015b");
const QColor kDriverTimeColor("#8f0062");
const QColor kPresentApiTimeColor("#c0290c5c");
const QColor kAppTimeColor("#c0600063");
const QColor kSimStartColor("#290c5c");
const QColor kClipLineColor("#f0543d");
const QColor kClipLineHoverColor("#7bdeff");
#endif  // BLUE_HUE

// Divergent Blue
#ifdef DIVERGENT_BLUE_HUE
//# 0054ae
//# 4b6bba
//# 7184c5
//# 939ed0
//#b3b9db
//#d2d4e6
//#f1f1f1
//#f3d6d6
//#f3babc
//#f09fa2
//#ec838a
//#e66572
//#de425b
const QColor kActualFlipColor("#de425b");
const QColor kFrameTimeColor("#f1f1f1");
const QColor kSimStartColor("#0054ae");
const QColor kAppTimeColor("#c04b6bba");
const QColor kDriverTimeColor("#7184c5");
const QColor kPresentApiTimeColor("#c0939ed0");
const QColor kGpuTimeColor("#f09fa2");
const QColor kDisplayQueueTimeColor("#e66572");
const QColor kAnimationErrorColor("#f9d7f0");

const QColor kAvgFpsColor("#0054ae");
const QColor k99FpsColor("#4b6bba");
const QColor k95FpsColor("#7184c5");
const QColor k90FpsColor("#939ed0");
const QColor k1FpsColor("#b3b9db");
const QColor kMovingAvgFpsColor("#de425b");
const QColor kFpsColor("#f09fa2");

const QColor kClipLineColor("#5c74a9");
const QColor kClipLineHoverColor("#b1cbff");
const QColor kClipLinePressColor("#00285a");

//# 0054ae
//# 7151b3
//#ad49ab
//#db4297
//#fb497b
//#ff615a
//#ff8237
//#ffa600
const int kMaxBarSets = 8;
const std::vector<QColor> kBarSetColors = {
    QColor("#0054ae"), QColor("#ffa600"), QColor("#7151b3"), QColor("#ff8237"),
    QColor("#ad49ab"), QColor("#ff615a"), QColor("#db4297"), QColor("#fb497b")};
#endif  // DIVERGENT_BLUE_HUE

// Palette Blue
#ifdef PALETTE_BLUE_HUE
/*
#0054ae
#7151b3
#ad49ab
#db4297
#fb497b
#ff615a
#ff8237
#ffa600
*/
const QColor kActualFlipColor("#0054ae");
const QColor kFrameTimeColor("#f1f1f1");
const QColor kSimStartColor("#0054ae");
const QColor kAppTimeColor("#c04b6bba");
const QColor kDriverTimeColor("#ff615a");
const QColor kPresentApiTimeColor("#c0ad49ab");
const QColor kGpuTimeColor("#ff8237");
const QColor kDisplayQueueTimeColor("#7151b3");
const QColor kAnimationErrorColor("#ffa600");

const QColor kAvgFpsColor("#0054ae");
const QColor k99FpsColor("#4b6bba");
const QColor k95FpsColor("#7184c5");
const QColor k90FpsColor("#939ed0");
const QColor k1FpsColor("#b3b9db");
const QColor kMovingAvgFpsColor("#de425b");
const QColor kFpsColor("#f09fa2");

const QColor kClipLineColor("#698ed7");
const QColor kClipLineHoverColor("#b1cbff");
const QColor kClipLinePressColor("#0054ae");

//# 0054ae
//# 7151b3
//#ad49ab
//#db4297
//#fb497b
//#ff615a
//#ff8237
//#ffa600
const int kMaxBarSets = 8;
const std::vector<QColor> kBarSetColors = {
    QColor("#0054ae"), QColor("#ffa600"), QColor("#7151b3"), QColor("#ff8237"),
    QColor("#ad49ab"), QColor("#ff615a"), QColor("#db4297"), QColor("#fb497b")};
#endif  // DIVERGENT_BLUE_HUE
#endif  // _COLOR_PALETTE_H_