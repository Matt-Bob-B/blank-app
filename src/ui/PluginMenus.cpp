#include "PluginMenus.h"

namespace tess::plugin_menus
{

namespace
{
    struct BuiltIn { const char* name; const char* type; };

    const BuiltIn builtInEffects[] = {
        { "Channel EQ (4-band)", te::EqualiserPlugin::xmlTypeName },
        { "Compressor",          te::CompressorPlugin::xmlTypeName },
        { "Reverb",              te::ReverbPlugin::xmlTypeName },
        { "Delay",               te::DelayPlugin::xmlTypeName },
        { "Chorus",              te::ChorusPlugin::xmlTypeName },
        { "Phaser",              te::PhaserPlugin::xmlTypeName },
        { "Low/High Pass",       te::LowPassPlugin::xmlTypeName },
        { "Pitch Shifter",       te::PitchShiftPlugin::xmlTypeName },
    };

    void addExternal (juce::PopupMenu& menu, te::Edit& edit, bool instruments,
                      std::function<void (te::Plugin::Ptr)> onChosen)
    {
        auto types = edit.engine.getPluginManager().knownPluginList.getTypes();
        std::map<juce::String, juce::PopupMenu> byVendor;

        for (auto& desc : types)
        {
            if (desc.isInstrument != instruments)
                continue;

            byVendor[desc.manufacturerName.isEmpty() ? juce::String ("Other") : desc.manufacturerName]
                .addItem (desc.name, [&edit, desc, onChosen]
                {
                    if (auto p = edit.getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, desc))
                        onChosen (p);
                });
        }

        if (byVendor.empty())
        {
            menu.addItem ("No VST3 plugins found — use Settings > Plug-in Manager to scan", false, false, [] {});
            return;
        }

        for (auto& [vendor, sub] : byVendor)
            menu.addSubMenu (vendor, sub);
    }
}

bool isHiddenPlugin (te::Plugin& p)
{
    return dynamic_cast<te::VolumeAndPanPlugin*> (&p) != nullptr
        || dynamic_cast<te::LevelMeterPlugin*> (&p) != nullptr;
}

void chooseInstrument (te::Edit& edit, juce::Component& target, std::function<void (te::Plugin::Ptr)> onChosen)
{
    juce::PopupMenu menu, vst;
    menu.addSectionHeader ("Built-in");
    menu.addItem ("Synth (4-oscillator)", [&edit, onChosen]
    {
        if (auto p = edit.getPluginCache().createNewPlugin (te::FourOscPlugin::xmlTypeName, {}))
            onChosen (p);
    });
    menu.addItem ("Sampler", [&edit, onChosen]
    {
        if (auto p = edit.getPluginCache().createNewPlugin (te::SamplerPlugin::xmlTypeName, {}))
            onChosen (p);
    });

    addExternal (vst, edit, true, onChosen);
    menu.addSectionHeader ("VST3 Instruments");
    menu.addSubMenu ("VST3", vst);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target));
}

void chooseEffect (te::Edit& edit, juce::Component& target, std::function<void (te::Plugin::Ptr)> onChosen)
{
    juce::PopupMenu menu, vst;
    menu.addSectionHeader ("Built-in");

    for (auto& b : builtInEffects)
        menu.addItem (b.name, [&edit, type = juce::String (b.type), onChosen]
        {
            if (auto p = edit.getPluginCache().createNewPlugin (type, {}))
                onChosen (p);
        });

    addExternal (vst, edit, false, onChosen);
    menu.addSectionHeader ("VST3 Effects");
    menu.addSubMenu ("VST3", vst);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target));
}

void showEditor (te::Plugin& p)
{
    if (p.windowState != nullptr)
        p.showWindowExplicitly();
}

} // namespace tess::plugin_menus
