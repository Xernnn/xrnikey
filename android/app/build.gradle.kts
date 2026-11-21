import java.util.Properties

plugins {
    // Kotlin is built into AGP 9: no kotlin-android plugin.
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.compose)
}

// The repo: android/ sits in it, and the core it builds is ../core.
val repoDir: File = rootProject.projectDir.parentFile

// The owner's release key, outside every repo in ../xrn-signing.properties next to the
// repo (storeFile, storePassword, keyAlias, keyPassword; storeFile relative to that file's
// folder). It is found only from the main checkout; a worktree's release build is unsigned.
val xrnSigningFile: File = repoDir.resolve("../xrn-signing.properties")
val xrnSigning: Properties? = xrnSigningFile.takeIf { it.isFile }?.let { f ->
    Properties().apply { f.inputStream().use { load(it) } }
}

// The trained models (data/lm/README.md): bundled into the APK at build time, never in git.
// -Pxrnkey.models=<dir> points elsewhere.
val modelDir: File = (findProperty("xrnkey.models") as String?)?.let(::File)
    ?: File(System.getProperty("user.home"), ".cache/xrnkey-lm/models")

// The engine built for this machine, for the JVM tests: the core, the JNI bridge and the
// small test models trained from made-up text (android/host/CMakeLists.txt).
val hostDir: File = layout.buildDirectory.dir("host").get().asFile
val hostSrc: File = rootProject.projectDir.resolve("host")

val configureHost = tasks.register<Exec>("configureHost") {
    description = "Configures the engine's build for this machine (JVM tests)."
    inputs.file(hostSrc.resolve("CMakeLists.txt"))
    outputs.file(hostDir.resolve("build.ninja"))
    commandLine("cmake", "-S", hostSrc, "-B", hostDir, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release")
}

val buildHost = tasks.register<Exec>("buildHost") {
    description = "Builds the engine, its JNI bridge and the test models for this machine."
    dependsOn(configureHost)
    inputs.dir(repoDir.resolve("core"))
    inputs.dir(repoDir.resolve("tools"))
    inputs.dir(repoDir.resolve("data/lm"))
    inputs.dir(projectDir.resolve("src/main/cpp"))
    outputs.dir(hostDir.resolve("lm"))
    outputs.dir(hostDir.resolve("jni"))
    commandLine("cmake", "--build", hostDir, "-j", "4")
}

abstract class BundleModels : DefaultTask() {
    @get:Internal abstract val realDir: DirectoryProperty
    @get:Internal abstract val testDir: DirectoryProperty
    @get:InputFile abstract val emojiIndex: RegularFileProperty
    @get:Input abstract val release: Property<Boolean>
    @get:OutputDirectory abstract val outputDir: DirectoryProperty

    @TaskAction
    fun bundle() {
        val out = outputDir.get().asFile
        out.deleteRecursively()
        out.mkdirs()
        emojiIndex.get().asFile.copyTo(out.resolve("emoji-index.tsv"))
        val real = realDir.get().asFile
        val vi = real.resolve("vi-phone.xklm")
        val en = real.resolve("en.xklm")
        if (vi.isFile && en.isFile) {
            vi.copyTo(out.resolve("vi.xklm"))
            en.copyTo(out.resolve("en.xklm"))
            return
        }
        if (release.get()) {
            throw GradleException(
                "No trained models in $real (vi-phone.xklm and en.xklm are needed for a release " +
                    "build). Build them as data/lm/README.md says, or pass -Pxrnkey.models=<dir>.",
            )
        }
        logger.warn("xrnkey: no trained models in $real; this debug build bundles the small test models")
        val test = testDir.get().asFile
        test.resolve("vi.xklm").copyTo(out.resolve("vi.xklm"))
        test.resolve("en.xklm").copyTo(out.resolve("en.xklm"))
    }
}

android {
    namespace = "com.xern.xrnkey"
    compileSdk = 35
    ndkVersion = "27.1.12297006"

    defaultConfig {
        applicationId = "com.xern.xrnkey"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"
        ndk { abiFilters += "arm64-v8a" }
        externalNativeBuild {
            cmake {
                // c++_shared: the core and the JNI bridge are two libraries. The job pool keeps
                // the native build to 4 compilers on a laptop whose RAM is shared.
                arguments += listOf(
                    "-DANDROID_STL=c++_shared",
                    "-DCMAKE_JOB_POOLS=xk=4",
                    "-DCMAKE_JOB_POOL_COMPILE=xk",
                    "-DCMAKE_JOB_POOL_LINK=xk",
                )
            }
        }
    }

    externalNativeBuild {
        // The system CMake, through cmake.dir in local.properties: the core needs 3.27.
        cmake { path = file("src/main/cpp/CMakeLists.txt") }
    }

    signingConfigs {
        if (xrnSigning != null) {
            create("xrn") {
                storeFile = xrnSigningFile.parentFile.resolve(xrnSigning.getProperty("storeFile"))
                storePassword = xrnSigning.getProperty("storePassword")
                keyAlias = xrnSigning.getProperty("keyAlias")
                keyPassword = xrnSigning.getProperty("keyPassword")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
            signingConfigs.findByName("xrn")?.let { signingConfig = it }
        }
    }

    // The models are memory-mapped straight from the APK, so they must stay uncompressed.
    androidResources { noCompress += "xklm" }

    buildFeatures {
        compose = true
        buildConfig = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    testOptions {
        unitTests {
            isIncludeAndroidResources = true
            all { test ->
                test.dependsOn(buildHost)
                test.systemProperty("java.library.path", hostDir.resolve("jni").path)
                test.systemProperty("xrnkey.testModels", hostDir.resolve("lm").path)
                test.systemProperty("xrnkey.realModels", modelDir.path)
                test.systemProperty("xrnkey.emojiIndex", repoDir.resolve("data/emoji/emoji-index.tsv").path)
                (findProperty("xrnkey.shots") as String?)?.let { test.systemProperty("xrnkey.shots", it) }
                test.maxHeapSize = "1536m"
                // A native library binds to one class loader: the Robolectric tests and the plain
                // JNI tests each need a JVM of their own.
                test.forkEvery = 1
                test.jvmArgs("--add-opens", "java.base/java.io=ALL-UNNAMED")
            }
        }
    }
}

androidComponents {
    onVariants { variant ->
        val release = variant.buildType == "release"
        val name = variant.name.replaceFirstChar { it.uppercase() }
        val bundle = tasks.register<BundleModels>("bundle${name}Models") {
            realDir.set(modelDir)
            emojiIndex.set(repoDir.resolve("data/emoji/emoji-index.tsv"))
            testDir.set(hostDir.resolve("lm"))
            this.release.set(release)
            if (!release && !modelDir.resolve("vi-phone.xklm").isFile) dependsOn(buildHost)
        }
        variant.sources.assets?.addGeneratedSourceDirectory(bundle, BundleModels::outputDir)
    }
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.activity.compose)
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.compose.ui)
    implementation(libs.androidx.compose.material3)
    testImplementation(libs.junit)
    testImplementation(libs.robolectric)
}
