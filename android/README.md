# xrnkey for Android

The keyboard: Telex through the shared core (`../core`), with accent restoration and the
English/Vietnamese choice from the language models. It has no permissions. The feature spec is
[docs/feature-spec.md](docs/feature-spec.md).

## Build and test

`local.properties` needs the SDK and the system CMake (the core needs CMake 3.27 or newer):

```
sdk.dir=/path/to/Android/Sdk
cmake.dir=/usr
```

```bash
./gradlew :app:testDebugUnitTest      # JVM tests, against the engine built for this machine
./gradlew :app:assembleDebug          # arm64-v8a
./gradlew :app:assembleRelease        # needs the trained models
```

- **Models.** The trained models (`vi-phone.xklm`, `en.xklm`; `../data/lm/README.md`) are
  bundled into the APK from `~/.cache/xrnkey-lm/models`, or from `-Pxrnkey.models=<dir>`.
  - A debug build without them bundles the small test models.
  - A release build without them fails.
  - They are stored uncompressed and memory-mapped in place.
- **Tests.** The JVM tests run the real core through the JNI bridge, built for this machine by
  `host/CMakeLists.txt` (the `buildHost` task, which needs a C++ compiler, CMake, Ninja, Python 3
  and a JDK).
- **Screenshots.** `-Pxrnkey.shots=<dir>` saves the keyboard's screenshots there (Robolectric,
  native graphics).
- **Signing.** A release build is signed with the key that `../../xrn-signing.properties`
  describes, when that file is there. A worktree's release build comes out unsigned.

## Credits

MiSans (Xiaomi) for every label: `app/src/main/res/font/misans_vf.ttf`.
