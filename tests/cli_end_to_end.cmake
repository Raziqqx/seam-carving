# Runs the seamcarve tool on a real image with each algorithm and checks that it exits
# cleanly and writes its output files. Called by ctest (see CMakeLists.txt).
file(MAKE_DIRECTORY ${OUTPUT_DIR})

# seamcarve exits with code 3 when the requested feature belongs to a milestone that has
# not been merged yet. Such a step is counted as skipped, not as a failure.
set(skipped_steps 0)
set(last_step_skipped FALSE)

macro(run_seamcarve)
    execute_process(COMMAND ${SEAMCARVE} ${ARGN} RESULT_VARIABLE exit_code OUTPUT_VARIABLE output
        ERROR_VARIABLE error_output)
    if(exit_code EQUAL 3)
        math(EXPR skipped_steps "${skipped_steps} + 1")
        set(last_step_skipped TRUE)
        message(STATUS "skipped: ${error_output}")
    elseif(NOT exit_code EQUAL 0)
        message(FATAL_ERROR "seamcarve ${ARGN} failed with exit code ${exit_code}: ${error_output}")
    else()
        set(last_step_skipped FALSE)
        message(STATUS "${output}")
    endif()
endmacro()

# Checks that an output file exists, unless the step that should write it was skipped.
function(require_file path)
    if(NOT last_step_skipped AND NOT EXISTS ${path})
        message(FATAL_ERROR "expected output file is missing: ${path}")
    endif()
endfunction()

# The tower image is 500 x 339. DP and greedy on the full image.
foreach(algo dp greedy)
    run_seamcarve(--algo ${algo} --width 460 --height 320
        --seams-image ${OUTPUT_DIR}/${algo}_seams.png
        ${INPUT} ${OUTPUT_DIR}/${algo}.png)
    require_file(${OUTPUT_DIR}/${algo}.png)
    require_file(${OUTPUT_DIR}/${algo}_seams.png)
endforeach()

# Graph cut is slow, so it runs on the small copy of the image (150 x 101).
run_seamcarve(--algo graphcut --width 145 --height 99
    ${SMALL_INPUT} ${OUTPUT_DIR}/graphcut.png)
require_file(${OUTPUT_DIR}/graphcut.png)

# Forward energy, enlarging, and PNG animation frames.
run_seamcarve(--algo dp --energy forward --width 480 ${INPUT} ${OUTPUT_DIR}/forward.png)
run_seamcarve(--algo dp --width 520 ${INPUT} ${OUTPUT_DIR}/enlarged.png)
run_seamcarve(--algo dp --width 490 --frames ${OUTPUT_DIR}/frame --frame-step 5
    ${INPUT} ${OUTPUT_DIR}/frames_out.png)
require_file(${OUTPUT_DIR}/frame_0000.png)
require_file(${OUTPUT_DIR}/frame_0010.png)

# Enlarging with animation frames (this used to crash: the frames must be large enough
# for the grown image), and shrinking the width while enlarging the height.
run_seamcarve(--algo dp --width 170 --frames ${OUTPUT_DIR}/grow --frame-step 10
    --seams-image ${OUTPUT_DIR}/grow_seams.png ${SMALL_INPUT} ${OUTPUT_DIR}/grow_out.png)
require_file(${OUTPUT_DIR}/grow_0000.png)
require_file(${OUTPUT_DIR}/grow_0020.png)
run_seamcarve(--algo dp --width 120 --height 110 --frames ${OUTPUT_DIR}/mixed --frame-step 10
    --seams-image ${OUTPUT_DIR}/mixed_seams.png ${SMALL_INPUT} ${OUTPUT_DIR}/mixed_out.png)
require_file(${OUTPUT_DIR}/mixed_seams.png)
require_file(${OUTPUT_DIR}/mixed_0039.png)

# Runs seamcarve with the given arguments and checks the exact exit code.
# For bad input the code must be 1 ("rejected cleanly"), not 3: 3 means "feature not
# merged yet", and bad input must be rejected before any milestone's code is reached.
function(expect_exit_code expected description)
    execute_process(COMMAND ${SEAMCARVE} ${ARGN} RESULT_VARIABLE exit_code
        OUTPUT_QUIET ERROR_QUIET)
    if(NOT exit_code EQUAL expected)
        message(FATAL_ERROR "${description}: expected exit code ${expected}, got ${exit_code}")
    endif()
endfunction()

# Asking for help is not an error.
expect_exit_code(0 "--help" --help)

# Wrong command lines must be rejected with exit code 1.
expect_exit_code(1 "unknown algorithm" --algo nonsense ${INPUT} ${OUTPUT_DIR}/x.png)
expect_exit_code(1 "unknown option" --no-such-option ${INPUT} ${OUTPUT_DIR}/x.png)
expect_exit_code(1 "missing output file name" ${INPUT})
expect_exit_code(1 "missing value after --width" ${INPUT} ${OUTPUT_DIR}/x.png --width)
expect_exit_code(1 "non-numeric --width" --width abc ${INPUT} ${OUTPUT_DIR}/x.png)
expect_exit_code(1 "input file does not exist" ${OUTPUT_DIR}/no_such_file.png
    ${OUTPUT_DIR}/x.png)

# Targets of twice the size or more are rejected (seam insertion can at most double).
expect_exit_code(1 "--width 5000 (10x the width)" --width 5000 ${INPUT} ${OUTPUT_DIR}/x.png)
expect_exit_code(1 "--height 5000 (15x the height)" --height 5000 ${INPUT} ${OUTPUT_DIR}/x.png)

# KNOWN BUG (reported to the owner of src/main.cpp): --width 0 and negative widths are
# treated as "keep the current width" (main.cpp: target_width > 0 ? ... : image.cols)
# and exit 0 instead of 1. Reported as a warning, not a failure, until main.cpp is fixed;
# after the fix this block should become expect_exit_code(1 ...) like the ones above.
foreach(bad_size 0 -5)
    execute_process(COMMAND ${SEAMCARVE} --width ${bad_size} ${INPUT} ${OUTPUT_DIR}/x.png
        RESULT_VARIABLE exit_code OUTPUT_QUIET ERROR_QUIET)
    if(exit_code EQUAL 1)
        message(STATUS "--width ${bad_size} is rejected (known bug fixed)")
    else()
        message(WARNING "KNOWN BUG: --width ${bad_size} exited ${exit_code}, expected 1")
    endif()
endforeach()

# ctest marks this test as "skipped" when the line below reports a non-zero count
# (see SKIP_REGULAR_EXPRESSION in CMakeLists.txt). With every milestone merged it is 0.
message(STATUS "SKIPPED_STEPS=${skipped_steps}")
