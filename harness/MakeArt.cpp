// Exports every logo, icon and installer image from the single logo source (source/ui/Logo.cpp),
// so all of them stay identical. Re-run after changing the logo:
//
//   ./cmake-build-debug/MakeArt packaging
//
// Writes, under the given packaging directory:
//   icon.png, icon_small.png        ICON_BIG / ICON_SMALL for juce_add_plugin (JUCE makes the
//                                   standalone app's .icns/.ico from these at build time)
//   Chordify.icns                   custom Finder icon for the plugin bundles (macOS, via iconutil)
//   Chordify.ico                    installer / uninstaller icon (Windows)
//   resources/background*.png       macOS installer artwork (light and dark mode)
//   windows/wizard*.png             Inno Setup wizard images (converted to BMP by the build script)
//   marketing/*.png                 large logo and wordmark for a website or store listing

#include <ui/Logo.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace
{
    juce::File outputRoot;

    template <typename Painter>
    juce::Image render (int width, int height, Painter&& paint, juce::Colour background = juce::Colours::transparentBlack)
    {
        juce::Image image (juce::Image::ARGB, width, height, true);
        juce::Graphics g (image);
        g.fillAll (background);
        paint (g, juce::Rectangle<float> ((float) width, (float) height));
        return image;
    }

    juce::MemoryBlock pngData (const juce::Image& image)
    {
        juce::MemoryOutputStream stream;
        juce::PNGImageFormat().writeImageToStream (image, stream);
        return stream.getMemoryBlock();
    }

    void save (const juce::Image& image, const juce::String& relativePath)
    {
        const auto file = outputRoot.getChildFile (relativePath);
        file.getParentDirectory().createDirectory();
        const auto data = pngData (image);
        file.replaceWithData (data.getData(), data.getSize());
        std::cout << "wrote " << file.getFullPathName() << " (" << image.getWidth() << "x" << image.getHeight() << ")\n";
    }

    juce::Image appIcon (int size)
    {
        // Fine detail turns to noise at small sizes, so those use the simplified drawing
        return render (size, size, [size] (juce::Graphics& g, auto bounds) { logo::drawAppIcon (g, bounds, size <= 64); });
    }

    // .ico with PNG-compressed entries (supported since Windows Vista)
    void writeIco (const juce::String& relativePath, std::initializer_list<int> sizes)
    {
        std::vector<juce::MemoryBlock> images;
        for (auto size : sizes)
            images.push_back (pngData (appIcon (size)));

        juce::MemoryOutputStream out;
        out.writeShort (0);                     // reserved
        out.writeShort (1);                     // type: icon
        out.writeShort ((short) images.size()); // image count

        auto offset = 6 + 16 * (int) images.size();
        size_t i = 0;
        for (auto size : sizes)
        {
            out.writeByte ((char) (size >= 256 ? 0 : size)); // 0 means 256
            out.writeByte ((char) (size >= 256 ? 0 : size));
            out.writeByte (0);  // palette size
            out.writeByte (0);  // reserved
            out.writeShort (1); // colour planes
            out.writeShort (32); // bits per pixel
            out.writeInt ((int) images[i].getSize());
            out.writeInt (offset);
            offset += (int) images[i].getSize();
            ++i;
        }
        for (const auto& image : images)
            out.write (image.getData(), image.getSize());

        const auto file = outputRoot.getChildFile (relativePath);
        file.replaceWithData (out.getData(), out.getDataSize());
        std::cout << "wrote " << file.getFullPathName() << "\n";
    }

    void writeIcns (const juce::String& relativePath)
    {
        // Build an .iconset and let Apple's iconutil pack it
        const auto iconset = juce::File::createTempFile (".iconset");
        iconset.createDirectory();
        for (auto size : { 16, 32, 128, 256, 512 })
        {
            const auto oneX = pngData (appIcon (size));
            const auto twoX = pngData (appIcon (size * 2));
            iconset.getChildFile ("icon_" + juce::String (size) + "x" + juce::String (size) + ".png").replaceWithData (oneX.getData(), oneX.getSize());
            iconset.getChildFile ("icon_" + juce::String (size) + "x" + juce::String (size) + "@2x.png").replaceWithData (twoX.getData(), twoX.getSize());
        }

        const auto file = outputRoot.getChildFile (relativePath);
        juce::ChildProcess iconutil;
        if (iconutil.start (juce::StringArray { "iconutil", "-c", "icns", iconset.getFullPathName(), "-o", file.getFullPathName() })
            && iconutil.waitForProcessToFinish (30000) && iconutil.getExitCode() == 0)
            std::cout << "wrote " << file.getFullPathName() << "\n";
        else
            std::cout << "skipped " << relativePath << " (needs macOS iconutil)\n";
        iconset.deleteRecursively();
    }

    // macOS Installer artwork: the wordmark in the bottom-left corner, transparent elsewhere so the
    // installer's own text stays readable. One for light mode, one for dark.
    juce::Image installerBackground (juce::Colour ink)
    {
        constexpr int scale = 2; // drawn at 2x for Retina; the installer scales it to fit
        return render (620 * scale, 418 * scale, [ink] (juce::Graphics& g, auto) {
            g.addTransform (juce::AffineTransform::scale ((float) scale));
            logo::drawWordmark (g, { 22.0f, 418.0f - 76.0f, 250.0f, 52.0f }, ink);
        });
    }

    // Inno Setup's tall wizard image: a strip of the cream case with the logo printed on it
    juce::Image wizardSidebar (int width, int height)
    {
        return render (width, height, [] (juce::Graphics& g, juce::Rectangle<float> bounds) {
            const auto s = bounds.getWidth() / 164.0f;
            g.setGradientFill (juce::ColourGradient::vertical (pt1::colours::caseTop, 0.0f, pt1::colours::caseBottom, bounds.getBottom()));
            g.fillRect (bounds);
            logo::drawKeysMark (g, { 22.0f * s, 40.0f * s, 120.0f * s, 70.0f * s });
            g.setColour (pt1::colours::ink);
            g.setFont (pt1::printFont (28.0f * s));
            g.drawText ("Chordify", juce::Rectangle<float> (0.0f, 128.0f * s, bounds.getWidth(), 36.0f * s), juce::Justification::centred, false);
            g.setColour (pt1::colours::accent);
            g.fillRect (juce::Rectangle<float> (42.0f * s, 168.0f * s, 80.0f * s, 3.0f * s));
            g.setColour (pt1::colours::inkDim);
            g.setFont (pt1::printFont (9.5f * s));
            g.drawText ("CHORD RESONATOR", juce::Rectangle<float> (0.0f, 178.0f * s, bounds.getWidth(), 14.0f * s), juce::Justification::centred, false);
            g.drawText ("DUPHON", juce::Rectangle<float> (0.0f, bounds.getBottom() - 30.0f * s, bounds.getWidth(), 14.0f * s), juce::Justification::centred, false);
        });
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juce;

    if (argc < 2)
    {
        std::cerr << "usage: MakeArt <packaging directory>\n";
        return 1;
    }
    outputRoot = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);
    outputRoot.createDirectory();

    // App icons for juce_add_plugin: JUCE builds the standalone app's .icns/.ico from these
    save (appIcon (1024), "icon.png");
    save (render (256, 256, [] (juce::Graphics& g, auto bounds) { logo::drawAppIcon (g, bounds, true); }), "icon_small.png");

    writeIcns ("Chordify.icns");
    writeIco ("Chordify.ico", { 16, 32, 48, 256 });

    save (installerBackground (pt1::colours::ink), "resources/background.png");
    save (installerBackground (pt1::colours::caseTop), "resources/background-dark.png");

    save (wizardSidebar (164, 314), "windows/wizard.png");
    save (wizardSidebar (328, 628), "windows/wizard@2x.png");
    save (render (55, 55, [] (juce::Graphics& g, auto bounds) { logo::drawAppIcon (g, bounds, true); }, pt1::colours::caseTop), "windows/wizard-small.png");
    save (render (110, 110, [] (juce::Graphics& g, auto bounds) { logo::drawAppIcon (g, bounds, true); }, pt1::colours::caseTop), "windows/wizard-small@2x.png");

    save (appIcon (2048), "marketing/Chordify-icon-2048.png");
    save (render (1600, 400, [] (juce::Graphics& g, auto bounds) { logo::drawWordmark (g, bounds.reduced (40.0f, 60.0f)); }), "marketing/Chordify-wordmark.png");

    return 0;
}
