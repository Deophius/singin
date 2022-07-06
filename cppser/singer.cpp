// Implementation for Singer class's mainloop()
#include <boost/asio.hpp>
#include "singd.h"

namespace {
}

namespace Spirit {
    using nlohmann::json;
    
    void Singer::mainloop() {
        namespace asio = boost::asio;
        using asio::ip::udp;
        Logfile logfile("singer.log");
        // If intro or serv_port are missing, no need to go on.
        logfile << mConfig["intro"].get<std::string>() << '\n';
        asio::io_context ioc;
        udp::socket serv_sock(ioc, udp::endpoint(udp::v4(), mConfig["serv_port"]));
        mLocalData.reset(new Connection(mConfig["dbname"], mConfig["passwd"]));
        logfile.flush();
        while (true) {
            udp::endpoint client;
            // Get the request
            nlohmann::json request;
            // The result to be returned
            nlohmann::json result;
            try {
                // Make sure to flush logs
                LogSection log_section(logfile);
                asio::streambuf req_buf;
                req_buf.commit(serv_sock.receive_from(req_buf.prepare(1024), client));
                std::istream rfile(&req_buf);
                rfile >> request;
                logfile << "Received: " << request.dump() << '\n';
            } catch (const boost::system::system_error& ex) {
                logfile << ex.what() << '\n';
                continue;
            } catch (nlohmann::json::parse_error& ex) {
                logfile << ex.what() << '\n';
                result["success"] = false;
                result["what"] = "Unrecognized format, "s + ex.what();
            }
            if (!request.contains("command")) {
                result["success"] = false;
                result["what"] = "Missing command!";
            } else {
                const std::string command = request["command"];
                if (command == "report_absent")
                    result = handle_rep_abs(request, logfile);
            }
            LogSection logsection(logfile);
            auto result_dumped = result.dump();
            logfile << result_dumped << '\n';
            serv_sock.send_to(boost::asio::buffer(result_dumped), client);
        }
    }

    json Singer::handle_rep_abs(const json& request, Logfile& log) {
        const auto lessons = get_lesson(*mLocalData);
        json ans;
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
        // FIXME: SQL operations might throw exceptions. Add Connection::get_last_error_msg.
        ans["success"] = true;
        ans["name"] = json::array();
        for (auto&& [name, id] : report_absent(*mLocalData, lessons[sessid].id))
            ans["name"].push_back(std::move(name));
        return ans;
    }
}