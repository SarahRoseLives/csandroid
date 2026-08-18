cmake_minimum_required(VERSION 3.22.1)
project(cspsp C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Point at the repo root relative to this CMakeLists.txt (android/app/src/main/cpp).
get_filename_component(CPP_DIR ${CMAKE_CURRENT_SOURCE_DIR} ABSOLUTE)
set(JGE_DIR ${CPP_DIR}/../../../../../jge/JGE)
set(Cspsp_DIR ${CPP_DIR}/../../../../../jge/Projects/cspsp)

add_library(cspsp SHARED)

# --- JGE shared sources -----------------------------------------------------------
set(JGE_SOURCES
    ${JGE_DIR}/src/JApp.cpp
    ${JGE_DIR}/src/JGE.cpp
    ${JGE_DIR}/src/JGBKFont.cpp
    ${JGE_DIR}/src/JGui.cpp
    ${JGE_DIR}/src/JLBFont.cpp
    ${JGE_DIR}/src/JGameObject.cpp
    ${JGE_DIR}/src/JSpline.cpp
    ${JGE_DIR}/src/JAnimator.cpp
    ${JGE_DIR}/src/JResourceManager.cpp
    ${JGE_DIR}/src/JFileSystem.cpp
    ${JGE_DIR}/src/JParticle.cpp
    ${JGE_DIR}/src/JParticleEmitter.cpp
    ${JGE_DIR}/src/JParticleEffect.cpp
    ${JGE_DIR}/src/JParticleSystem.cpp
    ${JGE_DIR}/src/JSprite.cpp
    ${JGE_DIR}/src/Vector2D.cpp
    ${JGE_DIR}/src/unzip/ioapi.c
    ${JGE_DIR}/src/unzip/mztools.c
    ${JGE_DIR}/src/unzip/unzip.c
    ${JGE_DIR}/src/tinyxml/tinystr.cpp
    ${JGE_DIR}/src/tinyxml/tinyxml.cpp
    ${JGE_DIR}/src/tinyxml/tinyxmlparser.cpp
    ${JGE_DIR}/src/tinyxml/tinyxmlerror.cpp
    ${JGE_DIR}/src/hge/hgeparticle.cpp
    ${JGE_DIR}/src/hge/hgecolor.cpp
    ${JGE_DIR}/src/hge/hgevector.cpp
    ${JGE_DIR}/src/hge/hgerect.cpp
    # Android backends
    ${JGE_DIR}/src/android/JRenderer_Android.cpp
    ${JGE_DIR}/src/android/JSoundSystem_Android.cpp
    ${JGE_DIR}/src/android/JGEInputAndroid.cpp
)

# --- CSPSP game sources -----------------------------------------------------------
set(Cspsp_SOURCES
    ${Cspsp_DIR}/src/GameState.cpp
    ${Cspsp_DIR}/src/GameApp.cpp
    ${Cspsp_DIR}/src/GameLauncher.cpp
    ${Cspsp_DIR}/src/GameStateConnect.cpp
    ${Cspsp_DIR}/src/GameStateLoading.cpp
    ${Cspsp_DIR}/src/GameStateLobby.cpp
    ${Cspsp_DIR}/src/GameStateMenu.cpp
    ${Cspsp_DIR}/src/GameStateNewGame.cpp
    ${Cspsp_DIR}/src/GameStateOnline.cpp
    ${Cspsp_DIR}/src/GameStateOptions.cpp
    ${Cspsp_DIR}/src/GameStatePlay.cpp
    ${Cspsp_DIR}/src/GameStateSplash.cpp
    ${Cspsp_DIR}/src/GameStateUpdate.cpp
    ${Cspsp_DIR}/src/Person.cpp
    ${Cspsp_DIR}/src/AI.cpp
    ${Cspsp_DIR}/src/TileMap.cpp
    ${Cspsp_DIR}/src/Bullet.cpp
    ${Cspsp_DIR}/src/ParticleEngine.cpp
    ${Cspsp_DIR}/src/Particle.cpp
    ${Cspsp_DIR}/src/MenuItem.cpp
    ${Cspsp_DIR}/src/GunObject.cpp
    ${Cspsp_DIR}/src/SfxManager.cpp
    ${Cspsp_DIR}/src/Node.cpp
    ${Cspsp_DIR}/src/BuyMenu.cpp
    ${Cspsp_DIR}/src/TeamMenu.cpp
    ${Cspsp_DIR}/src/Hud.cpp
    ${Cspsp_DIR}/src/Camera.cpp
    ${Cspsp_DIR}/src/Wlan.cpp
    ${Cspsp_DIR}/src/Globals.cpp
    ${Cspsp_DIR}/src/Packet.cpp
    ${Cspsp_DIR}/src/UdpManager.cpp
    ${Cspsp_DIR}/src/Game.cpp
    ${Cspsp_DIR}/src/PersonOnline.cpp
    ${Cspsp_DIR}/src/Collision.cpp
    ${Cspsp_DIR}/src/GunObjectOnline.cpp
    ${Cspsp_DIR}/src/Danzeff.cpp
    ${Cspsp_DIR}/src/BulletOnline.cpp
    ${Cspsp_DIR}/src/Grenade.cpp
    ${Cspsp_DIR}/src/Grid.cpp
    ${Cspsp_DIR}/src/AStar.cpp
    ${Cspsp_DIR}/src/Animation.cpp
    ${Cspsp_DIR}/src/HttpManager.cpp
    ${Cspsp_DIR}/src/ListBox.cpp
)

# --- Android entry point ----------------------------------------------------------
set(Android_SOURCES
    ${CPP_DIR}/main.cpp
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)

target_sources(cspsp PRIVATE ${JGE_SOURCES} ${Cspsp_SOURCES} ${Android_SOURCES})

target_include_directories(cspsp PRIVATE
    ${JGE_DIR}/include
    ${JGE_DIR}/include/hge
    ${JGE_DIR}/src
    ${JGE_DIR}/src/android
    ${JGE_DIR}/Dependencies/include
    ${Cspsp_DIR}/src
    ${CPP_DIR}
    ${ANDROID_NDK}/sources/android/native_app_glue
)

target_compile_definitions(cspsp PRIVATE ANDROID)

target_compile_options(cspsp PRIVATE
    -fexceptions
    -frtti
    -Wno-write-strings
    -Wno-narrowing
    -Wno-unused-variable
    -Wno-missing-field-initializers
    -Wno-format
    -Wno-format-security
    "-include${CPP_DIR}/compat.h"
)

find_library(log-lib log)
find_library(android-lib android)
find_library(egl-lib EGL)
find_library(glesv1-lib GLESv1_CM)
find_library(z-lib z)

target_link_libraries(cspsp PRIVATE
    ${egl-lib}
    ${glesv1-lib}
    ${android-lib}
    ${log-lib}
    ${z-lib}
)
