#pragma once

#include "engine/Tess.h"

namespace tess::theme
{

//==============================================================================
/** Design tokens. One place for every colour, radius and size in the app.
    Dark, low-contrast chrome so the music (regions, meters) carries the colour. */
namespace colour
{
    inline const juce::Colour window        { 0xff1c1c1f };
    inline const juce::Colour panel         { 0xff252529 };
    inline const juce::Colour panelRaised   { 0xff2e2e33 };
    inline const juce::Colour control       { 0xff3a3a40 };
    inline const juce::Colour controlHover  { 0xff46464d };
    inline const juce::Colour separator     { 0xff111113 };
    inline const juce::Colour hairline      { 0x1affffff };
    inline const juce::Colour lane          { 0xff202024 };
    inline const juce::Colour laneAlt       { 0xff232327 };
    inline const juce::Colour gridBar       { 0x26ffffff };
    inline const juce::Colour gridBeat      { 0x10ffffff };

    inline const juce::Colour text          { 0xffececf0 };
    inline const juce::Colour textSecondary { 0xff9b9ba3 };
    inline const juce::Colour textTertiary  { 0xff66666e };

    inline const juce::Colour accent        { 0xff3e8cff };   // selection, focus, active toggles
    inline const juce::Colour record        { 0xffef4f4a };
    inline const juce::Colour play          { 0xff46c36f };
    inline const juce::Colour cycle         { 0xffe9b23c };
    inline const juce::Colour mute          { 0xff4aa8e8 };
    inline const juce::Colour solo          { 0xffe9c93c };
    inline const juce::Colour playhead      { 0xffffffff };

    inline const juce::Colour lcdBackground { 0xff141416 };
    inline const juce::Colour lcdText       { 0xffd8e6ff };

    inline const juce::Colour meterLow      { 0xff46c36f };
    inline const juce::Colour meterMid      { 0xffe9c93c };
    inline const juce::Colour meterHigh     { 0xffef4f4a };

    /** Track/region palette, assigned round-robin to new tracks. */
    juce::Colour trackColour (int index);
}

namespace size
{
    constexpr int controlBarHeight = 52;
    constexpr int rulerHeight      = 30;
    constexpr int trackHeaderWidth = 220;
    constexpr int trackHeight      = 64;
    constexpr int radius           = 6;
    constexpr int smallRadius      = 4;
    constexpr int gap              = 8;
}

/** UI font: Inter if installed, else Segoe UI Variable / Segoe UI on Windows, else system sans. */
juce::Font font (float height, bool bold = false);
juce::Font monoFont (float height);

//==============================================================================
class LookAndFeel  : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float min, float max,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;

    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                        int thumbStart, int thumbSize, bool over, bool down) override;
    int getDefaultScrollbarWidth() override { return 10; }

    juce::Font getMenuBarFont (juce::MenuBarComponent&, int, const juce::String&) override;
    void drawMenuBarBackground (juce::Graphics&, int w, int h, bool, juce::MenuBarComponent&) override;

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowFont() override;
};

} // namespace tess::theme
