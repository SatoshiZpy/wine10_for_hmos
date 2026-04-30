target_sources(adapter PRIVATE
    adapter/app_window_adapter.cc
    adapter/sub_window_adapter.cc
    adapter/system_floating_window_adapter.cc
    window_common.cc
    wine_float_window.cc
    wine_popup_window.cc
    wine_toplevel_window.cc
    wine_window_factory.cc
    wine_window_manager.cc
    wine_window.cc)