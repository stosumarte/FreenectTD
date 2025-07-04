get_filename_component(CONFIG_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)

set(FREENECT_INCLUDE_DIRS "${CONFIG_DIR}/../../include/libfreenect")
set(FREENECT_RUNTIME_LIBRARY_DIRS "${CONFIG_DIR}/../../bin")
set(FREENECT_LIBRARY_DIRS "${CONFIG_DIR}/../../lib")
#/libfreenect.so.0.7.5
set(FREENECT_LIBRARIES "freenect")
set(FREENECT_VERSION "0.7.5")
