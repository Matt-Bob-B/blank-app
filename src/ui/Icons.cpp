#include "Icons.h"

namespace tess::icons
{

bool isStroked (Icon i)
{
    return i == Icon::cycle || i == Icon::metronome || i == Icon::mixer
        || i == Icon::pianoRoll || i == Icon::library || i == Icon::plus || i == Icon::scissors;
}

juce::Path get (Icon i)
{
    juce::Path p;

    switch (i)
    {
        case Icon::play:
            p.addTriangle (8.0f, 5.5f, 8.0f, 18.5f, 19.0f, 12.0f);
            p = p.createPathWithRoundedCorners (1.5f);
            break;

        case Icon::stop:
            p.addRoundedRectangle (6.5f, 6.5f, 11.0f, 11.0f, 2.0f);
            break;

        case Icon::record:
            p.addEllipse (6.0f, 6.0f, 12.0f, 12.0f);
            break;

        case Icon::toStart:
            p.addRoundedRectangle (5.5f, 6.0f, 2.2f, 12.0f, 1.0f);
            p.addTriangle (18.5f, 6.0f, 18.5f, 18.0f, 9.0f, 12.0f);
            break;

        case Icon::cycle:
            // Two opposing arrows around a rounded loop.
            p.addRoundedRectangle (4.0f, 7.0f, 16.0f, 10.0f, 5.0f);
            p.startNewSubPath (13.0f, 4.5f);  p.lineTo (15.5f, 7.0f);  p.lineTo (13.0f, 9.5f);
            p.startNewSubPath (11.0f, 14.5f); p.lineTo (8.5f, 17.0f);  p.lineTo (11.0f, 19.5f);
            break;

        case Icon::metronome:
            p.startNewSubPath (9.0f, 4.5f);  p.lineTo (15.0f, 4.5f);  p.lineTo (18.5f, 19.5f);
            p.lineTo (5.5f, 19.5f);          p.closeSubPath();
            p.startNewSubPath (12.0f, 16.0f); p.lineTo (17.0f, 7.0f);
            break;

        case Icon::mixer:
            for (auto x : { 6.0f, 12.0f, 18.0f })
            {
                p.startNewSubPath (x, 4.5f);
                p.lineTo (x, 19.5f);
            }
            p.addRoundedRectangle (4.0f, 13.0f, 4.0f, 3.0f, 1.0f);
            p.addRoundedRectangle (10.0f, 7.0f, 4.0f, 3.0f, 1.0f);
            p.addRoundedRectangle (16.0f, 11.0f, 4.0f, 3.0f, 1.0f);
            break;

        case Icon::pianoRoll:
            p.addRoundedRectangle (4.0f, 5.0f, 16.0f, 14.0f, 2.0f);
            p.startNewSubPath (8.0f, 9.0f);   p.lineTo (12.0f, 9.0f);
            p.startNewSubPath (11.0f, 12.0f); p.lineTo (17.0f, 12.0f);
            p.startNewSubPath (7.0f, 15.0f);  p.lineTo (10.0f, 15.0f);
            break;

        case Icon::library:
            p.addRoundedRectangle (4.5f, 5.0f, 4.0f, 14.0f, 1.0f);
            p.addRoundedRectangle (10.0f, 5.0f, 4.0f, 14.0f, 1.0f);
            p.startNewSubPath (16.0f, 6.0f);  p.lineTo (19.5f, 18.5f);
            break;

        case Icon::plus:
            p.startNewSubPath (12.0f, 6.0f);  p.lineTo (12.0f, 18.0f);
            p.startNewSubPath (6.0f, 12.0f);  p.lineTo (18.0f, 12.0f);
            break;

        case Icon::scissors:
            p.addEllipse (5.0f, 14.0f, 5.0f, 5.0f);
            p.addEllipse (14.0f, 14.0f, 5.0f, 5.0f);
            p.startNewSubPath (9.0f, 15.0f);  p.lineTo (16.0f, 5.0f);
            p.startNewSubPath (15.0f, 15.0f); p.lineTo (8.0f, 5.0f);
            break;
    }

    return p;
}

} // namespace tess::icons
