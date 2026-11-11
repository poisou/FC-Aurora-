#pragma once

#include <QtGlobal>

// ============================================================================
// Aurora 每帧数据前的 6 字节包头。
//
// 目前仅定义为 6 字节全 0 占位结构。后续确定具体协议后，可以直接把
// payload 数组替换为具体字段，但必须保证结构体总大小仍然为 6 字节。
// 保存数据源时，程序会自动把 payload[4]、payload[5] 设置为当前帧
// 包含的 1024 字节包数，字节序由 BIN_HEADER_BIG_ENDIAN 控制。
//
// 修改示例：
//   struct AHEAD
//   {
//       quint8  fieldA;
//       quint16 fieldB;
//       quint16 fieldC;
//       quint8  fieldD;
//   };
//
// 写入文件时会按 AHEAD 的内存布局逐字节写入，因此字段顺序、类型宽度
// 和字节序需要与实际 Aurora 协议保持一致。
// ============================================================================
#pragma pack(push, 1)
struct AHEAD
{
    quint8 payload[6] = {0, 0, 0, 0, 0, 0};
};
#pragma pack(pop)

static_assert(sizeof(AHEAD) == 6, "AHEAD must be exactly 6 bytes.");
