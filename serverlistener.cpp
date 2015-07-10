#include "serverlistener.h"

#include <iostream>
#include <cstdlib>
#include <list>
#include <future>
#include <chrono>
#include <thread>
#include <sstream>

#include "requestparser.h"

ServerListener::ServerListener(int port, size_t buffer_size) {
    this->port = port;
    this->buffer_size = buffer_size;
    this->server_running = false;
    this->listen_socket = INVALID_SOCKET;

    WSADATA wsaData;
    if(WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw ServerStartupException();
    }
}

void ServerListener::run(std::function<void(ClientAcceptationException)> client_acceptation_error_callback) {
    std::shared_ptr<addrinfo> socket_props(nullptr, [](addrinfo* ai) { freeaddrinfo(ai); });
    addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    int addrinfo_status = getaddrinfo(NULL, std::to_string(port).c_str(), &hints, (addrinfo**)&socket_props);
    if(addrinfo_status != 0) {
        throw AddrinfoException(addrinfo_status);
    }

    listen_socket = socket(socket_props->ai_family, socket_props->ai_socktype, socket_props->ai_protocol);
    if(listen_socket == INVALID_SOCKET) {
        throw SocketCreationException(WSAGetLastError());
    }

    if(bind(listen_socket, socket_props->ai_addr, (int)socket_props->ai_addrlen) == SOCKET_ERROR) {
        closesocket(listen_socket);
        throw SocketBindingException(WSAGetLastError());
    }

    if(listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listen_socket);
        throw ListenException(WSAGetLastError());
    }

    std::map<SOCKET, std::thread> threads;

    bool server_running = true;
    while(server_running) {
        SOCKET client_socket;

        try {
            client_socket = accept(listen_socket, NULL, NULL);
            if(client_socket == INVALID_SOCKET) {
                throw ClientAcceptationException(WSAGetLastError());
            }
        } catch(ClientAcceptationException &e) {
            client_acceptation_error_callback(e);
            continue;
        }
        threads[client_socket] = std::thread(ServerListener::clientHandler, client_socket, buffer_size);
    }
}

void ServerListener::stop() {
    server_running = false;
    if(listen_socket != INVALID_SOCKET) {
        shutdown(listen_socket, SD_BOTH);
        closesocket(listen_socket);
    }
}

void ServerListener::clientHandler(SOCKET client_socket, size_t buffer_size) {
    int recvbuflen = buffer_size;
    char recvbuf[recvbuflen];
    int bytes_received;
    RequestParser parser;

    sockaddr_in client_info;
    int client_info_len = sizeof(sockaddr_in);
    char *client_ip;

    if(getpeername(client_socket, (sockaddr*)(&client_info), &client_info_len) == SOCKET_ERROR) {
        goto cleanup;
    }
    client_ip = inet_ntoa(client_info.sin_addr);

    while(1) {
        parser.reset();

        bool headers_ready = false;
        while(!headers_ready) {
            bytes_received = recv(client_socket, recvbuf, recvbuflen, 0);
            if(bytes_received > 0) {
                parser.processChunk(recvbuf, bytes_received);
                if(parser.allHeadersAvailable()) {
                    headers_ready = true;
                }
            } else {
                goto cleanup;
            }
        }

        auto headers = parser.getHeaders();

        auto conn_it = headers.find("Connection");
        if(conn_it != headers.end() && conn_it->second == "close") {
            goto cleanup;
        }

        std::cout << parser.getMethod() << " "
                  << parser.getPath() << " "
                  << parser.getProtocol() << "\n";

        std::cout << "> " << client_ip << "\n";

        auto ua_it = headers.find("User-Agent");
        if(ua_it != headers.end()) {
            std::cout << "> " << ua_it->second << "\n";
        } else {
            std::cout << "> no UAString provided" << "\n";
        }

        std::cout << "\n";

        std::string response_body = "<!DOCTYPE html><html><head>"
            "<title>Request info</title>"
            "<style>"
                "table, th, td { border: 1px solid #333; }"
                "th, td { padding: 3px 5px; }"
                "th { text-align: right; }"
                "td { text-align: left; }"
            "</style>"
        "</head>"
        "<body><h1>Request info</h1>";

        response_body += "<table>";

        response_body += "<tr><th>Method</th><td>" + parser.getMethod() + "</td></tr>";
        response_body += "<tr><th>Path</th><td>" + parser.getPath() + "</td></tr>";
        response_body += "<tr><th>Protocol</th><td>" + parser.getProtocol() + "</td></tr>";

        for(const auto &header : headers) {
            response_body += "<tr><th>" + header.first + "</th><td>" + header.second + "</td></tr>";
        }

        std::stringstream ss;
        ss << std::this_thread::get_id();
        response_body += "<tr><th>Thread ID</th><td>" + ss.str() + "</td></tr>";

        response_body += "</table>";
        response_body += "</body></html>\r\n";


        std::string response_headers = "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Connection: keep-alive\r\n"
        "Content-Length: " + std::to_string(response_body.length()) + "\r\n\r\n";

        std::string response = response_headers + response_body;
        send(client_socket, response.c_str(), strlen(response.c_str()), 0);
    }
cleanup:
    closesocket(client_socket);
}

ServerListener::~ServerListener() {
    WSACleanup();
}
