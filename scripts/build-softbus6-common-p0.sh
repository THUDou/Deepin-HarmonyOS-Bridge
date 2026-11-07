#!/usr/bin/env bash
# P0 probe: compile OpenHarmony 6.0 dsoftbus core/common as a standalone
# Linux host static library, plus a minimal hilog stub and log smoke test.
# It does not claim the full SoftBus server builds.
set -euo pipefail

LAB="${LAB:-/root/softbus6}"
OH="$LAB/dsoftbus"
OUT="$LAB/p0"

if [[ ! -d "$OH/core/common" ]]; then
  echo "error: missing $OH/core/common" >&2
  exit 1
fi

rm -rf "$OUT"
mkdir -p "$OUT/stub/hilog"

cat > "$OUT/stub/hilog/log.h" <<'EOF'
#ifndef DHB_P0_HILOG_STUB_H
#define DHB_P0_HILOG_STUB_H
#include <stdarg.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { LOG_CORE = 0 };
enum { LOG_DEBUG = 3, LOG_INFO = 4, LOG_WARN = 5, LOG_ERROR = 6, LOG_FATAL = 7 };
int HILOG_IMPL(int type, int level, unsigned int domain, const char *tag,
               const char *fmt, ...);
#ifdef __cplusplus
}
#endif
#endif
EOF

cat > "$OUT/hilog_stub.c" <<'EOF'
#include "hilog/log.h"
#include <stdarg.h>
#include <stdio.h>

int HILOG_IMPL(int type, int level, unsigned int domain, const char *tag,
               const char *fmt, ...)
{
    (void)type;
    (void)domain;
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[hilog:%d:%s] ", level, tag != NULL ? tag : "-");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    return 0;
}
EOF

cat > "$OUT/log_smoke.c" <<'EOF'
#include "comm_log.h"
#include "softbus_log.h"

int main(void)
{
    COMM_LOGE(COMM_UTILS, "hilog stub smoke %d", 42);
    return 0;
}
EOF

cat > "$OUT/link_smoke.c" <<'EOF'
#include "softbus_adapter_timer.h"
#include <stdio.h>

int main(void)
{
    printf("softbus_time_ms=%llu\n",
           (unsigned long long)SoftBusGetSysTimeMs());
    return 0;
}
EOF

cat > "$OUT/deps_smoke.c" <<'EOF'
#include "cJSON.h"
#include "securec.h"
#include "sqlite3.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    char dst[8] = {0};
    errno_t rc = memcpy_s(dst, sizeof(dst), "abc", 3);
    cJSON *json = cJSON_Parse("{\"ok\":1}");
    printf("securec_rc=%d json_object=%d sqlite=%s\n",
           (int)rc, json != NULL && cJSON_IsObject(json), sqlite3_libversion());
    cJSON_Delete(json);
    return (rc == 0 && json != NULL) ? 0 : 1;
}
EOF

cat > "$OUT/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.16)
project(dhb_softbus6_common_p0 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(OH "${ROOT}/dsoftbus")

add_library(hilog_stub STATIC hilog_stub.c)
target_include_directories(hilog_stub PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/stub")

file(GLOB SECUREC_SOURCES "${ROOT}/third_party/bounds_checking_function/src/*.c")
add_library(securec_p0 STATIC ${SECUREC_SOURCES})
target_include_directories(securec_p0 PUBLIC
  "${ROOT}/third_party/bounds_checking_function/include"
  "${ROOT}/third_party/bounds_checking_function/src")
target_compile_options(securec_p0 PRIVATE -Wall -Wextra -Wno-unused-parameter)

add_library(cjson_p0 STATIC "${ROOT}/third_party/cJSON/cJSON.c")
target_include_directories(cjson_p0 PUBLIC "${ROOT}/third_party/cJSON/include")

add_library(sqlite_p0 STATIC "${ROOT}/third_party/sqlite/src/sqlite3.c")
target_include_directories(sqlite_p0 PUBLIC "${ROOT}/third_party/sqlite/include")
target_compile_definitions(sqlite_p0 PRIVATE SQLITE_OMIT_LOAD_EXTENSION=1)

add_library(softbus6_common_p0 STATIC
  "${OH}/core/common/bitmap/softbus_bitmap.c"
  "${OH}/core/common/json_utils/softbus_json_utils.c"
  "${OH}/core/common/message_handler/message_handler.c"
  "${OH}/core/common/network/softbus_network_utils.c"
  "${OH}/core/common/queue/softbus_queue.c"
  "${OH}/core/common/softbus_property/softbus_feature_config.c"
  "${OH}/core/common/utils/softbus_tlv_utils.c"
  "${OH}/core/common/utils/softbus_utils.c"
  "${OH}/core/common/utils/sqlite3_utils.c"
)

target_include_directories(softbus6_common_p0 PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/stub"
  "${OH}/core/common/include"
  "${OH}/dfx/interface/include"
  "${OH}/include"
  "${OH}/adapter/common/include"
  "${OH}/adapter/common/include/OS_adapter_define/linux"
  "${ROOT}/base/include"
  "${ROOT}/third_party/cJSON/include"
  "${ROOT}/third_party/bounds_checking_function/include"
  "${ROOT}/third_party/sqlite"
  "${ROOT}/third_party/sqlite/include"
  "${ROOT}/third_party/include"
  "${ROOT}/third_party/openssl/include"
)

target_link_libraries(softbus6_common_p0 PUBLIC hilog_stub securec_p0 cjson_p0 sqlite_p0)

target_compile_definitions(softbus6_common_p0 PRIVATE
  DEFAULT_NET_INF_NAME_FOR_LINUX="eth0"
  NSTACKX_WITH_HMOS_LINUX=1
  FILLP_LINUX=1
)

target_compile_options(softbus6_common_p0 PRIVATE -Wall -Wextra -Wno-unused-parameter)

add_library(softbus6_adapter_p0 STATIC
  "${OH}/adapter/common/kernel/posix/lnn_ip_utils_adapter.c"
  "${OH}/adapter/common/kernel/posix/softbus_adapter_file.c"
  "${OH}/adapter/common/kernel/posix/softbus_adapter_mem.c"
  "${OH}/adapter/common/kernel/posix/softbus_adapter_socket.c"
  "${OH}/adapter/common/kernel/posix/softbus_adapter_thread.c"
  "${OH}/adapter/common/kernel/posix/softbus_adapter_timer.c"
  "${OH}/adapter/common/openssl/softbus_adapter_crypto.c"
  "${OH}/adapter/common/openssl/softbus_aes_encrypt.c"
  "${OH}/adapter/common/perf/softbus_adapter_perf.c"
  "${OH}/adapter/common/range/softbus_adapter_range.c"
)

target_include_directories(softbus6_adapter_p0 PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/stub"
  "${OH}/adapter/common/include"
  "${OH}/adapter/common/include/OS_adapter_define/linux"
  "${OH}/adapter/default_config"
  "${OH}/include"
  "${OH}/interfaces/inner_kits/lnn"
  "${OH}/dfx/interface/include"
  "${ROOT}/base/include"
  "${ROOT}/third_party/cJSON/include"
  "${ROOT}/third_party/bounds_checking_function/include"
  "${ROOT}/third_party/openssl/include"
)

target_compile_definitions(softbus6_adapter_p0 PRIVATE
  DEFAULT_NET_INF_NAME_FOR_LINUX="eth0"
  NSTACKX_WITH_HMOS_LINUX=1
  FILLP_LINUX=1
)
target_compile_options(softbus6_adapter_p0 PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(softbus6_adapter_p0 PUBLIC hilog_stub securec_p0 cjson_p0 sqlite_p0)

add_library(softbus6_nstackx_util_p0 STATIC
  "${OH}/components/nstackx/nstackx_util/core/nstackx_dev.c"
  "${OH}/components/nstackx/nstackx_util/core/nstackx_event.c"
  "${OH}/components/nstackx/nstackx_util/core/nstackx_getopt.c"
  "${OH}/components/nstackx/nstackx_util/core/nstackx_log.c"
  "${OH}/components/nstackx/nstackx_util/core/nstackx_openssl.c"
  "${OH}/components/nstackx/nstackx_util/core/nstackx_socket.c"
  "${OH}/components/nstackx/nstackx_util/core/nstackx_timer.c"
  "${OH}/components/nstackx/nstackx_util/core/nstackx_util.c"
  "${OH}/components/nstackx/nstackx_util/platform/unix/sys_dev.c"
  "${OH}/components/nstackx/nstackx_util/platform/unix/sys_epoll.c"
  "${OH}/components/nstackx/nstackx_util/platform/unix/sys_event.c"
  "${OH}/components/nstackx/nstackx_util/platform/unix/sys_log.c"
  "${OH}/components/nstackx/nstackx_util/platform/unix/sys_socket.c"
  "${OH}/components/nstackx/nstackx_util/platform/unix/sys_timer.c"
  "${OH}/components/nstackx/nstackx_util/platform/unix/sys_util.c"
)
target_include_directories(softbus6_nstackx_util_p0 PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/stub"
  "${OH}/components/nstackx/nstackx_util/interface"
  "${OH}/components/nstackx/nstackx_util/platform/unix"
  "${OH}/components/nstackx/nstackx_util"
  "${OH}/include"
  "${OH}/dfx/interface/include"
  "${ROOT}/base/include"
  "${ROOT}/third_party/securec/include"
  "${ROOT}/third_party/bounds_checking_function/include"
  "${ROOT}/third_party/cJSON/include"
  "${ROOT}/third_party/sqlite/include"
  "${ROOT}/third_party/openssl/include"
)
target_compile_definitions(softbus6_nstackx_util_p0 PRIVATE
  NSTACKX_WITH_HMOS_LINUX=1
  FILLP_LINUX=1
)
# OHOS' unix adapter assumes the build already exposes POSIX fcntl/flags.
target_compile_options(softbus6_nstackx_util_p0 PRIVATE -include fcntl.h)
target_link_libraries(softbus6_nstackx_util_p0 PUBLIC
  hilog_stub securec_p0 cjson_p0 sqlite_p0 ssl crypto pthread)

add_library(softbus6_nstackx_ctrl_p0 STATIC
  "${OH}/components/nstackx/nstackx_ctrl/core/json_payload.c"
  "${OH}/components/nstackx/nstackx_ctrl/core/nstackx_common.c"
  "${OH}/components/nstackx/nstackx_ctrl/core/nstackx_device.c"
  "${OH}/components/nstackx/nstackx_ctrl/core/nstackx_device_local.c"
  "${OH}/components/nstackx/nstackx_ctrl/core/nstackx_device_remote.c"
  "${OH}/components/nstackx/nstackx_ctrl/core/nstackx_dfinder_hidump.c"
  "${OH}/components/nstackx/nstackx_ctrl/core/nstackx_dfinder_hievent.c"
  "${OH}/components/nstackx/nstackx_ctrl/core/nstackx_dfinder_log.c"
  "${OH}/components/nstackx/nstackx_ctrl/core/nstackx_dfinder_mgt_msg_log.c"
  "${OH}/components/nstackx/nstackx_ctrl/core/nstackx_inet.c"
  "${OH}/components/nstackx/nstackx_ctrl/core/nstackx_smartgenius.c"
  "${OH}/components/nstackx/nstackx_ctrl/core/nstackx_statistics.c"
)
target_include_directories(softbus6_nstackx_ctrl_p0 PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/stub"
  "${OH}/components/nstackx/nstackx_ctrl/include"
  "${OH}/components/nstackx/nstackx_ctrl/include/coap_discover"
  "${OH}/components/nstackx/nstackx_ctrl/include/mini_discover"
  "${OH}/components/nstackx/nstackx_ctrl/interface"
  "${OH}/interfaces/kits/nstackx"
  "${OH}/components/nstackx/nstackx_util/interface"
  "${OH}/components/nstackx/nstackx_util/platform/unix"
  "${OH}/include"
  "${OH}/dfx/interface/include"
  "${ROOT}/base/include"
  "${ROOT}/third_party/bounds_checking_function/include"
  "${ROOT}/third_party/cJSON/include"
  "${ROOT}/third_party/sqlite/include"
  "${ROOT}/third_party/coap/include"
  "${ROOT}/third_party/openssl/include"
)
target_compile_definitions(softbus6_nstackx_ctrl_p0 PRIVATE
  NSTACKX_WITH_HMOS_LINUX=1
  FILLP_LINUX=1
)
target_link_libraries(softbus6_nstackx_ctrl_p0 PUBLIC
  softbus6_nstackx_util_p0 hilog_stub securec_p0 cjson_p0 sqlite_p0 ssl crypto pthread)

add_executable(log_smoke log_smoke.c)
target_link_libraries(log_smoke PRIVATE hilog_stub)
target_include_directories(log_smoke PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/stub"
  "${OH}/dfx/interface/include")

add_executable(deps_smoke deps_smoke.c)
target_link_libraries(deps_smoke PRIVATE cjson_p0 securec_p0 sqlite_p0)

add_executable(link_smoke link_smoke.c)
target_link_libraries(link_smoke PRIVATE
  softbus6_adapter_p0 softbus6_common_p0
  hilog_stub securec_p0 cjson_p0 sqlite_p0
  pthread ssl crypto)
EOF

cmake -S "$OUT" -B "$OUT/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$OUT/build" -j"${JOBS:-4}"
echo
echo "P0 common library:"
ls -l "$OUT/build/libsoftbus6_common_p0.a"
echo "P0 adapter library:"
ls -l "$OUT/build/libsoftbus6_adapter_p0.a"
echo "P0 deps:"
ls -l "$OUT/build/libsecurec_p0.a" "$OUT/build/libcjson_p0.a" "$OUT/build/libsqlite_p0.a"
echo "P0 nstackx util:"
ls -l "$OUT/build/libsoftbus6_nstackx_util_p0.a"
echo "P0 nstackx ctrl:"
ls -l "$OUT/build/libsoftbus6_nstackx_ctrl_p0.a"
echo "P0 hilog stub smoke:"
"$OUT/build/log_smoke"
echo "P0 deps smoke:"
"$OUT/build/deps_smoke"
echo "P0 link smoke:"
"$OUT/build/link_smoke"
