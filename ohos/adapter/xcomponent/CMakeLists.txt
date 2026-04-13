target_sources(adapter PRIVATE
    xcomponent_manager.cc
    xcomponent_utils.cc
    adapter/input_event_callback_adapter.cc
    renderer/xcomponent_impl.cc
    adapter/window_adapter.cc
    event/gesture_event_receiver.cc
    event/window_event_common.cc
    event/window_event_filter_adapter.cc
    event/window_event_receiver.cc
    )
