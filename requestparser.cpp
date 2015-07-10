#include "requestparser.h"

RequestParser::RequestParser() {
    reset();
}

void RequestParser::reset() {
    half_end_of_line = false;
    end_of_line = false;
    first_line = true;
    beginning = true;

    method = "";
    path = "";
    proto_ver = "";

    tmp_header_name = "";
    tmp_header_value = "";

    previous_char = '\0';

    headers.clear();
    headers_available = false;
}

void RequestParser::processChunk(const char *buf, size_t size) {
    char c;
    size_t i = 0;
    c = buf[i];
    for(; i < size; ++i, c = buf[i]) {
        if(c == '\r') {
            half_end_of_line = true;
            goto next_iter;
        } else if(half_end_of_line && c == '\n') {
            if(end_of_line) {
                headers_available = true;
            } else {
                if(!first_line) {
                    headers[tmp_header_name] = tmp_header_value;
                    tmp_header_name = "";
                    tmp_header_value = "";
                }
                end_of_line = true;
                first_line = false;
                goto next_iter;
            }
        }
        if(first_line) {
            static int field = 0;
            if(beginning || end_of_line) {
                field = 0;
            }

            if(c == ' ') {
                field++;
            } else {
                switch(field) {
                case 0:
                    method += c;
                    break;

                case 1:
                    path += c;
                    break;

                case 2:
                    proto_ver += c;
                    break;
                }
            }
        } else {
            static int field = 0;
            if(end_of_line) {
                field = 0;
            }

            switch(field) {
            case 0:
                if(c == ' ' && previous_char == ':') {
                    tmp_header_name.pop_back();
                    field++;
                } else {
                    tmp_header_name += c;
                }
                break;

            case 1:
                tmp_header_value += c;
                break;
            }
        }
        half_end_of_line = false;
        end_of_line = false;
next_iter:
        previous_char = c;
        beginning = false;
    }
}

bool RequestParser::allHeadersAvailable() {
    return headers_available;
}

std::map<std::string, std::string> RequestParser::getHeaders() {
    return headers;
}

std::string RequestParser::getMethod() {
    return method;
}

std::string RequestParser::getPath() {
    return path;
}

std::string RequestParser::getProtocol() {
    return proto_ver;
}
