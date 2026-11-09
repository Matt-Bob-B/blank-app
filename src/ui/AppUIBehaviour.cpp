#include "AppUIBehaviour.h"
#include "theme/Theme.h"

namespace tess
{

namespace
{
    /** Hosts a VST3 editor, or JUCE's generic editor for plugins without one. */
    struct ExternalEditor  : public te::Plugin::EditorComponent
    {
        explicit ExternalEditor (te::ExternalPlugin& p) : plugin (p)
        {
            if (auto* instance = plugin.getAudioPluginInstance())
            {
                editor.reset (instance->createEditorIfNeeded());

                if (editor == nullptr)
                    editor = std::make_unique<juce::GenericAudioProcessorEditor> (*instance);

                addAndMakeVisible (*editor);
            }

            fit();
        }

        bool allowWindowResizing() override  { return false; }

        juce::ComponentBoundsConstrainer* getBoundsConstrainer() override
        {
            return editor != nullptr ? editor->getConstrainer() : nullptr;
        }

        void resized() override
        {
            if (editor != nullptr)
                editor->setBounds (getLocalBounds());
        }

        void childBoundsChanged (juce::Component* c) override
        {
            if (c == editor.get())
            {
                plugin.edit.pluginChanged (plugin);
                fit();
            }
        }

        void fit()
        {
            setSize (juce::jmax (8, editor != nullptr ? editor->getWidth() : 0),
                     juce::jmax (8, editor != nullptr ? editor->getHeight() : 0));
        }

        te::ExternalPlugin& plugin;
        std::unique_ptr<juce::AudioProcessorEditor> editor;
    };

    class PluginWindow  : public juce::DocumentWindow
    {
    public:
        explicit PluginWindow (te::Plugin& p)
            : juce::DocumentWindow (p.getName(), theme::colour::panel, juce::DocumentWindow::closeButton),
              plugin (p)
        {
            setUsingNativeTitleBar (true);
            getConstrainer()->setMinimumOnscreenAmounts (0x10000, 50, 30, 50);
            recreateEditor();
            setBoundsConstrained (getLocalBounds() + plugin.windowState->choosePositionForPluginWindow());
            trackBounds = true;
        }

        ~PluginWindow() override
        {
            trackBounds = false;
            plugin.edit.flushPluginStateIfNeeded (plugin);
            setContentNonOwned (nullptr, false);
            editor.reset();
        }

        bool hasEditor() const  { return editor != nullptr; }

        void recreateEditor()
        {
            setContentNonOwned (nullptr, false);
            editor.reset();

            if (auto* external = dynamic_cast<te::ExternalPlugin*> (&plugin))
                editor = std::make_unique<ExternalEditor> (*external);
            else
                editor = plugin.createEditor();

            if (editor == nullptr)
                editor = std::make_unique<GenericBuiltInEditor> (plugin);

            setContentNonOwned (editor.get(), true);
            setResizable (editor->allowWindowResizing(), false);
        }

        void closeButtonPressed() override  { plugin.windowState->closeWindowExplicitly(); }

        void moved() override
        {
            if (trackBounds)
            {
                plugin.windowState->lastWindowBounds = getBounds();
                plugin.edit.pluginChanged (plugin);
            }
        }

    private:
        /** Built-in effects have no custom UI yet: expose their automatable parameters as sliders. */
        struct GenericBuiltInEditor  : public te::Plugin::EditorComponent
        {
            explicit GenericBuiltInEditor (te::Plugin& p)
            {
                for (auto* param : p.getAutomatableParameters())
                {
                    auto* row = rows.add (new Row (*param));
                    addAndMakeVisible (row->label);
                    addAndMakeVisible (row->slider);
                }

                setSize (420, juce::jmax (80, rows.size() * 34 + 20));
            }

            bool allowWindowResizing() override { return true; }
            juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

            void paint (juce::Graphics& g) override  { g.fillAll (theme::colour::panel); }

            void resized() override
            {
                auto r = getLocalBounds().reduced (12, 10);

                for (auto* row : rows)
                {
                    auto line = r.removeFromTop (34);
                    row->label.setBounds (line.removeFromLeft (140));
                    row->slider.setBounds (line);
                }
            }

            struct Row  : private te::AutomatableParameter::Listener
            {
                explicit Row (te::AutomatableParameter& p) : param (&p)
                {
                    label.setText (p.getParameterName(), juce::dontSendNotification);
                    label.setFont (theme::font (13.0f));
                    auto range = p.getValueRange();
                    slider.setSliderStyle (juce::Slider::LinearHorizontal);
                    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 20);
                    slider.setRange (range.getStart(), range.getEnd());
                    slider.setValue (p.getCurrentValue(), juce::dontSendNotification);
                    slider.textFromValueFunction = [this] (double) { return param->getCurrentValueAsString(); };
                    slider.onDragStart = [this] { param->parameterChangeGestureBegin(); };
                    slider.onDragEnd = [this] { param->parameterChangeGestureEnd(); };
                    slider.onValueChange = [this] { param->setParameter ((float) slider.getValue(), juce::sendNotification); };
                    param->addListener (this);
                }

                ~Row() override  { param->removeListener (this); }

                void curveHasChanged (te::AutomatableParameter&) override {}

                void currentValueChanged (te::AutomatableParameter&) override
                {
                    juce::MessageManager::callAsync ([this, sp = juce::Component::SafePointer<juce::Slider> (&slider)]
                    {
                        if (sp != nullptr && ! slider.isMouseButtonDown())
                            slider.setValue (param->getCurrentValue(), juce::dontSendNotification);
                    });
                }

                te::AutomatableParameter::Ptr param;
                juce::Label label;
                juce::Slider slider;
            };

            juce::OwnedArray<Row> rows;
        };

        te::Plugin& plugin;
        std::unique_ptr<te::Plugin::EditorComponent> editor;
        bool trackBounds = false;
    };
}

std::unique_ptr<juce::Component> AppUIBehaviour::createPluginWindow (te::PluginWindowState& state)
{
    auto* ws = dynamic_cast<te::Plugin::WindowState*> (&state);

    if (ws == nullptr)
        return {};

    if (auto* external = dynamic_cast<te::ExternalPlugin*> (&ws->plugin))
        if (external->getAudioPluginInstance() == nullptr)
            return {};

    auto w = std::make_unique<PluginWindow> (ws->plugin);
    w->setVisible (true);
    w->toFront (true);
    return w;
}

void AppUIBehaviour::recreatePluginWindowContentAsync (te::Plugin& p)
{
    if (auto* w = dynamic_cast<PluginWindow*> (p.windowState->pluginWindow.get()))
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PluginWindow> (w)]
        {
            if (sp != nullptr)
                sp->recreateEditor();
        });
}

void AppUIBehaviour::showWarningMessage (const juce::String& message)
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Tessitura", message);
}

void AppUIBehaviour::runTaskWithProgressBar (te::ThreadPoolJobWithProgress& task)
{
    // Renders are short for typical projects; run inline with a busy cursor.
    juce::MouseCursor::showWaitCursor();

    while (task.runJob() == juce::ThreadPoolJob::jobNeedsRunningAgain)
    {}

    juce::MouseCursor::hideWaitCursor();
}

} // namespace tess
