#include "proc.hpp"

namespace proc {
	void kill(const std::wstring name) {
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(PROCESSENTRY32W);

    const auto snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (::Process32FirstW(snapshot, &entry)) {
      while (::Process32NextW(snapshot, &entry))  {
        if (::_wcsicmp(entry.szExeFile, name.c_str()) == 0) {
          const auto handle = ::OpenProcess(PROCESS_TERMINATE, false, entry.th32ProcessID);

          if (!handle
            || handle == INVALID_HANDLE_VALUE)
            continue;

          ::TerminateProcess(handle, -1);
          ::CloseHandle(handle);
        }
      }
    }

    ::CloseHandle(snapshot);
	}
} // namespace proc
