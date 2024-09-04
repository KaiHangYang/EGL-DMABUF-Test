
ANDROID_NDK_HOME=/home/faceunity/Programs/Android/NDK/android-ndk-r25c
ANDROID_ABI="arm64-v8a"
# DEBUG: for AHardwareBuffer_allocate
ANDROID_API=26

INSTALL_DIR=./out/android/$ANDROID_ABI

cmake -S . -B build_android \
        -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR \
        -G "Unix Makefiles" \
        -DANDROID_STL=c++_static \
        -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
        -DANDROID_NDK=$ANDROID_NDK_HOME \
        -DANDROID_TOOLCHAIN=clang \
        -DANDROID_ABI=$ANDROID_ABI \
        -DANDROID_NATIVE_API_LEVEL=$ANDROID_API \
        -DOCLT_COMPILING_LOG_LEVEL=SPDLOG_LEVEL_DEBUG \
        -DOCLT_ENABLE_CLHPP_EXCEPTIONS=1 \
        -DOCLT_BUILD_EXAMPLES=ON

cmake --build ./build_android --target install

REMOTE_DIR=/data/local/tmp/egl_dma_test
adb shell "[[ -d ${REMOTE_DIR} ]] && rm -r ${REMOTE_DIR} || true"
adb shell "mkdir -p ${REMOTE_DIR}"

adb push $INSTALL_DIR/main $REMOTE_DIR

adb shell "cd $REMOTE_DIR && ./main"
