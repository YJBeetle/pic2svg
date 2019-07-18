cmake_minimum_required(VERSION 3.4)
set (CMAKE_CXX_STANDARD 11)

# SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb -DDEBUG")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

include_directories(/usr/local/include)
link_directories(/usr/local/lib)

project(pic2svg)
aux_source_directory(./src SRC_LIST)
add_executable(${PROJECT_NAME} ${SRC_LIST})

target_link_libraries(${PROJECT_NAME} opencv_shape opencv_stitching opencv_superres opencv_videostab opencv_aruco opencv_bgsegm opencv_bioinspired opencv_ccalib opencv_datasets opencv_dpm opencv_face opencv_freetype opencv_fuzzy opencv_hdf opencv_line_descriptor opencv_optflow opencv_video opencv_plot opencv_reg opencv_saliency opencv_stereo opencv_structured_light opencv_phase_unwrapping opencv_rgbd opencv_viz opencv_surface_matching opencv_text opencv_ximgproc opencv_calib3d opencv_features2d opencv_flann opencv_xobjdetect opencv_objdetect opencv_ml opencv_xphoto opencv_highgui opencv_videoio opencv_imgcodecs opencv_photo opencv_imgproc opencv_core)
configure_file(example/test.png example/test.png COPYONLY)