# hush_privacy_source_audit — regression tripwire (not a DAW checker)
if(NOT HUSH_SOURCE_ROOT)
  message(FATAL_ERROR "HUSH_SOURCE_ROOT required")
endif()
set(DENYLIST
  "curl_easy"
  "curl_multi"
  "libcurl"
  "Sentry"
  "sentry_init"
  "Firebase"
  "firebase::"
  "Mixpanel"
  "Amplitude"
  "Segment."
  "analytics.track"
  "Crashlytics"
  "Bugsnag"
  "AppCenter"
  "HockeyApp"
  "socket.io"
  "WebSocketClient"
  "httplib::Client"
  "cpr::"
  "restclient"
)
file(GLOB_RECURSE SOURCES
  "${HUSH_SOURCE_ROOT}/engine/*.[ch]pp"
  "${HUSH_SOURCE_ROOT}/cosmic-studio/include/*.[ch]pp"
  "${HUSH_SOURCE_ROOT}/cosmic-studio/tests/*.[ch]pp"
)
set(HIT "")
foreach(f ${SOURCES})
  file(READ "${f}" content)
  foreach(pat ${DENYLIST})
    string(FIND "${content}" "${pat}" pos)
    if(NOT pos EQUAL -1)
      set(HIT "${HIT}\n  ${f}: ${pat}")
    endif()
  endforeach()
endforeach()
if(HIT)
  message(FATAL_ERROR "Privacy denylist hit:${HIT}")
endif()
message(STATUS "hush_privacy_source_audit: PASS (no denylist hits)")
