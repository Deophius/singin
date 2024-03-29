// Implementation for Singer class's mainloop()
#include <boost/asio.hpp>
#include "singd.h"
#include <cstdlib>

namespace Spirit {
    using nlohmann::json;

    Singer::Singer(const Spirit::Configuration& config) :
        mConfig(config)
    {}

    void Singer::mainloop(Watchdog& watchdog, Logfile& logfile) {
        namespace asio = boost::asio;
        using asio::ip::udp;
        // If intro or serv_port are missing, no need to go on.
        logfile << mConfig["intro"].get<std::string>() << '\n';
        asio::io_context ioc;
        udp::socket serv_sock(ioc, udp::endpoint(udp::v4(), mConfig["serv_port"]));
        logfile << "Created socket, bound to " << mConfig["serv_port"] << '\n';
        mLocalData.reset(new Connection(mConfig["dbname"], mConfig["passwd"]));
        logfile << "Opened local data, singer's instance\n";
        logfile.flush();
        while (true) {
            udp::endpoint client;
            // Get the request
            nlohmann::json request;
            // The result to be returned
            nlohmann::json result;
            // Make sure to flush logs
            LogSection log_section(logfile);
            // True if should be dispatched
            bool dispatch = true;
            try {
                asio::streambuf req_buf;
                req_buf.commit(serv_sock.receive_from(req_buf.prepare(1024), client));
                std::istream rfile(&req_buf);
                rfile >> request;
                logfile << client << ": " << request.dump() << std::endl;
            } catch (const boost::system::system_error& ex) {
                logfile << ex.what() << std::endl;
                continue;
            } catch (nlohmann::json::parse_error& ex) {
                logfile << ex.what() << std::endl;
                result["success"] = false;
                result["what"] = "Unrecognized format, "s + ex.what();
                dispatch = false;
            }
            if (dispatch) {
                if (!request.contains("command")) {
                    result["success"] = false;
                    result["what"] = "Missing command!";
                } else {
                    const std::string command = request["command"];
                    if (command == "report_absent")
                        result = handle_rep_abs(request, logfile);
                    else if (command == "write_record")
                        result = handle_wrt_rec(request, logfile);
                    else if (command == "restart_gs")
                        result = handle_restart(request, logfile);
                    else if (command == "today_info")
                        result = handle_today(request, logfile);
                    else if (command == "quit_spirit") {
                        logfile << "Stopping on request from client!\n";
                        result["success"] = true;
                        serv_sock.send_to(boost::asio::buffer(result.dump()), client);
                        return;
                    } else if (command == "flush_notice")
                        result = handle_notice(request, logfile);
                    else if (command == "doggie_stick")
                        result = handle_doggie(request, logfile, watchdog);
                    else {
                        result["success"] = false;
                        result["what"] = "Unknown command!";
                    }
                }
            }
            auto result_dumped = result.dump();
            logfile << "Generated response: " << result_dumped << std::endl;
            try {
                serv_sock.send_to(boost::asio::buffer(result_dumped), client);
            } catch (const boost::system::system_error& ex) {
                logfile << "When sending response to client: " << ex.what() << std::endl;
            }
        }
    }

    json Singer::handle_rep_abs(const json& request, Logfile& log) noexcept {
        json ans;
        try {
            const auto lessons = get_lesson(*mLocalData);
            ans["success"] = false;
            if (!request.contains("sessid")) {
                ans["what"] = "No sessid specified!";
                return ans;
            }
            const int sessid = request["sessid"];
            if (sessid < 0 || sessid >= static_cast<int>(lessons.size())) {
                ans["what"] = "sessid out of range";
                return ans;
            }
            ans["success"] = true;
            ans["name"] = json::array();
                for (auto&& [name, id] : report_absent(*mLocalData, lessons[sessid].id))
                    ans["name"].push_back(std::move(name));
        } catch (const SQLError& ex) {
            ans["success"] = false;
            ans["what"] = ex.what();
        } catch (const std::exception& ex) {
            log << "Unexpected exception in handle_rep_abs()\n";
            ans["what"] = ex.what();
            ans["success"] = false;
        }
        return ans;
    }

    json Singer::handle_wrt_rec(const json& request, Logfile& log) noexcept {
        json ans;
        ans["success"] = false;
        try {
            const auto lessons = get_lesson(*mLocalData);
            const int sessid = request.at("sessid");
            if (sessid < 0 || sessid >= lessons.size())
                throw std::out_of_range("sessid out of range!");
            std::vector<std::string> req_names(request.at("name").begin(), request.at("name").end());
            IncrementalClock clock;
            write_record(*mLocalData, lessons[sessid].id, std::move(req_names), clock);
            ans["success"] = true;
        } catch (const std::out_of_range& ex) {
            ans["what"] = "out_of_range: "s + ex.what();
        } catch (const SQLError& ex) {
            ans["what"] = "SQL error: "s + ex.what();
        } catch (const std::exception& ex) {
            ans["what"] = ex.what();
        }
        return ans;
    }

    json Singer::handle_today(const json& request, Logfile& log) noexcept {
        json ans;
        ans["success"] = false;
        try {
            auto machine_id = get_machine(*mLocalData);
            if (request.at("machine") != machine_id) {
                ans["what"] = "Wrong machine";
                ans["machine"] = std::move(machine_id);
                return ans;
            }
            // Matched here
            auto lessons = get_lesson(*mLocalData);
            ans["end"] = json::array();
            for (auto&& lesson : lessons)
                ans["end"].push_back(Clock::time2str(lesson.endtime));
            ans["success"] = true;
        } catch (const std::out_of_range& ex) {
            ans["what"] = "out_of_range: "s + ex.what();
        } catch (const SQLError& ex) {
            ans["what"] = "SQL error: "s + ex.what();
        } catch (const std::exception& ex) {
            log << "Unexpected std::exception in handle_today()\n";
            ans["what"] = ex.what();
        }
        return ans;
    }

    json Singer::handle_restart(const json& request, Logfile& log) noexcept {
        try {
            send_to_gs(mConfig, log, "$DoRestart");
            return json({{ "success", true }});
        } catch (const NetworkError& ex) {
            return json({{ "success", false }, { "what", ex.what() }});
        } catch (const GSError&) {
            return json({{ "success", false }, { "what", "GS internal error, see logs." }});
        } catch (const std::exception& ex) {
            log << "unexpected '" << ex.what() << "' in handle_restart()\n";
            return json({{ "success", false }, { "what", "UKE, see logs." }});
        }
    }

    json Singer::handle_notice(const json& request, Logfile& log) noexcept {
        try {
            send_to_gs(mConfig, log, "$DoMediaTask");
            return {{ "success", true }};
        } catch (const NetworkError& ex) {
            return json({{ "success", false }, { "what", ex.what() }});
        } catch (const GSError&) {
            return json({{ "success", false }, { "what", "GS internal error, see logs." }});
        } catch (const std::exception& ex) {
            log << "Unexpected std::exception in Singer::handle_notice()\n";
            return json({{ "success", false }, {"what", "Unexpected exception: "s + ex.what() }});
        }
    }

    json Singer::handle_doggie(const json& request, Logfile& log, Watchdog& watchdog) noexcept {
        json ans;
        try {
            const bool pause = request.at("pause");
            if (pause)
                watchdog.pause();
            else
                watchdog.resume();
            ans["success"] = true;
        } catch (const std::out_of_range& ex) {
            ans["success"] = false;
            ans["what"] = "Missing argument: "s + ex.what();
        } catch (const std::exception& ex) {
            ans["success"] = false;
            log << "Unknown error: " << ex.what() << '\n';
            ans["what"] = "Unknown error, see logs.";
        }
        return ans;
    }
}