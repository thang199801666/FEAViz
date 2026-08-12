cmake_minimum_required(VERSION 3.24)
project(FEAVizInstallConsumer LANGUAGES C)

find_package(FEAViz 0.16 REQUIRED CONFIG)

add_executable(FEAVizInstallConsumer main.c)
target_link_libraries(FEAVizInstallConsumer PRIVATE FEAViz::FEAViz)
