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
  Windows PC. Never commit `.dmg`, `.exe`, `.pkg` or plug-in bundles (git-ignored).
- **macOS DMG = a DMG that contains a standard installer `.pkg`** (`Installer/macos/build_dmg.sh`): English
  Welcome / Read Me / Conclusion, a Customize step to pick VST3, AU and AAX, "NF Audio Tools by Nenno Fernando".
  **No Apple Developer account for now**: VST3/AU are ad-hoc signed and the `.pkg` is unsigned (Gatekeeper: right-click > Open
  on other Macs). macOS cannot auto-launch an installer from a DMG, so the DMG opens a clean Finder window with only
  "Install NF Glue X.Y.Z.pkg". PACE account for signing: `nenofernando` (`WRAP_ACCOUNT`); `wraptool sign` also needs `--signid` (the wrap has "Digitally sign binary"), the script passes `WRAP_SIGNID` (default `-` = ad-hoc); the AAX SDK is auto-detected in `~/Documents`.
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
