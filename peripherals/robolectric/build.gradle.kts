plugins { id("com.android.application") version "8.13.2" }

// AGP rejects a package attribute in source manifests; Soong requires it.
// Test against a generated copy so the shipped manifest stays unchanged.
val appDir = file("..")
val testManifest = layout.buildDirectory.file("generated/AndroidManifest.xml").get().asFile
testManifest.parentFile.mkdirs()
testManifest.writeText(
    file("../AndroidManifest.xml").readText()
        .replace(Regex("""\s+package="[^"]*""""), "")
)

android {
    namespace = "org.eleos.peripherals"
    compileSdk = 36
    defaultConfig {
        minSdk = 35
        targetSdk = 36
    }
    sourceSets["main"].apply {
        manifest.srcFile(testManifest)
        java.setSrcDirs(listOf(File(appDir, "src")))
        res.setSrcDirs(listOf(File(appDir, "res")))
    }
    // Match Soong, which always compiles en-XA and ar-XB.
    buildTypes["debug"].isPseudoLocalesEnabled = true
    testOptions.unitTests.isIncludeAndroidResources = true
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

dependencies {
    testImplementation("junit:junit:4.13.2")
    testImplementation("org.robolectric:robolectric:4.16")
}

tasks.withType<Test>().configureEach {
    systemProperty("robolectric.graphicsMode", "NATIVE")
    testLogging { events("passed", "failed"); showStandardStreams = false }
}
