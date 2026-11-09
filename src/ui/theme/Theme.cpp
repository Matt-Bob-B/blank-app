#include "Theme.h"

namespace tess::theme
{

juce::Colour colour::trackColour (int index)
{
    static const juce::Colour palette[] = {
        juce::Colour (0xff5bbf7a), juce::Colour (0xff4b9fe8), juce::Colour (0xffe9a43c), juce::Colour (0xffb98cf0),
        juce::Colour (0xffe96c6c), juce::Colour (0xff45c4c0), juce::Colour (0xffe7d04b), juce::Colour (0xffef7fb7)
    };

    return palette[(size_t) juce::jmax (0, index) % std::size (palette)];
}

static juce::String chooseTypefaceName()
{
    static const juce::String name = []
    {
        auto available = juce::Font::findAllTypefaceNames();

        for (auto candidate : { "Inter", "Segoe UI Variable Text", "Segoe UI", "SF Pro Text", "Helvetica Neue", "Noto Sans", "DejaVu Sans" })
            if (available.contains (candidate))
                return juce::String (candidate);

        return juce::Font::getDefaultSansSerifFontName();
    }();

    return name;
}

juce::Font font (float height, bool bold)
{
    return juce::Font (juce::FontOptions (chooseTypefaceName(), height, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font monoFont (float height)
{
    static const juce::String name = []
    {
        auto available = juce::Font::findAllTypefaceNames();

        for (auto candidate : { "JetBrains Mono", "Cascadia Mono", "Consolas", "SF Mono", "DejaVu Sans Mono" })
            if (available.contains (candidate))
                return juce::String (candidate);

        return juce::Font::getDefaultMonospacedFontName();
    }();

    return juce::Font (juce::FontOptions (name, height, juce::Font::plain));
}

//==============================================================================
LookAndFeel::LookAndFeel()
{
    using namespace colour;

    setColourScheme ({ window, panel, panelRaised, hairline, text, accent, text, controlHover, text });

    setColour (juce::ResizableWindow::backgroundColourId, window);
    setColour (juce::DocumentWindow::textColourId, text);
    setColour (juce::TextButton::buttonColourId, control);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::Label::textColourId, text);
    setColour (juce::Label::outlineWhenEditingColourId, accent);
    setColour (juce::TextEditor::backgroundColourId, lcdBackground);
    setColour (juce::TextEditor::outlineColourId, hairline);
    setColour (juce::TextEditor::focusedOutlineColourId, accent);
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.4f));
    setColour (juce::ComboBox::backgroundColourId, control);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::arrowColourId, textSecondary);
    setColour (juce::PopupMenu::backgroundColourId, panelRaised);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::headerTextColourId, textSecondary);
    setColour (juce::Slider::thumbColourId, juce::Colour (0xffdcdce2));
    setColour (juce::Slider::trackColourId, accent);
    setColour (juce::Slider::rotarySliderFillColourId, accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, control);
    setColour (juce::Slider::textBoxTextColourId, textSecondary);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId, juce::Colour (0x55ffffff));
    setColour (juce::ListBox::backgroundColourId, panel);
    setColour (juce::TreeView::backgroundColourId, panel);
    setColour (juce::AlertWindow::backgroundColourId, panelRaised);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, accent);
    setColour (juce::CaretComponent::caretColourId, accent);
}

juce::Typeface::Ptr LookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
    {
        auto copy = f;
        copy.setTypefaceName (font (f.getHeight()).getTypefaceName());
        return juce::Typeface::createSystemTypefaceFor (copy);
    }

    return LookAndFeel_V4::getTypefaceForFont (f);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg,
                                        bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto c = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : bg;

    if (down)              c = c.brighter (0.15f);
    else if (highlighted)  c = c.brighter (0.07f);
    if (! b.isEnabled())   c = c.withMultipliedAlpha (0.5f);

    g.setColour (c);
    g.fillRoundedRectangle (r, (float) size::radius);

    // Subtle top highlight gives controls a gentle, lit-from-above depth.
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (r, (float) size::radius, 1.0f);
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int h)
{
    return font (juce::jmin (14.0f, (float) h * 0.5f));
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                    juce::Slider::SliderStyle style, juce::Slider& s)
{
    const bool vertical = style == juce::Slider::LinearVertical;

    if (! vertical && style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
        return;
    }

    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float trackThickness = 4.0f;

    auto track = vertical ? bounds.withSizeKeepingCentre (trackThickness, bounds.getHeight())
                          : bounds.withSizeKeepingCentre (bounds.getWidth(), trackThickness);

    g.setColour (colour::separator);
    g.fillRoundedRectangle (track, trackThickness * 0.5f);

    auto filled = vertical ? track.withTop (pos) : track.withRight (pos);
    g.setColour (s.findColour (juce::Slider::trackColourId).withAlpha (s.isEnabled() ? 0.9f : 0.4f));
    g.fillRoundedRectangle (filled, trackThickness * 0.5f);

    // Fader cap: a rounded pill with a centre line, like a hardware fader.
    auto cap = vertical ? juce::Rectangle<float> (0, 0, juce::jmin (bounds.getWidth(), 30.0f), 16.0f).withCentre ({ bounds.getCentreX(), pos })
                        : juce::Rectangle<float> (0, 0, 14.0f, juce::jmin (bounds.getHeight(), 20.0f)).withCentre ({ pos, bounds.getCentreY() });

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (cap.translated (0, 1.5f), 4.0f);
    g.setColour (s.findColour (juce::Slider::thumbColourId));
    g.fillRoundedRectangle (cap, 4.0f);
    g.setColour (colour::separator.withAlpha (0.8f));

    if (vertical)
        g.drawHorizontalLine ((int) cap.getCentreY(), cap.getX() + 4.0f, cap.getRight() - 4.0f);
    else
        g.drawVerticalLine ((int) cap.getCentreX(), cap.getY() + 4.0f, cap.getBottom() - 4.0f);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto centre = bounds.getCentre();
    const float lineW = 3.0f;
    auto arcRadius = radius - lineW;

    juce::Path bg;
    bg.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (colour::separator);
    g.strokePath (bg, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Bipolar controls (pan) fill from the centre; others from the start.
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zeroAngle = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    const float valueAngle = startAngle + pos * (endAngle - startAngle);

    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                       juce::jmin (zeroAngle, valueAngle), juce::jmax (zeroAngle, valueAngle), true);
    g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId));
    g.strokePath (arc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    auto knob = radius - lineW * 2.5f;
    g.setColour (colour::controlHover);
    g.fillEllipse (centre.x - knob, centre.y - knob, knob * 2.0f, knob * 2.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.0f, -knob, 2.0f, knob * 0.55f, 1.0f);
    g.setColour (colour::text);
    g.fillPath (pointer, juce::AffineTransform::rotation (valueAngle).translated (centre));
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<int> (w, h).toFloat().reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, (float) size::smallRadius);

    juce::Path chevron;
    auto cx = (float) w - 11.0f, cy = (float) h * 0.5f;
    chevron.startNewSubPath (cx - 3.5f, cy - 1.5f);
    chevron.lineTo (cx, cy + 2.0f);
    chevron.lineTo (cx + 3.5f, cy - 1.5f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&)    { return font (13.0f); }
juce::Font LookAndFeel::getPopupMenuFont()                    { return font (14.0f); }
juce::Font LookAndFeel::getLabelFont (juce::Label& l)         { return font (juce::jmin (14.0f, l.getFont().getHeight())); }
juce::Font LookAndFeel::getAlertWindowTitleFont()             { return font (17.0f, true); }
juce::Font LookAndFeel::getAlertWindowMessageFont()           { return font (14.0f); }
juce::Font LookAndFeel::getAlertWindowFont()                  { return font (13.0f); }
juce::Font LookAndFeel::getMenuBarFont (juce::MenuBarComponent&, int, const juce::String&) { return font (13.5f); }

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (colour::panelRaised);
    g.setColour (colour::hairline);
    g.drawRect (0, 0, w, h);
}

void LookAndFeel::drawMenuBarBackground (juce::Graphics& g, int w, int h, bool, juce::MenuBarComponent&)
{
    g.fillAll (colour::panel);
    g.setColour (colour::separator);
    g.drawHorizontalLine (h - 1, 0.0f, (float) w);
}

void LookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                                 int thumbStart, int thumbSize, bool over, bool down)
{
    auto thumb = vertical ? juce::Rectangle<int> (x, thumbStart, w, thumbSize)
                          : juce::Rectangle<int> (thumbStart, y, thumbSize, h);

    auto t = thumb.toFloat().reduced (2.5f);
    g.setColour (juce::Colours::white.withAlpha (down ? 0.45f : over ? 0.35f : 0.22f));
    g.fillRoundedRectangle (t, juce::jmin (t.getWidth(), t.getHeight()) * 0.5f);
}

} // namespace tess::theme
