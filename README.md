# Swell Chain — 0.2.0

Une chaîne vocale avec les plugins Swell, dans un seul plugin. VST3 / AU / Standalone, par **Swell-audio** · @sevyondatracks.

## Ce qu'il y a

- **6 emplacements** en série. Chaque module : On/Off, solo (S), retirer (×), et on le déplace en le glissant.
- **7 modules**, avec le traitement audio des plugins Swell :
  Vocal Match (EQ et compression, analyse de la voix), VOX (Clean, Compress, De-ess, Air, Warmth),
  Vocal Lift, Ad-lib FX (23 presets, macros Space, Dirt, Pitch, Width, Mix), Space & Time (20 presets, Width, Focus, Ping-pong),
  Knock pour la voix (Saturation et Attaque séparées, sans renfort des graves) et Glue (6 styles).
- **32 presets de chaînes** classés par style : Trap, R&B, Rap, Afro, Drill, Rage, FX, Topline.
- **Analyser ma voix** : 12 secondes de voix, puis Vocal Match se règle sur la voix (Grave, Médium ou Aiguë).
- **A/B**, **Bypass**, **Volume auto**, mix et gain de toute la chaîne, mètres d'entrée et de sortie.
- **Latence fixe** de 36 échantillons (0,8 ms à 44,1 kHz) : elle ne bouge pas quand on change de modules.
- **Sortie protégée** : rien ne change sous -1 dBFS, puis une courbe douce qui ne dépasse jamais 0 dBFS.

Les presets sont des points de départ, à valider à l'écoute dans de vraies sessions.

## Compiler

Mac : `./build-mac.sh` (universel Apple Silicon et Intel, macOS 11+).
Windows : `.\build.ps1` (Visual Studio 2022 Build Tools et CMake).

Pour réutiliser un JUCE 8.0.6 déjà téléchargé :
`./build-mac.sh -DFETCHCONTENT_SOURCE_DIR_JUCE=../Swell-Glue/build/_deps/juce-src`

## Tests

`ctest --test-dir build-mac` : chaîne vide neutre et retard exact, chaque module à 44,1 / 48 / 96 kHz,
les 32 presets stables avec le volume auto, silence, bypass, changements de presets pendant la lecture.
Validation Apple AU (auval) réussie sur Mac.
