include(CTest)

macro(wirecall_test name)
    add_executable(wirecall-tests-${name} ${name}.cpp)
    target_link_libraries(wirecall-tests-${name} wirecall wirecall-asio)
    add_test(wirecall-tests-${name} wirecall-tests-${name})

    add_executable(wirecall-tests-single-header-${name} ${name}.cpp)
    target_link_libraries(wirecall-tests-single-header-${name} wirecall-single-header wirecall-asio)
    add_test(wirecall-tests-single-header-${name} wirecall-tests-single-header-${name})
endmacro()

foreach(test ipc demo)
    wirecall_test(${test})
endforeach()
