include(FetchContent)

# All dependencies are permissively licensed (see README "Licencias").
# Any of them can be pointed at a local checkout with -DFETCHCONTENT_SOURCE_DIR_<NAME>=/path.

# nlohmann/json (MIT) - project / edit JSON.
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(nlohmann_json)

if(TONEAYAN_BUILD_TESTS)
    # Catch2 v3 (BSL-1.0) - unit tests.
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.1
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(Catch2)
endif()

if(TONEAYAN_BUILD_PLUGIN)
    # JUCE 8 (AGPLv3 / commercial).
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.9
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)

    if(TONEAYAN_ENABLE_ARA)
        # Celemony ARA SDK 2.2.0 (Apache-2.0). JUCE 8 requires exactly this version.
        FetchContent_Declare(ARA_SDK
            GIT_REPOSITORY https://github.com/Celemony/ARA_SDK.git
            GIT_TAG        releases/2.2.0
            GIT_SHALLOW    TRUE
            GIT_SUBMODULES ARA_API ARA_Library)
        FetchContent_MakeAvailable(ARA_SDK) # no top-level CMakeLists: this only downloads it
        juce_set_ara_sdk_path("${ara_sdk_SOURCE_DIR}")
    endif()
endif()
