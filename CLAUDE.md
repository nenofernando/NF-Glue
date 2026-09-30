# NF Glue -- notes for Claude

Standalone repo for the **NF Glue** compressor plug-in (NF Audio Tools). It was split from NF Q3's repo, so the
look-and-feel and artwork are its own copies (`Source/UI/NFGlueLookAndFeel.*`, `Assets/`).

## Mandatory: version consistency
Whenever the version changes it MUST be right everywhere. **Single source of truth: `CMakeLists.txt` line 2**
(`project(NFGlue VERSION X.Y.Z ...)`). Everything else derives from it automatically:
- plugin UI footer "V1.0.0" (bottom-left) reads `JucePlugin_VersionString`;
- `Installer/macos/build_dmg.sh` and `Installer/Windows/build_windows_installer.ps1` read it from `CMakeLists.txt`
  (the Windows script passes it to Inno Setup as `/DMyAppVersion`; `NFGlue.iss` refuses to build without it).
After a bump, grep for the old version to catch anything missed:
`grep -rn "OLD_VERSION" --include="*.cpp" --include="*.h" --include="*.iss" --include="*.sh" --include="*.ps1" --include="*.txt" .`

## Build & release rules (owner's decisions)
- **No GitHub Actions / no uploads.** Installers are built locally: DMG on the owner's Mac, `.exe` on the partner's
  Windows PC. Never commit `.dmg`, `.exe`, or plug-in bundles (git-ignored).
- **AAX must be in the installers and PACE-signed** (`wraptool`, wrap "NF Glue - Signing Only",
  Wrap GUID `3FA9A390-BCC4-11F1-8E61-00505692C25A`). AAX SDK is expected in `~/Documents/AAX_SDK`
  (`AAX_SDK_PATH` overrides). `SKIP_AAX=1` is the only way to build without it. Never re-run `codesign` on a
  signed `.aaxplugin`. Never commit the AAX SDK, passwords or certificates.
- Identifiers: bundle `com.nfaudiotools.nfglue`, manufacturer `Nfat`, plug-in code `Nfgl`, product number `NFGLUE001`.
- Presets: `.nfgluepreset` in `Documents/NF Audio Tools/NF Glue/Presets`; 15 factory presets in `Source/FactoryPresets.h`.

## Design notes
- UI is a fixed 1200x400 layout scaled uniformly; keep the approved look (do not add controls unasked).
- DSP is JUCE-free in `Source/DSP/Compressor.h` and covered by `Tests/CompressorTests.cpp`.
- "2x" = ratio x2 and threshold -6 dB (doubling the ratio alone is inaudible at high ratios).
- Output is makeup gain 0..+24 dB (0 dB at the far left).
- No licence system yet (NF Q3 has one); decide before selling.
