#ifndef OHOS_ADAPTER_MULTIPROCESS_CHILD_PROCESS_CHILD_PROCESS_H_
#define OHOS_ADAPTER_MULTIPROCESS_CHILD_PROCESS_CHILD_PROCESS_H_

#include "ohos/adapter/ipc/native_ipc.h"
#include "ohos/adapter/multiprocess/child_process/child_process_stub.h"
#include "ohos/adapter/multiprocess/child_process/process_manager_proxy.h"
#include "ohos/adapter/multiprocess/command_line/command_line_helper.h"

namespace ohos::adapter::multiprocess {
class ChildProcess {
public:

    static ChildProcess& Instance() {
        static ChildProcess instance;
        return instance;
    }

    int CreateProcess(const std::vector<std::string>& commands,
                      const std::vector<std::pair<int, int>>& fds) {
        return GetProxy()->CreateProcess(commands, fds);
    }

    int KillProcess(int32_t pid) {
        return GetProxy()->KillProcess(pid);
    }

    int CreateWindow(const ipc::WindowParam& param) {
        return GetProxy()->CreateWindow(param);
    }

    int UpdateWindow(int32_t widget_id, const ipc::WindowParam& param) {
        return GetProxy()->UpdateWindow(widget_id, param);
    }

    int SetWindowBounds(int32_t widget_id, const WindowRect& rect) {
        return GetProxy()->SetWindowBounds(widget_id, rect);
    }

    int SetWindowVisible(int widget_id, int visible) {
        return GetProxy()->SetWindowVisible(widget_id, visible);
    }

    int CloseWindow(int32_t widget_id) {
        return GetProxy()->CloseWindow(widget_id);
    }

    int GetDisplay(ipc::DisplayParam& display) {
        return GetProxy()->GetDisplay(display);
    }

    int OpenUrl(const std::string& url) {
        return GetProxy()->OpenUrl(url);
    }

    std::shared_ptr<ChildProcessStub> GetChildProcessStub() {
        return childProcessStub;
    }

    std::shared_ptr<ProcessManagerProxy> GetProxy() {
        if (processManagerProxy == nullptr) {
            auto remote = childProcessStub->GetProcessManager()->Get();
            processManagerProxy = std::make_shared<ProcessManagerProxy>(remote);
        }
        return processManagerProxy;
    }

private:
    std::shared_ptr<ProcessManagerProxy> processManagerProxy = nullptr;
    std::shared_ptr<ChildProcessStub> childProcessStub = std::make_shared<ChildProcessStub>();
};
} // namespace ohos::adapter::multiprocess
#endif // OHOS_ADAPTER_MULTIPROCESS_CHILD_PROCESS_CHILD_PROCESS_H_