plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

// ---------------------------------------------------------------------------
// Version from git, never hand-edited (Phase 5 working rule; DECISIONS_4 #3 for
// the desktop, #37 for iOS, #46 here). Play, like App Store Connect, wants a
// plain numeric versionName and a strictly increasing versionCode:
//   versionName = X.Y.Z from `git describe` — an RC's "-rc.N" is NOT part of it;
//                 it lives in BuildConfig.BUILD_DESCRIBE and in About
//   versionCode = commit count on HEAD (monotonic on main, unique per commit,
//                 so two RCs of one X.Y.Z upload as different codes)
//   SUMI_APP_VERSION (-D into the core build) = the full describe string, so
//                 the engine carries the same version the desktop build does
// SUMI_APP_VERSION in the environment overrides the describe (CI parity).
// `android/prepare_release.sh` prints these three for a human to read.
// ---------------------------------------------------------------------------
fun git(vararg args: String): String = try {
    providers.exec {
        commandLine("git", *args)
        isIgnoreExitValue = true
    }.standardOutput.asText.get().trim()
} catch (_: Exception) {
    ""
}

val sumiDescribe: String = (System.getenv("SUMI_APP_VERSION")?.takeIf { it.isNotBlank() }
    ?: git("describe", "--tags", "--always", "--dirty").ifBlank { "0.0.0-dev" }).removePrefix("v")
val sumiVersionName: String =
    Regex("^(\\d+\\.\\d+\\.\\d+)").find(sumiDescribe)?.groupValues?.get(1) ?: "0.0.0"
val sumiVersionCode: Int = git("rev-list", "--count", "HEAD").toIntOrNull()?.coerceAtLeast(1) ?: 1

tasks.register("printVersion") {
    description = "The version triple this checkout builds (android/prepare_release.sh reads it)."
    doLast {
        println("versionName=$sumiVersionName")
        println("versionCode=$sumiVersionCode")
        println("describe=$sumiDescribe")
    }
}

android {
    namespace = "com.vibetuned.midisink"
    compileSdk = 36
    ndkVersion = "27.2.12479018"

    defaultConfig {
        applicationId = "com.vibetuned.midisink"
        minSdk = 29          // AMidi floor (step brief)
        targetSdk = 36
        versionCode = sumiVersionCode
        versionName = sumiVersionName
        buildConfigField("String", "BUILD_DESCRIBE", "\"$sumiDescribe\"")
        externalNativeBuild {
            cmake {
                // Single static libc++ inside the one JNI lib; BUILD_TESTING
                // off keeps the root lists from even looking at ctest. The
                // version reaches the core the same way the spine injects it.
                arguments += listOf("-DANDROID_STL=c++_static", "-DBUILD_TESTING=OFF",
                                    "-DSUMI_APP_VERSION=$sumiDescribe")
                targets += "sumi-shell"
            }
        }
        ndk { abiFilters += "arm64-v8a" }
    }

    externalNativeBuild {
        cmake {
            path = file("../../CMakeLists.txt")   // repo root (iOS-precedent wiring)
            version = "3.30.3"
        }
    }

    buildFeatures {
        compose = true
        buildConfig = true   // BuildConfig.BUILD_DESCRIBE for About
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }
}

dependencies {
    implementation(platform("androidx.compose:compose-bom:2025.05.00"))
    implementation("androidx.activity:activity-compose:1.10.1")
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.foundation:foundation")
}
