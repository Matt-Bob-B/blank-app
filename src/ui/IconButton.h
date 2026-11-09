#pragma once

#include "Icons.h"
#include "theme/Theme.h"

namespace tess
{

/** Rounded toolbar button that shows a vector icon. When toggled on it fills
    with its "on" colour (e.g. red for record, yellow for cycle). */
class IconButton  : public juce::Button
{
public:
    IconButton (const juce::String& name, icons::Icon i, juce::Colour onColour = theme::colour::accent)
        : juce::Button (name), icon (i), onColour (onColour)
    {
        setTooltip (name);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    /** Colour used for the glyph when the button is off (e.g. record shows a red dot). */
    void setIdleGlyphColour (juce::Colour c)  { idleGlyph = c; repaint(); }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const bool on = getToggleState();

        auto bg = on ? onColour : theme::colour::control;
        if (down)             bg = bg.brighter (0.15f);
        else if (highlighted) bg = bg.brighter (0.08f);

        g.setColour (bg);
        g.fillRoundedRectangle (r, (float) theme::size::radius);

        auto glyphColour = on ? (onColour.getPerceivedBrightness() > 0.6f ? juce::Colour (0xff1c1c1f) : juce::Colours::white)
                              : idleGlyph.value_or (theme::colour::text);

        auto path = icons::get (icon);
        const float side = juce::jmin (r.getWidth(), r.getHeight()) * 0.72f;
        auto box = juce::Rectangle<float> (side, side).withCentre (r.getCentre());
        path.applyTransform (juce::AffineTransform::scale (side / 24.0f).translated (box.getX(), box.getY()));

        g.setColour (glyphColour.withMultipliedAlpha (isEnabled() ? 1.0f : 0.4f));

        if (icons::isStroked (icon))
            g.strokePath (path, juce::PathStrokeType (1.9f * side / 24.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        else
            g.fillPath (path);
    }

private:
    icons::Icon icon;
    juce::Colour onColour;
    std::optional<juce::Colour> idleGlyph;
};

} // namespace tess
