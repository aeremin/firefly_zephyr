source ./third_party/pigweed/bootstrap.sh
export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH=$(pwd)/third_party/pigweed/environment/cipd/packages/arm
export ZEPHYR_BASE=$(pwd)/third_party/zephyr
cursor .