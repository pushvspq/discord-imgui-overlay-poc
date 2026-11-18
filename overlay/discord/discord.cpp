#include "discord.hpp"

#include "pages/pages.hpp"
#include "imgui/imgui.h"

#include "util/logger/logger.hpp"
#include "util/proc/proc.hpp"

#include <httplib.h>

namespace discord {
	template<typename _Ty>
	static void raw_append(std::vector<char>& buf, const _Ty& val) {
		auto p = reinterpret_cast<const char*>(&val);
		buf.insert(buf.end(), p, p + sizeof(_Ty));
	}

	static void raw_append(std::vector<char>& buf, const void* data, std::size_t len) {
		auto p = static_cast<const char*>(data);
		buf.insert(buf.end(), p, p + len);
	}

  static void write_cmd_list(ImDrawList* dl, std::vector<char>& buf) {
    const auto n_vtx = static_cast<std::uint32_t>(dl->VtxBuffer.Size);

    raw_append(buf, n_vtx);

    if (n_vtx > 0)
      raw_append(buf, dl->VtxBuffer.Data, n_vtx * sizeof(ImDrawVert));

    const auto o_idx = static_cast<uint32_t>(dl->IdxBuffer.Size);
    std::uint32_t n_idx = o_idx;

    if (o_idx % 2 == 1)
      ++n_idx;

    raw_append(buf, n_idx);

    if (o_idx > 0)
      raw_append(buf, dl->IdxBuffer.Data, o_idx * sizeof(ImDrawIdx));

    if (o_idx % 2 == 1) {
      std::uint16_t pad{};
      raw_append(buf, pad);
    }

    std::uint32_t n_cmd{};

    for (int i{}; i < dl->CmdBuffer.Size; ++i)
      if (!dl->CmdBuffer[i].UserCallback)
        ++n_cmd;

    raw_append(buf, n_cmd);

    for (int i{}; i < dl->CmdBuffer.Size; ++i) {
      const auto& c = dl->CmdBuffer[i];

      if (c.UserCallback)
        continue;

      const auto n_elem = c.ElemCount;
      const auto tex_id = static_cast<std::uint32_t>(c.GetTexID());
      const auto v_off = static_cast<std::uint32_t>(c.VtxOffset);
      const auto i_off = static_cast<std::uint32_t>(c.IdxOffset);

      raw_append(buf, n_elem);
      raw_append(buf, tex_id);
      raw_append(buf, v_off);
      raw_append(buf, i_off);
      raw_append(buf, c.ClipRect);
    }
  }

  static std::vector<char> build_tex_msg(
    std::uint32_t id,
    std::int32_t type,
    std::int32_t w, std::int32_t h,
    std::int32_t rev,
    const void* pixels,
    std::size_t sz
  ) {
    std::vector<char> m{};

    m.reserve(21 + sz);
    m.push_back(0x01);

    raw_append(m, id);
    raw_append(m, type);
    raw_append(m, w);
    raw_append(m, h);
    raw_append(m, rev);
    raw_append(m, pixels, sz);

    return m;
  }

  static void build_frame_msg(ImDrawData* dd, std::vector<char>& out) {
    out.clear();

    std::size_t est = 5;

    for (int i{}; i < dd->CmdListsCount; ++i) {
      const auto* dl = dd->CmdLists[i];

      est += 4
        + 4 + dl->VtxBuffer.Size * sizeof(ImDrawVert)
        + 4 + (dl->IdxBuffer.Size + (dl->IdxBuffer.Size % 2)) * sizeof(ImDrawIdx)
        + 4 + dl->CmdBuffer.Size * 32;
    }

    out.reserve(est);
    out.push_back(0x02);

    const auto n = static_cast<std::uint32_t>(dd->CmdListsCount);
    raw_append(out, n);

    for (int i{}; i < dd->CmdListsCount; ++i) {
      const auto size_pos = out.size();
      std::uint32_t placeholder{};
      raw_append(out, placeholder);

      const auto data_start = out.size();
      write_cmd_list(dd->CmdLists[i], out);

      const auto list_size = static_cast<std::uint32_t>(out.size() - data_start);
      std::memcpy(out.data() + size_pos, &list_size, sizeof(list_size));
    }
  }

	bool main() {
    HWND hwnd{};

    while (!(hwnd = ::FindWindowW(L"UnityWndClass", nullptr)))
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

    DWORD pid{};
    ::GetWindowThreadProcessId(hwnd, &pid);

    logger::info("Game PID: {}", pid);

		httplib::Server svr{};
		svr.set_websocket_ping_interval(0);

    const auto replace = [](std::string& str, const char* tok, int val) {
      std::string s = std::to_string(val);

      for (std::size_t p; (p = str.find(tok)) != std::string::npos;)
        str.replace(p, strlen(tok), s);
      };

    std::string app = pages::APP_HTML;
    std::string popout = pages::POPOUT_HTML;

    replace(app, "__PORT__", PORT);
    replace(app, "__PID__", pid);

    replace(popout, "__PORT__", PORT);

    svr.Get("/popout", [&](const httplib::Request&, httplib::Response& res) {
      res.set_content(popout, "text/html; charset=utf-8");
      });

    svr.set_error_handler([&](const httplib::Request& req, httplib::Response& res) {
      res.set_content(app, "text/html; charset=utf-8");
      res.status = 200;
      });

    std::mutex mtx{};

    svr.WebSocket("/ws", [&](const httplib::Request&, httplib::ws::WebSocket& ws) {
      std::unique_lock lock(mtx, std::try_to_lock);

      if (!lock.owns_lock()) {
        ws.close(httplib::ws::CloseStatus::Normal, "another client active");
        return;
      }

      ImGuiContext* ctx = ImGui::CreateContext();
      ImGui::SetCurrentContext(ctx);

      auto& io = ImGui::GetIO();

      io.MouseDrawCursor = false;
      io.DisplaySize = ImVec2(1280.f, 800.f);
      io.IniFilename = nullptr;

      ImGui::StyleColorsDark();
      auto& style = ImGui::GetStyle();

      style.AntiAliasedFill = false;
      style.AntiAliasedLines = false;
      style.WindowRounding = 0.f;
      style.ScrollbarRounding = 0.f;

      unsigned char* pixels; int texW, texH;
      io.Fonts->GetTexDataAsAlpha8(&pixels, &texW, &texH);

      auto t0 = std::chrono::steady_clock::now();
      std::string msg{};
      std::vector<char> frame_buf{};

      while (ws.read(msg)) {
        if (msg.starts_with("init")) {
          const auto tex = build_tex_msg(0, 0, texW, texH, 1, pixels, static_cast<size_t>(texW) * texH);
          ws.send(tex.data(), tex.size());
        }
        else if (msg.starts_with("frame")) {
          RECT rc{};

          if (::GetClientRect(hwnd, &rc))
            io.DisplaySize = ImVec2(static_cast<float>(rc.right), static_cast<float>(rc.bottom));

          POINT pt{};

          ::GetCursorPos(&pt);
          ::ScreenToClient(hwnd, &pt);

          io.AddMousePosEvent(static_cast<float>(pt.x), static_cast<float>(pt.y));
          io.AddMouseButtonEvent(ImGuiMouseButton_Left,
            (::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
          io.AddMouseButtonEvent(ImGuiMouseButton_Right,
            (::GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
          io.AddMouseButtonEvent(ImGuiMouseButton_Middle,
            (::GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0);

          auto now = std::chrono::steady_clock::now();
          float dt = std::chrono::duration<float>(now - t0).count();

          t0 = now;
          io.DeltaTime = (dt > 0.f && dt < 1.f) ? dt : 1.f / 60.f;

          ImGui::NewFrame();
          ImGui::ShowDemoWindow();

          ImGui::Render();

          build_frame_msg(ImGui::GetDrawData(), frame_buf);
          ws.send(frame_buf.data(), frame_buf.size());
        }
      }

      ImGui::DestroyContext(ctx);
      });

		if (!svr.bind_to_port("127.0.0.1", PORT)) {
			logger::error("Failed to bind server at port {}", PORT);
			return false;
		}

		std::thread([&]() -> void {
			svr.listen_after_bind();
      logger::info("Server listening on 127.0.0.1:{}", PORT);
			}).detach();

    proc::kill(L"Update.exe");
    proc::kill(L"Discord.exe");

    {
      const auto local_app_data = std::getenv("LOCALAPPDATA");

      if (!local_app_data) {
        logger::error("LOCALAPPDATA not set");
        return false;
      }

      std::filesystem::path discord_dir = std::filesystem::path(local_app_data) / "Discord";

      if (!std::filesystem::exists(discord_dir)) {
        logger::error("Discord not found");
        return false;
      }

      std::string exe{};

      for (const auto& entry : std::filesystem::directory_iterator(discord_dir)) {
        if (!entry.is_directory())
          continue;

        const auto name = entry.path().filename().string();

        if (!name.starts_with("app-"))
          continue;

        auto path = (entry.path() / "Discord.exe").string();

        if (std::filesystem::exists(path) && path > exe)
          exe = std::move(path);
      }

      if (exe.empty()) {
        logger::error("Discord executable not found");
        return false;
      }

      const auto wexe = std::filesystem::path(exe).wstring();
      const auto wdir = std::filesystem::path(exe).parent_path().wstring();

      auto env_src = ::GetEnvironmentStringsW();
      std::wstring env_block{};

      for (auto p = env_src; *p; p += wcslen(p) + 1) {
        env_block.append(p);
        env_block.push_back(L'\0');
      }

      ::FreeEnvironmentStringsW(env_src);

      env_block += L"DISCORD_WEBAPP_ENDPOINT=http://127.0.0.1:" + std::to_wstring(PORT);
      env_block.push_back(L'\0');
      env_block.push_back(L'\0');

      STARTUPINFOW si{};
      si.cb = sizeof(si);

      PROCESS_INFORMATION pi{};

      ::CreateProcessW(
        wexe.c_str(), nullptr, nullptr, nullptr, FALSE,
        CREATE_NEW_PROCESS_GROUP | CREATE_UNICODE_ENVIRONMENT,
        env_block.data(), wdir.c_str(), &si, &pi
      );

      logger::info("Discord launched (PID {})", pi.dwProcessId);

      ::CloseHandle(pi.hProcess);
      ::CloseHandle(pi.hThread);
    }

    while (true)
		  std::this_thread::sleep_for(std::chrono::milliseconds(100));

		return true;
	}
} // namespace discord
