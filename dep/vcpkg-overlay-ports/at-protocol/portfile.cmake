# at-protocol: the language at-tmux and at-terminal share (ATT-7). REF is the one
# place this terminal's version of the language is chosen: the commit of
# https://forgejo.romaine.life/romaine-life/at-protocol it is built from, which the
# build compiles in as the version its hello says. at-tmux pins the same commit, and
# the two are released together.
set(REF 1dff9944cdf728cfe514bd41483c81e14186787f)
vcpkg_from_git(
    OUT_SOURCE_PATH SOURCE_PATH
    URL https://forgejo.romaine.life/romaine-life/at-protocol.git
    REF ${REF}
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DAT_PROTOCOL_VERSION=${REF}
        -DAT_PROTOCOL_TESTS=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME at-protocol CONFIG_PATH lib/cmake/at-protocol)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(WRITE "${CURRENT_PACKAGES_DIR}/share/${PORT}/copyright" "romaine-life/at-protocol: Nelson's own.\n")
