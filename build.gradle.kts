import com.android.build.gradle.AppExtension
import org.gradle.api.tasks.Exec
import java.io.ByteArrayOutputStream

plugins {
    alias(libs.plugins.agp.app) apply false
}

fun String.execute(currentWorkingDir: File = file("./")): String {
    val byteOut = ByteArrayOutputStream()
    try {
        val proc = Runtime.getRuntime().exec(this, emptyArray(), currentWorkingDir)
        byteOut.write(proc.inputStream.readBytes())
        proc.waitFor()
    } catch (e: Exception) {
        return ""
    }
    return String(byteOut.toByteArray()).trim()
}

val gitCommitCount = try {
    "git rev-list HEAD --count".execute().toIntOrNull() ?: 0
} catch (e: Exception) {
    0
}
val gitCommitHash = try {
    "git rev-parse --verify --short HEAD".execute().ifEmpty { "unknown" }
} catch (e: Exception) {
    "unknown"
}

// also the soname
val moduleId by extra("zygisk-injector")
val moduleName by extra("Zygisk Injector")
val verName by extra("v1")
val verCode by extra(gitCommitCount)
val commitHash by extra(gitCommitHash)
val abiList by extra(listOf("arm64-v8a", "armeabi-v7a", "x86", "x86_64"))

val androidMinSdkVersion by extra(26)
val androidTargetSdkVersion by extra(36)
val androidCompileSdkVersion by extra(36)
val androidBuildToolsVersion by extra("36.0.0")
val androidCompileNdkVersion by extra("29.0.14206865")
val androidSourceCompatibility by extra(JavaVersion.VERSION_21)
val androidTargetCompatibility by extra(JavaVersion.VERSION_21)

tasks.register("Delete", Delete::class) {
    delete(layout.buildDirectory.get().asFile)
}

fun Project.configureBaseExtension() {
    extensions.findByType(AppExtension::class)?.run {
        namespace = "io.github.a13e300.zygisk_next.module.sample"
        compileSdkVersion(androidCompileSdkVersion)
        ndkVersion = androidCompileNdkVersion
        buildToolsVersion = androidBuildToolsVersion

        defaultConfig {
            minSdk = androidMinSdkVersion
        }

        compileOptions {
            sourceCompatibility = androidSourceCompatibility
            targetCompatibility = androidTargetCompatibility
        }
    }

}

subprojects {
    plugins.withId("com.android.application") {
        configureBaseExtension()
    }
    plugins.withType(JavaPlugin::class.java) {
        extensions.configure(JavaPluginExtension::class.java) {
            sourceCompatibility = androidSourceCompatibility
            targetCompatibility = androidTargetCompatibility
        }
    }
}
