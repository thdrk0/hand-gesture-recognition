execute_process(COMMAND "${REPLAY}" "${INPUT}" RESULT_VARIABLE status OUTPUT_VARIABLE output)
set(expected "{\"time_ms\":600,\"gesture\":1,\"action\":\"selection_1\"}\n{\"time_ms\":2500,\"gesture\":1,\"action\":\"selection_1\"}\n{\"time_ms\":3200,\"gesture\":4,\"action\":\"selection_4\"}\n{\"time_ms\":3900,\"gesture\":5,\"action\":\"selection_5\"}\n")
string(REPLACE "\r\n" "\n" output "${output}")
if(NOT status EQUAL 0 OR NOT output STREQUAL expected)
    message(FATAL_ERROR "Replay failed (${status}): ${output}")
endif()

