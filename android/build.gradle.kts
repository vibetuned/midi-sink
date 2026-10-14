// Compose shell for the Suminagashi engine (§5.4). The native side is NOT a
// standalone CMake project: app/build.gradle.kts points externalNativeBuild
// at the REPO ROOT CMakeLists (the NDK toolchain defines ANDROID, which
// builds core/ + android/cpp only — see the root lists).
plugins {
    id("com.android.application") version "9.4.1" apply false
    // Kotlin is AGP 9's built-in (no kotlin-android plugin; the Compose compiler plugin stays its own)
    id("org.jetbrains.kotlin.plugin.compose") version "2.2.10" apply false
}
