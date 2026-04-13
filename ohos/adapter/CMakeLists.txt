cmake_minimum_required(VERSION 3.16)

project(adapter)

set(CMAKE_CXX_STANDARD 20)
set(ADAPTER_SRC
    native_initializer.cc
    ohos_adapter.cc
)
add_library(adapter SHARED ${ADAPTER_SRC})
target_include_directories(adapter PUBLIC ../../ ./include/)

add_subdirectory(../../third_party/aki aki)
add_subdirectory(accessibility)
add_subdirectory(aki_hook)
add_subdirectory(battery)
add_subdirectory(bluetooth)
add_subdirectory(browser)
add_subdirectory(cert_manager)
add_subdirectory(common)
add_subdirectory(context)
add_subdirectory(context_path)
add_subdirectory(cursor)
add_subdirectory(default_application)
add_subdirectory(device)
add_subdirectory(device_info)
add_subdirectory(device_user_auth)
add_subdirectory(drag_drop)
add_subdirectory(external_protocol)
add_subdirectory(file_manager)
add_subdirectory(file_picker)
add_subdirectory(font)
add_subdirectory(geolocation)
add_subdirectory(ime_adapter)
add_subdirectory(life_cycle)
add_subdirectory(media_manager)
add_subdirectory(mime_type)
add_subdirectory(multiprocess)
add_subdirectory(native_theme)
add_subdirectory(net)
# add_subdirectory(node_handle)
add_subdirectory(notification)
add_subdirectory(ocr)
add_subdirectory(ohos_i18n)
add_subdirectory(permission_manager)
add_subdirectory(policy)
add_subdirectory(power_monitor)
add_subdirectory(print)
add_subdirectory(res_sched)
add_subdirectory(runninglock)
add_subdirectory(screen)
add_subdirectory(screenlock_monitor)
add_subdirectory(screenshot)
add_subdirectory(shape_detection)
add_subdirectory(speech)
add_subdirectory(task_runner)
add_subdirectory(tracing)
add_subdirectory(web_entry)
add_subdirectory(window)
add_subdirectory(xcomponent)

target_link_libraries(adapter PUBLIC aki_jsbind ohcommonevent hitrace_ndk.z native_window native_window_manager ohinput ace_ndk.z 
ace_napi.z deviceinfo_ndk.z ohfileuri image_ndk.z image_receiver_ndk.z time_service_ndk child_process bundle_ndk.z 
ipc_capi)

target_compile_definitions(adapter
    PRIVATE
        USE_LOADER_TARGET=2
        LIBADAPTER_SOURCE
        ENABLE_START_ISOLATE_PROCESS)
