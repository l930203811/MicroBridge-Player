#pragma once

#include <JuceHeader.h>

#include <cmath>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

#include "Lyrics.h"
#include "Localization.h"
#include "SrTheme.h"

// S-Rack 轻量本地配置：桌面歌词的位置/尺寸/置顶存用户目录 ini（跨工程共享，
// 不污染宿主工程状态）。锁定态刻意不持久化：每次打开窗口都可交互，避免
// 「宿主恢复状态后窗口鼠标穿透却无从解锁」的幽灵锁定。
namespace dl
{
    struct Prefs
    {
        bool   hasBounds = false;
        double x = 0, y = 0, w = 0, h = 0;
        bool   pin = true;
    };

    static juce::File prefsFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("MicroBridge Player")
             .getChildFile ("desktop_lyrics.ini");
    }

    static Prefs load()
    {
        Prefs p;
        const auto f = prefsFile();
        if (! f.existsAsFile()) return p;
        std::unique_ptr<juce::XmlElement> xml (
            juce::XmlDocument (f.loadFileAsString()).getDocumentElement());
        if (xml == nullptr || ! xml->hasTagName ("DesktopLyrics")) return p;
        p.hasBounds = xml->getBoolAttribute ("hasBounds", false);
        p.x = xml->getDoubleAttribute ("x", 0.0);
        p.y = xml->getDoubleAttribute ("y", 0.0);
        p.w = xml->getDoubleAttribute ("w", 0.0);
        p.h = xml->getDoubleAttribute ("h", 0.0);
        p.pin = xml->getBoolAttribute ("pin", true);
        return p;
    }

    static void save (const Prefs& p)
    {
        juce::XmlElement xml ("DesktopLyrics");
        xml.setAttribute ("hasBounds", p.hasBounds);
        xml.setAttribute ("x", p.x);
        xml.setAttribute ("y", p.y);
        xml.setAttribute ("w", p.w);
        xml.setAttribute ("h", p.h);
        xml.setAttribute ("pin", p.pin);
        const auto f = prefsFile();
        f.getParentDirectory().createDirectory();
        xml.writeToFile (f, "UTF-8");
    }

    // 与 mb::tr 同签名：直接按进程语言选中文/英文，无需改 Localization 白名单。
    inline juce::String tr (const char* cn, const char* en)
    {
        return L10n::isEnglish() ? juce::String (juce::CharPointer_UTF8 (en))
                                 : juce::String (juce::CharPointer_UTF8 (cn));
    }
}


//==============================================================================
// 桌面歌词：无边框透明顶层窗口，桌面只显示一行当前歌词。
//
// · 整窗拖动 = 改位置；右下角 20px 手柄拖动 = 改大小。字号只由窗口高度与整首
//   歌词的最长行决定，全曲统一，不会因长短句交替而忽大忽小。
//   位置/尺寸/置顶写用户目录 desktop_lyrics.ini，下次打开恢复。
// · 颜色由播放器每帧传入：未唱 = 播放器当前歌词文字色，已唱 = 同一套 KTV
//   红→黄→绿渐变；对比色描边保证任意桌面壁纸都可读，主题切换即时生效。
// · 操作按钮只在鼠标悬停（淡色卡片衬底出现）时显示：左侧锁定、右侧置顶 + 关闭；
//   背景透明或已锁定（鼠标穿透）时全部隐藏，桌面只留干净的一行歌词。
// · 左上角锁定：WS_EX_TRANSPARENT 让光标穿透到下层应用，同时禁止拖动/缩放；
//   锁定时窗口自身收不到鼠标事件，回到播放器点右上角「桌面歌词」按钮关闭再开
//   即可解锁（锁定态不持久化）。最右侧「×」关闭窗口并同步播放器的开关。
// · 置顶状态独立持久化；每次打开窗口恢复上次的置顶选择。
//==============================================================================
class DesktopLyricsWindow : public juce::Component,
                            private juce::Timer
{
public:
    struct Palette
    {
        juce::Colour base;                  // 未唱文字（= 播放器 sr::lyricText）
        juce::Colour sung0, sungMid, sung1; // 已唱渐变（= sr::red / yellow / green）
        juce::Colour outline;               // 描边
        juce::Colour hint;                  // 无歌词时的提示
    };

    /** 用户点右上角置顶开关：宿主写盘 + 同步设置页复选框。 */
    std::function<void (bool)> onPinChanged;
    /** 用户点左上角锁定 / 关闭桌面歌词：宿主写盘 + 同步托盘入口。 */
    std::function<void (bool)> onLockChanged;
    /** 用户点最右侧「×」：宿主销毁窗口并同步播放器「桌面歌词」开关。 */
    std::function<void()> onCloseClicked;
    /** 拖动 / 缩放后：宿主把最新坐标同步进内存副本，避免后续保存覆盖掉位置。 */
    std::function<void (juce::Rectangle<int>)> onBoundsChanged;

    DesktopLyricsWindow()
    {
        // 先关不透明再加桌面：addToDesktop 会自动带上 windowIsSemiTransparent，
        // 避免 setOpaque 二次重建 peer 导致刚设好的位置 / 置顶丢失。
        setOpaque (false);
        addToDesktop (juce::ComponentPeer::windowIsTemporary
                      | juce::ComponentPeer::windowIgnoresKeyPresses);

        const auto prefs = dl::load();
        if (prefs.hasBounds
            && prefs.w >= 200.0 && prefs.w <= 4000.0
            && prefs.h >= 30.0  && prefs.h <= 400.0)
        {
            setBounds (juce::roundToInt (prefs.x), juce::roundToInt (prefs.y),
                       juce::roundToInt (prefs.w), juce::roundToInt (prefs.h));
        }
        else
        {
            const auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
            const auto area = d != nullptr ? d->userBounds.toNearestInt()
                                           : juce::Rectangle<int> (0, 0, 1920, 1080);
            const int w = juce::jlimit (320, 720, area.getWidth() - 120);
            setBounds (area.getCentreX() - w / 2, area.getBottom() - 140, w, 62);
        }

        pin_    = prefs.pin;
        locked_ = false;   // 不恢复穿透锁定：每次打开窗口都可交互
        applyTopMost();
        applyMousePassthrough();
        setVisible (true);
    }

    ~DesktopLyricsWindow() override
    {
        stopTimer();
        saveBounds();
    }

    bool isLocked() const noexcept { return locked_; }
    bool isPinned() const noexcept { return pin_; }

    /** 设置页改动（置顶 / 锁定 / 主题）后同步：不回调宿主，避免写盘回环。 */
    void syncSettings()
    {
        const auto prefs = dl::load();
        pin_ = prefs.pin;
        setLocked (false, false);
        applyTopMost();
        repaint();
    }

    void setPinned (bool on)
    {
        pin_ = on;
        auto prefs = dl::load(); prefs.pin = on; dl::save (prefs);
        applyTopMost();
        repaint();
    }

    /** 锁定 = 鼠标穿透 + 位置固定；解锁后恢复可拖动 / 可点击。
        notify=true 时通知宿主持久化（用户点窗口按钮），false 用于外部同步。 */
    void setLocked (bool on, bool notify = true)
    {
        if (locked_ == on)
        {
            applyMousePassthrough();
            return;
        }
        locked_ = on;
        hover_ = false;
        hoverBtn_ = 0;
        applyMousePassthrough();
        repaint();
        showHint (on
            ? dl::tr ("已锁定（鼠标穿透）：回到播放器点 ", "Locked (click-through): click ")
                   + unlockHotkeyText() + dl::tr (" 关闭再开即可解锁", " to close and reopen to unlock")
            : dl::tr ("已解锁：可拖动、缩放、点按钮",
                      "Unlocked: drag, resize or click the buttons"),
            on ? 6000 : 2600);
        if (notify && onLockChanged) onLockChanged (on);
    }

    /** 设置页改动后置顶状态即时同步（主窗口置顶时桌面歌词一并置顶）。 */
    void applyTopMost()
    {
        if (auto* peer = getPeer())
            peer->setAlwaysOnTop (pin_);
    }

    /** 由播放器 30fps 驱动：当前行 + 时间 + 配色 + 整本歌词（用于统一字号）。
        字号不再随每一句的长短变化，避免长句变小 / 短句变大来回跳。 */
    void setLine (const LyricLine* line, double t, const Palette& pal,
                  const juce::String& placeholder = {},
                  const Lyrics* book = nullptr)
    {
        syncLyricsBook (book);
        const bool hasLine = line != nullptr && ! line->text.trim().isEmpty();
        const juce::String newText = hasLine ? line->text
                                   : (placeholder.isNotEmpty() ? placeholder
                                      : dl::tr ("未找到歌词", "No lyrics"));
        const float newFrac = sungFraction (line, t);
        const bool changed = newText != text_
                          || hasLine != hasLine_
                          || std::abs (newFrac - sungFrac_) > 0.004f
                          || pal.base.getARGB()    != pal_.base.getARGB()
                          || pal.sung0.getARGB()   != pal_.sung0.getARGB()
                          || pal.sungMid.getARGB() != pal_.sungMid.getARGB()
                          || pal.sung1.getARGB()   != pal_.sung1.getARGB()
                          || pal.outline.getARGB() != pal_.outline.getARGB()
                          || pal.hint.getARGB()    != pal_.hint.getARGB();
        if (! changed) return;
        text_ = newText;
        hasLine_ = hasLine;
        sungFrac_ = newFrac;
        pal_ = pal;
        repaint();
    }

    /** 拖动 / 缩放窗口后，统一字号要按新尺寸重算。 */
    void resized() override { fontHeight_ = 0.0f; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();

        // 悬停时才铺一层极淡衬底 + 边框，提示「这行可以拖动 / 缩放」
        if (hover_ && ! locked_)
        {
            g.setColour (juce::Colours::black.withAlpha (0.24f));
            g.fillRoundedRectangle (b.reduced (1.0f), 6.0f);
            g.setColour (juce::Colours::white.withAlpha (0.26f));
            g.drawRoundedRectangle (b.reduced (1.0f), 6.0f, 1.0f);
        }

        // 左右两侧为操作按钮预留空间：左锁定 1 枚、右置顶 + 关闭 2 枚。
        // 固定留白让按钮显隐时歌词位置不跳动。
        const float padL = 34.0f, padR = 64.0f;
        const float avail = textAvailWidth();
        // 全曲统一字号：只由窗口尺寸 + 最长歌词行决定，与当前这一句无关
        const float size = unifiedFontSize();
        juce::Font f = sr::font (size, true);
        float tw = juce::GlyphArrangement::getStringWidth (f, text_);
        if (tw > avail && tw > 0.0f)
        {
            // 兜底（换曲瞬间 / 极长提示文本）：最多再收 20%，不再逐句忽大忽小
            f = f.withHeight (juce::jmax (size * 0.80f, size * avail / tw));
            tw = juce::jmin (avail, juce::GlyphArrangement::getStringWidth (f, text_));
        }
        const auto tr = juce::Rectangle<float> (padL + (avail - tw) * 0.5f, 0.0f,
                                                tw + 2.0f, b.getHeight());

        // 八向描边：浅色桌面用白底黑字描边、深色桌面反之，由播放器按主题给出
        g.setFont (f);
        g.setColour (pal_.outline);
        static constexpr float kOff[][2] = {
            { -1.4f, 0.0f }, { 1.4f, 0.0f }, { 0.0f, -1.4f }, { 0.0f, 1.4f },
            { -1.0f, -1.0f }, { 1.0f, -1.0f }, { -1.0f, 1.0f }, { 1.0f, 1.0f }
        };
        for (const auto& o : kOff)
            g.drawText (text_, tr.translated (o[0], o[1]),
                        juce::Justification::centredLeft, false);

        g.setColour (hasLine_ ? pal_.base : pal_.hint);
        g.drawText (text_, tr, juce::Justification::centredLeft, false);

        // 已唱部分：与播放器 KTV 浮层同一套三段渐变
        if (hasLine_ && sungFrac_ > 0.002f)
        {
            g.saveState();
            g.reduceClipRegion (juce::Rectangle<float> (tr.getX(), 0.0f,
                                                        tw * sungFrac_ + 2.0f,
                                                        b.getHeight()).toNearestInt());
            juce::ColourGradient grad (pal_.sung0, tr.getX(), 0.0f,
                                        pal_.sung1, tr.getRight(), b.getHeight(), false);
            grad.addColour (0.55, pal_.sungMid);
            g.setGradientFill (grad);
            g.drawText (text_, tr, juce::Justification::centredLeft, false);
            g.restoreState();
        }

        // 卡片衬底透明（未悬停）或已锁定（鼠标穿透）时一个按钮都不画
        if (hover_ && ! locked_)
        {
            drawButton (g, lockButtonBounds(),   1);
            drawButton (g, pinButtonBounds(),    2);
            drawButton (g, closeButtonBounds(),  3);
        }

        // 锁定/解锁后的短提示（锁定态无法再接收鼠标，靠这条提示告知解锁方式）
        if (hint_.isNotEmpty())
        {
            const float hs = juce::jlimit (10.0f, 13.0f, static_cast<float> (getHeight()) * 0.2f);
            const juce::Font hf = sr::font (hs);
            const float hw = juce::jmin (b.getWidth() - 8.0f,
                                         juce::GlyphArrangement::getStringWidth (hf, hint_) + 18.0f);
            const auto hr = juce::Rectangle<float> ((b.getWidth() - hw) * 0.5f,
                                                    b.getHeight() - hs - 6.0f, hw, hs + 5.0f);
            g.setColour (juce::Colours::black.withAlpha (0.72f));
            g.fillRoundedRectangle (hr, 5.0f);
            g.setColour (juce::Colours::white.withAlpha (0.95f));
            g.setFont (hf);
            g.drawText (hint_, hr, juce::Justification::centred, false);
        }

        if (hover_ && ! locked_)     // 右下角缩放手柄
        {
            g.setColour (juce::Colours::white.withAlpha (0.65f));
            for (int i = 0; i < 3; ++i)
                g.drawLine (b.getRight() - 4.0f - i * 3.0f, b.getBottom() - 4.0f,
                            b.getRight() - 4.0f, b.getBottom() - 4.0f - i * 3.0f, 1.2f);
        }
    }

    void mouseEnter (const juce::MouseEvent&) override { hover_ = true; repaint(); }
    void mouseExit  (const juce::MouseEvent&) override
    {
        hover_ = false; hoverBtn_ = 0; repaint();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        if (locked_) return;
        const int btn = buttonAt (e.position);
        hoverBtn_ = btn;
        const auto cursor = btn != 0 ? juce::MouseCursor::PointingHandCursor
                          : isResizeHandle (e.position) ? juce::MouseCursor::BottomRightCornerResizeCursor
                          : juce::MouseCursor::DraggingHandCursor;
        setMouseCursor (juce::MouseCursor (cursor));
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (locked_) return;
        const int btn = buttonAt (e.position);
        if (btn == 1)                       // 锁定：此后鼠标穿透，只能靠快捷键/托盘解锁
        {
            setLocked (true);
            return;
        }
        if (btn == 2)                       // 置顶：与设置页复选框双向同步
        {
            setPinned (! pin_);
            if (onPinChanged) onPinChanged (pin_);
            return;
        }
        if (btn == 3)                       // 关闭：宿主异步销毁（不能在 mouseDown 里自毁）
        {
            if (onCloseClicked) onCloseClicked();
            return;
        }
        resizing_ = isResizeHandle (e.position);
        draggingActive_ = true;
        dragStart_ = e.getScreenPosition();
        startBounds_ = getScreenBounds();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (locked_ || ! draggingActive_) return;
        const auto d = e.getScreenPosition() - dragStart_;
        if (resizing_)
            setBounds (startBounds_.getX(), startBounds_.getY(),
                       juce::jlimit (220, 3000, startBounds_.getWidth() + d.x),
                       juce::jlimit (32, 260, startBounds_.getHeight() + d.y));
        else
            setBounds (startBounds_.getX() + d.x, startBounds_.getY() + d.y,
                       startBounds_.getWidth(), startBounds_.getHeight());
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        draggingActive_ = false;
        saveBounds();
    }

private:
    /** 歌词可用宽度（与 paint 的左右留白保持一致）。 */
    float textAvailWidth() const noexcept
    {
        return juce::jmax (40.0f, static_cast<float> (getWidth()) - 34.0f - 64.0f);
    }

    /** 换曲 / 歌词重载时找出整本里最宽的一行，作为统一字号的基准。
        用「行数 + 首行文本」做指纹，避免 30fps 每帧遍历几百行。 */
    void syncLyricsBook (const Lyrics* book)
    {
        const juce::String sig = (book != nullptr && book->isLoaded())
            ? juce::String (book->getLineCount()) + ":"
              + (book->line (0) != nullptr ? book->line (0)->text : juce::String())
            : juce::String();
        if (sig == bookSig_) return;

        bookSig_ = sig;
        longestLine_.clear();
        if (book != nullptr && book->isLoaded())
        {
            const juce::Font probe = sr::font (20.0f, true);
            float maxW = 0.0f;
            for (int i = 0; i < book->getLineCount(); ++i)
            {
                const auto* L = book->line (i);
                if (L == nullptr || L->text.isEmpty()) continue;
                const float w = juce::GlyphArrangement::getStringWidth (probe, L->text);
                if (w > maxW) { maxW = w; longestLine_ = L->text; }
            }
        }
        fontHeight_ = 0.0f;      // 强制下一帧按新歌词集重算
        repaint();
    }

    /** 统一字号：基准由窗口高度决定；最长行放不下时整首一起缩小（有下限），
        而不是每句各缩各的。结果缓存，仅窗口尺寸 / 歌词集变化时重算。 */
    float unifiedFontSize()
    {
        if (fontHeight_ > 0.0f) return fontHeight_;
        const float base = juce::jlimit (14.0f, 110.0f,
                                         static_cast<float> (getHeight()) * 0.62f);
        const float avail = textAvailWidth();
        float size = base;
        if (longestLine_.isNotEmpty())
        {
            const float w = juce::GlyphArrangement::getStringWidth (
                                sr::font (base, true), longestLine_);
            if (w > avail)
                size = juce::jmax (base * 0.55f, base * avail / w);
        }
        fontHeight_ = size;
        return size;
    }

    static constexpr float kBtn = 26.0f;

    juce::Rectangle<float> lockButtonBounds() const noexcept
    {
        return { 4.0f, 4.0f, kBtn, kBtn };
    }
    juce::Rectangle<float> closeButtonBounds() const noexcept   // 最右侧：关闭
    {
        return { static_cast<float> (getWidth()) - kBtn - 4.0f, 4.0f, kBtn, kBtn };
    }
    juce::Rectangle<float> pinButtonBounds() const noexcept     // 紧挨关闭按钮左边
    {
        return closeButtonBounds().translated (- (kBtn + 4.0f), 0.0f);
    }
    int buttonAt (juce::Point<float> p) const noexcept
    {
        if (lockButtonBounds().contains (p))  return 1;
        if (pinButtonBounds().contains (p))   return 2;
        if (closeButtonBounds().contains (p)) return 3;
        return 0;
    }

    /** which: 1 = 锁定, 2 = 置顶, 3 = 关闭。只在 hover 且未锁定时调用。 */
    void drawButton (juce::Graphics& g, juce::Rectangle<float> r, int which)
    {
        const bool active = which == 1 ? locked_ : pin_;
        const auto fg = (which != 3 && active)
                      ? juce::Colour (0xffffd24a).withAlpha (0.96f)
                      : juce::Colours::white.withAlpha (0.96f);

        g.setColour (juce::Colours::black.withAlpha (hoverBtn_ == which ? 0.66f : 0.40f));
        g.fillRoundedRectangle (r, 7.0f);

        const float cx = r.getCentreX(), cy = r.getCentreY();
        if (which == 3)                     // 关闭：×
        {
            g.setColour (fg);
            g.drawLine (cx - 4.5f, cy - 4.5f, cx + 4.5f, cy + 4.5f, 1.7f);
            g.drawLine (cx + 4.5f, cy - 4.5f, cx - 4.5f, cy + 4.5f, 1.7f);
        }
        else if (which == 1)
        {
            juce::Path shackle;
            shackle.startNewSubPath (cx - 4.0f, cy - 0.5f);
            shackle.lineTo (cx - 4.0f, cy - 4.0f);
            shackle.quadraticTo (cx - 4.0f, cy - 8.0f, cx, cy - 8.0f);
            if (active)
            {
                shackle.quadraticTo (cx + 4.0f, cy - 8.0f, cx + 4.0f, cy - 4.0f);
                shackle.lineTo (cx + 4.0f, cy - 0.5f);
            }
            else
                shackle.quadraticTo (cx + 4.0f, cy - 8.0f, cx + 4.5f, cy - 4.5f);
            g.setColour (fg);
            g.strokePath (shackle, juce::PathStrokeType (1.7f));
            g.fillRoundedRectangle (juce::Rectangle<float> (cx - 6.0f, cy - 1.0f, 12.0f, 8.5f), 1.6f);
            g.setColour (juce::Colours::black.withAlpha (0.7f));
            g.fillEllipse (cx - 1.0f, cy + 2.0f, 2.0f, 2.0f);
        }
        else
        {
            g.setColour (fg);
            g.drawLine (cx - 6.0f, cy - 7.0f, cx + 6.0f, cy - 7.0f, 1.8f);
            juce::Path arrow;
            arrow.startNewSubPath (cx, cy - 5.0f);
            arrow.lineTo (cx - 5.0f, cy + 0.5f);
            arrow.lineTo (cx - 2.0f, cy + 0.5f);
            arrow.lineTo (cx - 2.0f, cy + 6.0f);
            arrow.lineTo (cx + 2.0f, cy + 6.0f);
            arrow.lineTo (cx + 2.0f, cy + 0.5f);
            arrow.lineTo (cx + 5.0f, cy + 0.5f);
            arrow.closeSubPath();
            if (active) g.fillPath (arrow);
            else        g.strokePath (arrow, juce::PathStrokeType (1.3f));
        }
    }

    bool isResizeHandle (juce::Point<float> p) const noexcept
    {
        return p.x >= static_cast<float> (getWidth()) - 20.0f
            && p.y >= static_cast<float> (getHeight()) - 20.0f;
    }

    /** 鼠标穿透：WS_EX_TRANSPARENT 让所有鼠标消息落到下层窗口。
        JUCE 的透明窗口本身带 WS_EX_LAYERED，改动时保留该位即可。 */
    void applyMousePassthrough()
    {
        setInterceptsMouseClicks (! locked_, ! locked_);
       #if JUCE_WINDOWS
        if (auto* peer = getPeer())
        {
            if (auto* hwnd = static_cast<HWND> (peer->getNativeHandle()))
            {
                const auto prev = GetWindowLongPtrW (hwnd, GWL_EXSTYLE);
                const auto next = locked_ ? (prev | WS_EX_TRANSPARENT)
                                          : (prev & ~static_cast<LONG_PTR> (WS_EX_TRANSPARENT));
                if (next != prev)
                {
                    SetWindowLongPtrW (hwnd, GWL_EXSTYLE, next);
                    SetWindowPos (hwnd, nullptr, 0, 0, 0, 0,
                                  SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER
                                  | SWP_NOACTIVATE | SWP_FRAMECHANGED);
                }
            }
        }
       #endif
    }

    static juce::String unlockHotkeyText()
    {
        return dl::tr ("播放器右上角的「桌面歌词」按钮",
                       "the Desktop Lyrics button at the player's top right");
    }

    void showHint (const juce::String& text, int ms)
    {
        hint_ = text;
        hintUntil_ = juce::Time::getMillisecondCounter() + static_cast<juce::uint32> (ms);
        startTimerHz (4);
        repaint();
    }

    void timerCallback() override
    {
        if (juce::Time::getMillisecondCounter() >= hintUntil_)
        {
            stopTimer();
            hint_.clear();
        }
        repaint();
    }

    /** 已唱比例 0..1（逐字按字宽累加，逐句按行时长）；字号只影响测量，比例不变。 */
    static float sungFraction (const LyricLine* L, double t)
    {
        if (L == nullptr) return 0.0f;
        const juce::Font f = sr::font (20.0f, true);
        const float total = juce::GlyphArrangement::getStringWidth (f, L->text);
        if (total <= 0.0f) return 0.0f;

        float sung = 0.0f;
        if (! L->words.empty())
        {
            for (const auto& w : L->words)
            {
                const float ww = juce::GlyphArrangement::getStringWidth (f, w.text);
                if (t >= w.t1) sung += ww;
                else if (t > w.t0)
                {
                    const float p = static_cast<float> ((t - w.t0)
                                    / juce::jmax (0.05, w.t1 - w.t0));
                    sung += ww * juce::jlimit (0.0f, 1.0f, p);
                    break;
                }
                else break;
            }
        }
        else if (L->t1 > L->t0)
            sung = total * static_cast<float> (
                       juce::jlimit (0.0, 1.0, (t - L->t0) / (L->t1 - L->t0)));

        return juce::jlimit (0.0f, 1.0f, sung / total);
    }

    void saveBounds()
    {
        const auto b = getScreenBounds();
        auto prefs = dl::load();
        prefs.hasBounds = true;
        prefs.x = b.getX();
        prefs.y = b.getY();
        prefs.w = b.getWidth();
        prefs.h = b.getHeight();
        dl::save (prefs);
        if (onBoundsChanged) onBoundsChanged (b);
    }

    juce::String text_, hint_;
    juce::String bookSig_, longestLine_;      // 全曲统一字号的输入与缓存指纹
    float fontHeight_ = 0.0f;                // 0 = 需重算（窗口尺寸 / 换曲时清零）
    bool hasLine_ = false;
    float sungFrac_ = 0.0f;
    bool pin_ = true, locked_ = false;
    Palette pal_ { juce::Colours::white, juce::Colour (0xffe4390e),
                   juce::Colour (0xffeed500), juce::Colour (0xff33d433),
                   juce::Colours::black, juce::Colours::grey };
    bool hover_ = false, resizing_ = false, draggingActive_ = false;
    int hoverBtn_ = 0;
    juce::uint32 hintUntil_ = 0;
    juce::Point<int> dragStart_;
    juce::Rectangle<int> startBounds_;

    JUCE_DECLARE_NON_COPYABLE (DesktopLyricsWindow)
};
