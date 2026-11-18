#pragma once

//==============================================================================
// 微音桥·播放器 主题：对齐 MicroBridge 音频播放器的深 / 浅两套配色。
//
// · 深色 = Studio One 风格中性深灰（Theme.h 的 mb::ui 深色板）+ 主题蓝 accent；
//   浅色 = 淡蓝紫（#CBDDFE 底，与主程序浅色主题同一色阶）。
// · 颜色全部是运行时变量：paint 入口调 sr::syncTheme() 后再取色，
//   切换主题只需翻转开关 + repaint()，无需重建任何控件。
// · 主题开关为进程级设置（与 L10n 同款），并由宿主状态持久化；
//   timerCallback 检测外部切换（如工程恢复晚于编辑器构造）后自动重绘。
//==============================================================================

#include <JuceHeader.h>

#include <atomic>

namespace sr
{
    // 0 = 深色（默认），1 = 浅色。进程级，宿主状态持久化。
    inline std::atomic<int>& themeRef()
    {
        static std::atomic<int> v { 0 };
        return v;
    }
    inline int  get()          { return themeRef().load (std::memory_order_relaxed); }
    inline void set (int v)    { themeRef().store (v == 1 ? 1 : 0, std::memory_order_relaxed); }
    inline bool isDark()       { return get() == 0; }
    inline bool isLight()      { return get() == 1; }
    inline void toggle()       { set (isDark() ? 1 : 0); }

    // ---- 运行时色板（paint 处 syncTheme() 后读取）--------------------------
    inline juce::Colour bg1, bg2;                    // 主背景渐变亮 / 暗端
    inline juce::Colour sidebarBg, sidebarEdge;      // 侧栏底 / 左缘
    inline juce::Colour titleBg, line;               // 标题条 / 分隔线
    inline juce::Colour accent;                      // 主色
    inline juce::Colour pill, pillHover, pillOn;     // 胶囊：常态 / 悬停 / 开关态
    inline juce::Colour pillBase, pillOff;           // 弹窗基座 / 禁用态
    inline juce::Colour keyBorder, offText;          // 键帽描边 / 禁用文字
    inline juce::Colour listBg, listCurrent, empty;  // 列表底 / 当前曲 / 空提示
    inline juce::Colour text, dim, faint;            // 文字三级
    inline juce::Colour vizBg1, vizBg2, vizBgMid;    // 频谱渐变底
    inline juce::Colour vizReflect, vizLine;         // 频谱倒影罩色 / 基线
    inline juce::Colour trackColour, fillColour;     // 进度条轨 / 填充
    inline juce::Colour lyricBacking, lyricText, lyricTextDim;
    inline juce::Colour green, yellow, red;          // LED 红→黄→绿
    inline juce::Colour early, late;                 // 歌词提前（绿）/ 延后（黄）
    inline juce::Colour popupBg, popupBorder;        // 弹窗面板
    inline juce::Colour shadow;                      // 键帽 / 卡片投影

    inline void applyPalette (bool dark)
    {
        if (dark)
        {
            // 与 MicroBridge Theme.h 的 mb::ui 深色板逐项对齐
            bg1 = juce::Colour (0xff2a2c31); bg2 = juce::Colour (0xff141518);
            sidebarBg = juce::Colour (0xff1a1b1e); sidebarEdge = juce::Colour (0xff101114);
            titleBg = juce::Colour (0xff1a1b1e); line = juce::Colour (0xff35383e);
            accent = juce::Colour (0xff2aa9e0);
            pill = juce::Colour (0xff31343a);
            pillHover = juce::Colour (0xff3a3e46);
            pillOn = pill.interpolatedWith (accent, 0.30f);
            pillBase = pill.interpolatedWith (accent, 0.45f);
            pillOff = juce::Colour (0xff24262b);
            keyBorder = juce::Colour (0xff35383e); offText = juce::Colour (0xff5d626a);
            listBg = juce::Colour (0xff24262b); listCurrent = juce::Colour (0xff123a4d);
            empty = juce::Colour (0xff5d626a);
            text = juce::Colour (0xffe6e8eb); dim = juce::Colour (0xff9aa0a8);
            faint = juce::Colour (0xff5d626a);
            vizBg1 = juce::Colour (0xff24262b); vizBg2 = juce::Colour (0xff141518);
            vizBgMid = juce::Colour (0xff2a2c31); vizReflect = juce::Colour (0xff141518);
            vizLine = juce::Colour (0xff2aa9e0).withAlpha (0.55f);
            trackColour = juce::Colour (0xff101114); fillColour = accent;
            lyricBacking = juce::Colour (0xff141518).withAlpha (0.85f);
            lyricText = juce::Colour (0xffe6e8eb);
            lyricTextDim = juce::Colour (0xff9aa0a8).withAlpha (0.85f);
            popupBg = juce::Colour (0xff24262b); popupBorder = juce::Colour (0xff2aa9e0).withAlpha (0.65f);
            shadow = juce::Colours::black.withAlpha (0.38f);
        }
        else
        {
            // 与 MicroBridge 播放器浅色板 / 主程序浅色 Theme 同一色阶
            bg1 = juce::Colour (0xfff2f7ff); bg2 = juce::Colour (0xffdce7fc);
            sidebarBg = juce::Colour (0xffe2ecfd); sidebarEdge = juce::Colour (0xff9fb3dd);
            titleBg = juce::Colour (0xffe2ecfd); line = juce::Colour (0xff9fb3dd);
            accent = juce::Colour (0xff1e88d0);
            pill = juce::Colour (0xffe4eefe);
            pillHover = juce::Colour (0xffd6e3fc);
            pillOn = juce::Colour (0xffd2e1fb);
            pillBase = juce::Colour (0xffcadcf9);
            pillOff = juce::Colour (0xffc7d4ef);
            keyBorder = juce::Colour (0xff9fb3dd); offText = juce::Colour (0xff9aa8c8);
            listBg = juce::Colour (0xffe8f0fe); listCurrent = juce::Colour (0xffd2e1fb);
            empty = juce::Colour (0xff8292b8);
            text = juce::Colour (0xff20304f); dim = juce::Colour (0xff4c5e86);
            faint = juce::Colour (0xff8292b8);
            vizBg1 = juce::Colour (0xffdcebf9); vizBg2 = juce::Colour (0xffc9dbf8);
            vizBgMid = juce::Colour (0xffd4e4fb); vizReflect = juce::Colour (0xffc6d8f6);
            vizLine = juce::Colour (0xff1e88d0).withAlpha (0.65f);
            trackColour = juce::Colour (0xffc2d1f4); fillColour = accent;
            lyricBacking = juce::Colour (0x90ffffff);
            lyricText = juce::Colour (0xff20304f); lyricTextDim = juce::Colour (0xcc20304f);
            popupBg = juce::Colour (0xffe8f0fe); popupBorder = juce::Colour (0xff1e88d0).withAlpha (0.65f);
            shadow = juce::Colours::black.withAlpha (0.16f);
        }

        green  = juce::Colour (dark ? 0xff3ecf8e : 0xff33d433);
        yellow = juce::Colour (dark ? 0xffeec643 : 0xffeed500);
        red    = juce::Colour (dark ? 0xffef5350 : 0xffe4390e);
        early  = green; late = yellow;
    }

    /** paint 入口调用：把调色板对齐到当前主题（幂等，开销可忽略）。 */
    inline void syncTheme() { applyPalette (isDark()); }

    /** 统一字体：与 MicroBridge 一致（Microsoft YaHei UI）。 */
    inline juce::Font font (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions ("Microsoft YaHei UI", size,
                                              bold ? juce::Font::bold : juce::Font::plain));
    }
}
