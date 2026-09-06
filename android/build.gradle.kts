// Compose shell for the Suminagashi engine (§5.4). The native side is NOT a
// standalone CMake project: app/build.gradle.kts points externalNativeBuild
// at the REPO ROOT CMakeLists (the NDK toolchain defines ANDROID, which
// builds core/ + android/cpp only — see the root lists).
plugins {
    id("com.android.application") version "8.13.2" apply false
    id("org.jetbrains.kotlin.android") version "2.1.20" apply false
    id("org.jetbrains.kotlin.plugin.compose") version "2.1.20" apply false
}
