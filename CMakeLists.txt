cmake_minimum_required(VERSION 3.16)
project(PlaceBridge VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql PrintSupport CorePrivate Network)

option(BUILD_REST_SERVER "Build the optional token-authenticated REST server" OFF)
option(ENABLE_WARNINGS "Compile with extra compiler warnings (-Wall -Wextra / /W4)" ON)

function(placebridge_warnings target)
    if(ENABLE_WARNINGS)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra)
        endif()
    endif()
endfunction()

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/student
    ${CMAKE_SOURCE_DIR}/company
    ${CMAKE_SOURCE_DIR}/placement_officer
    ${CMAKE_SOURCE_DIR}/ui
)

add_executable(PlaceBridge
    main.cpp
    mainwindow.h
    DataManager.h
    DataManager.cpp
    SecurityManager.h
    SecurityManager.cpp
    SystemServices.h
    SystemServices.cpp
    student/student.h
    student/resume_data.h
    company/company.h
    placement_officer/placement_officer.h
    ui/theme.h
    ui/context.h
    ui/dialogs.h
    ui/search_dialog.h
    ui/login_page.h
    ui/resume_preview.h
    ui/resume_studio.h
    ui/dashboard_page.h
    ui/drives_page.h
    ui/students_page.h
    ui/students_page.cpp
    ui/calendar_page.h
    ui/reports_page.h
    ui/student_portal.h
    ui/recruiter_page.h
    ui/applications_page.h
    ui/applications_page.cpp
    ui/activity_page.h
    ui/activity_page.cpp
    ui/settings_page.h
    ui/settings_page.cpp
)

target_link_libraries(PlaceBridge PRIVATE Qt6::Widgets Qt6::Sql Qt6::PrintSupport
    Qt6::CorePrivate Qt6::Network)
placebridge_warnings(PlaceBridge)

if(BUILD_REST_SERVER)
    find_package(Qt6 REQUIRED COMPONENTS HttpServer)
    add_executable(PlaceBridgeServer server/rest_server.cpp
        DataManager.h
        DataManager.cpp
        SecurityManager.h
        SecurityManager.cpp
        SystemServices.h
        SystemServices.cpp
        student/student.h
        company/company.h
    )
    target_link_libraries(PlaceBridgeServer PRIVATE Qt6::Core Qt6::Sql
        Qt6::Network Qt6::HttpServer)
    placebridge_warnings(PlaceBridgeServer)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(PlaceBridgeTests
        tests/test_placebridge.cpp
        DataManager.h
        DataManager.cpp
        SecurityManager.h
        SecurityManager.cpp
        SystemServices.h
        SystemServices.cpp
        student/student.h
        company/company.h
    )
    target_link_libraries(PlaceBridgeTests PRIVATE Qt6::Widgets Qt6::Sql Qt6::Test Qt6::Network)
    placebridge_warnings(PlaceBridgeTests)
    add_test(NAME PlaceBridgeTests COMMAND PlaceBridgeTests)

    add_executable(PlaceBridgeLoginTests
        tests/test_login_page.cpp
        DataManager.h
        DataManager.cpp
        SecurityManager.h
        SecurityManager.cpp
        SystemServices.h
        SystemServices.cpp
        student/student.h
        company/company.h
        ui/theme.h
        ui/context.h
        ui/dialogs.h
        ui/login_page.h
    )
    target_link_libraries(PlaceBridgeLoginTests PRIVATE Qt6::Widgets Qt6::Sql Qt6::Test
        Qt6::PrintSupport Qt6::Network Qt6::CorePrivate)
    placebridge_warnings(PlaceBridgeLoginTests)
    add_test(NAME PlaceBridgeLoginTests COMMAND PlaceBridgeLoginTests)
    set_tests_properties(PlaceBridgeLoginTests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    # Whole-UI smoke test: every page for every role, plus the main workflows.
    add_executable(PlaceBridgePageTests
        tests/test_pages.cpp
        DataManager.h
        DataManager.cpp
        SecurityManager.h
        SecurityManager.cpp
        SystemServices.h
        SystemServices.cpp
        student/student.h
        student/resume_data.h
        company/company.h
        placement_officer/placement_officer.h
        ui/theme.h
        ui/context.h
        ui/dialogs.h
        ui/search_dialog.h
        ui/login_page.h
        ui/resume_preview.h
        ui/resume_studio.h
        ui/dashboard_page.h
        ui/drives_page.h
        ui/students_page.h
        ui/students_page.cpp
        ui/calendar_page.h
        ui/reports_page.h
        ui/student_portal.h
        ui/recruiter_page.h
        ui/applications_page.h
        ui/applications_page.cpp
        ui/activity_page.h
        ui/activity_page.cpp
        ui/settings_page.h
        ui/settings_page.cpp
    )
    target_link_libraries(PlaceBridgePageTests PRIVATE Qt6::Widgets Qt6::Sql Qt6::Test
        Qt6::PrintSupport Qt6::Network Qt6::CorePrivate)
    placebridge_warnings(PlaceBridgePageTests)
    add_test(NAME PlaceBridgePageTests COMMAND PlaceBridgePageTests)
    set_tests_properties(PlaceBridgePageTests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(PlaceBridgeResumeTests
        tests/test_resume_studio.cpp
        DataManager.h
        DataManager.cpp
        SecurityManager.h
        SecurityManager.cpp
        SystemServices.h
        SystemServices.cpp
        student/student.h
        student/resume_data.h
        company/company.h
        ui/theme.h
        ui/context.h
        ui/resume_preview.h
        ui/resume_studio.h
    )
    target_link_libraries(PlaceBridgeResumeTests PRIVATE Qt6::Widgets Qt6::Sql Qt6::Test
        Qt6::PrintSupport Qt6::Network)
    placebridge_warnings(PlaceBridgeResumeTests)
    add_test(NAME PlaceBridgeResumeTests COMMAND PlaceBridgeResumeTests)
    set_tests_properties(PlaceBridgeResumeTests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
