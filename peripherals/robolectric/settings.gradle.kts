// Host-side UI test harness for the External devices app. The AOSP product
// builds the app with Soong (../Android.bp); this project only runs tests.
pluginManagement {
    repositories { google(); mavenCentral(); gradlePluginPortal() }
}
dependencyResolutionManagement {
    repositories { google(); mavenCentral() }
}
rootProject.name = "ele-peripherals-robolectric"
