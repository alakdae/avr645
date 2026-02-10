#pragma once

#include "esphome/core/component.h"
#include "esphome/components/socket/socket.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <memory>
#include <vector>
#include <string>

namespace esphome {
namespace avr645 {

static const char *TAG = "avr645";

class AVR645 : public Component {
 public:
  static AVR645 *instance;

  enum ParserState {
    WAIT_MPSEND,
    WAIT_META,
    WAIT_F0,
    READ_UPPER,
    WAIT_F1,
    READ_LOWER,
    WAIT_F2,
  };
  
  ParserState parser_state_{WAIT_MPSEND};
  uint8_t sig_pos_{0};
  uint8_t text_pos_{0};
  char upper_buf_[15];
  char lower_buf_[15];

  // Setters for the Python/YAML glue
  void set_uart_parent(uart::UARTComponent *parent) { this->stream_ = parent; }
  void set_upper_sensor(text_sensor::TextSensor *s) { this->upper_ = s; }
  void set_lower_sensor(text_sensor::TextSensor *s) { this->lower_ = s; }
  void set_volume_sensor(sensor::Sensor *s) { this->volume_ = s; }
  void set_port(uint16_t port) { this->port_ = port; }
  void set_buffer_size(size_t size) { this->buf_size_ = size; }

  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void setup() override {
    instance = this;
    ESP_LOGCONFIG(TAG, "Setting up AVR645 on port %u...", this->port_);

    this->buf_ = std::unique_ptr<uint8_t[]>{new uint8_t[this->buf_size_]};

    this->socket_ = socket::socket_ip(SOCK_STREAM, PF_INET);
    if (!this->socket_) {
        ESP_LOGE(TAG, "Failed to create socket");
        return;
    }
    this->socket_->setblocking(false);

    struct sockaddr_storage bind_addr;
    socklen_t bind_addrlen = socket::set_sockaddr_any(reinterpret_cast<struct sockaddr *>(&bind_addr), sizeof(bind_addr), this->port_);

    if (this->socket_->bind(reinterpret_cast<struct sockaddr *>(&bind_addr), bind_addrlen) != 0) {
        ESP_LOGE(TAG, "Socket bind failed for port %u", this->port_);
        return;
    }
    this->socket_->listen(8);
    ESP_LOGI(TAG, "AVR645 TCP Server listening on port %u", this->port_);
  }

  void loop() override {
    this->accept();
    this->read();   // Read from UART, parse MPSEND, fill ring buffer
    this->flush();  // Send ring buffer to TCP clients
    this->write();  // Read from TCP, write to UART
    this->cleanup();
  }

  void on_shutdown() override {
    for (const Client &client : this->clients_)
      client.socket->shutdown(SHUT_RDWR);
  }

  // Button commands: HA -> C++ -> UART
  void send_info(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    uint8_t info[4] = {a, b, c, d};
    uint8_t frame[14];
    memcpy(frame, "PCSEND", 6);
    frame[6] = 0x02; frame[7] = 0x04;
    memcpy(frame + 8, info, 4);
    uint16_t chk = (info[0] << 8 | info[1]) ^ (info[2] << 8 | info[3]);
    frame[12] = chk >> 8; frame[13] = chk & 0xFF;
    this->stream_->write_array(frame, 14);
    this->stream_->flush();
    ESP_LOGD(TAG, "PCSEND: %02X %02X %02X %02X", a, b, c, d);
  }

 protected:
  struct Client {
    Client(std::unique_ptr<socket::Socket> socket, std::string identifier, size_t position)
        : socket(std::move(socket)), identifier(std::move(identifier)), position(position) {}
    std::unique_ptr<socket::Socket> socket{nullptr};
    std::string identifier{};
    bool disconnected{false};
    size_t position{0};
  };

  void process_byte_(uint8_t c) {
    static const char sig[] = "MPSEND";
  
    switch (this->parser_state_) {
  
      case WAIT_MPSEND:
        if (c == sig[this->sig_pos_]) {
          this->sig_pos_++;
          if (this->sig_pos_ == 6) {
            this->sig_pos_ = 0;
            this->parser_state_ = WAIT_META;
          }
        } else {
          this->sig_pos_ = 0;
        }
        break;
  
      case WAIT_META:
        // Expect 0x03 then 0x32, we only care about 0x32
        if (c == 0x32) {
          this->parser_state_ = WAIT_F0;
        }
        break;
  
      case WAIT_F0:
        if (c == 0xF0) {
          this->text_pos_ = 0;
          this->parser_state_ = READ_UPPER;
        }
        break;
  
      case READ_UPPER:
        this->upper_buf_[this->text_pos_++] = print_(c);
        if (this->text_pos_ == 14) {
          this->upper_buf_[14] = 0;
          this->parser_state_ = WAIT_F1;
        }
        break;
  
      case WAIT_F1:
        if (c == 0xF1) {
          this->text_pos_ = 0;
          this->parser_state_ = READ_LOWER;
        }
        break;
  
      case READ_LOWER:
        this->lower_buf_[this->text_pos_++] = print_(c);
        if (this->text_pos_ == 14) {
          this->lower_buf_[14] = 0;
          this->parser_state_ = WAIT_F2;
        }
        break;
  
      case WAIT_F2:
        if (c == 0xF2) {
          std::string upper = trim_(this->upper_buf_);
          std::string lower = trim_(this->lower_buf_);
      
          this->upper_->publish_state(upper);
          this->lower_->publish_state(lower);
      
          if (this->volume_ != nullptr) {
            auto vol = parse_volume_(lower);
            if (vol.has_value()) {
              this->volume_->publish_state(*vol);
            }
          }
      
          this->parser_state_ = WAIT_MPSEND;
        }
        break;
    }
  }

  static optional<float> parse_volume_(const std::string &line) {
    // Expected: "VOL -43dB"
    size_t pos = line.find("VOL");
    if (pos == std::string::npos) return {};
  
    pos += 3; // skip "VOL"
    while (pos < line.size() && line[pos] == ' ') pos++;
  
    bool negative = false;
    if (pos < line.size() && line[pos] == '-') {
      negative = true;
      pos++;
    }
  
    if (pos >= line.size() || !isdigit(line[pos])) return {};
  
    int value = 0;
    while (pos < line.size() && isdigit(line[pos])) {
      value = value * 10 + (line[pos] - '0');
      pos++;
    }
  
    if (negative) value = -value;
  
    // Optional: check for "dB"
    if (pos + 1 < line.size() && line[pos] == 'd' && line[pos + 1] == 'B') {
      return value;
    }
  
    // Even if "dB" is missing, accept the number
    return value;
  }
  
  void accept() {
    struct sockaddr_storage client_addr;
    socklen_t client_addrlen = sizeof(client_addr);
  
    std::unique_ptr<socket::Socket> socket =
        this->socket_->accept(reinterpret_cast<struct sockaddr *>(&client_addr), &client_addrlen);
    if (!socket)
      return;
  
    socket->setblocking(false);
  
    std::string identifier = "unknown";
  
    if (client_addr.ss_family == AF_INET) {
      char ip[INET_ADDRSTRLEN];
      auto *addr = reinterpret_cast<struct sockaddr_in *>(&client_addr);
      inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));
      identifier = ip;
    }
  #if LWIP_IPV6
    else if (client_addr.ss_family == AF_INET6) {
      char ip[INET6_ADDRSTRLEN];
      auto *addr = reinterpret_cast<struct sockaddr_in6 *>(&client_addr);
      inet_ntop(AF_INET6, &addr->sin6_addr, ip, sizeof(ip));
      identifier = ip;
    }
  #endif
  
    this->clients_.emplace_back(std::move(socket), identifier, this->buf_head_);
    ESP_LOGD(TAG, "New client connected from %s", identifier.c_str());
  }

  void read() {
    while (this->stream_->available() > 0) {
      uint8_t c;
      this->stream_->read_byte(&c);
  
      // --- existing ring buffer logic (UNCHANGED) ---
      this->buf_[this->buf_index(this->buf_head_)] = c;
      this->buf_head_++;
  
      if (this->buf_head_ - this->buf_tail_ > 128) {
        this->buf_tail_ = this->buf_head_ - 64;
      }
      // ---------------------------------------------
  
      this->process_byte_(c);
    }
  }

  // Helper to check for the "MPSEND" string ending at the current position
  bool check_header_(size_t pos) {
    const char* sig = "MPSEND";
    for (int i = 0; i < 6; i++) {
        if (this->buf_[this->buf_index(pos - 5 + i)] != sig[i]) return false;
    }
    return true;
  }

  void parse_mpsend_(size_t header_end_pos) {
    // The header ended at header_end_pos. 
    // The markers F0/F1 should be within the next 50 bytes.
    
    // We wait a tiny bit or look back if we have enough data
    std::string u = "", l = "";
    bool found_u = false, found_l = false;

    // Scan forward from the end of "MPSEND"
    for (size_t j = 1; j < 60; j++) {
        size_t idx = this->buf_index(header_end_pos + j);
        uint8_t val = this->buf_[idx];

        if (val == 0xF0 && !found_u) {
            for (int x = 1; x <= 14; x++) 
                u += print_(this->buf_[this->buf_index(header_end_pos + j + x)]);
            found_u = true;
        }
        if (val == 0xF1 && !found_l) {
            for (int x = 1; x <= 14; x++) 
                l += print_(this->buf_[this->buf_index(header_end_pos + j + x)]);
            found_l = true;
        }
        if (found_u && found_l) break;
    }

    if (found_u) this->upper_->publish_state(trim_(u));
    if (found_l) this->lower_->publish_state(trim_(l));
  }

  void flush() {
    this->buf_tail_ = this->buf_head_;
    for (Client &client : this->clients_) {
      if (client.disconnected || client.position == this->buf_head_) continue;
      struct iovec iov[2];
      iov[0].iov_base = &this->buf_[this->buf_index(client.position)];
      iov[0].iov_len = std::min(this->buf_head_ - client.position, this->buf_ahead(client.position));
      iov[1].iov_base = &this->buf_[0];
      iov[1].iov_len = this->buf_head_ - (client.position + iov[0].iov_len);
      ssize_t written = client.socket->writev(iov, 2);
      if (written > 0) client.position += written;
      else if (written == 0 || errno == ECONNRESET) client.disconnected = true;
      this->buf_tail_ = std::min(this->buf_tail_, client.position);
    }
  }

  void write() {
    uint8_t buf[128];
    static std::vector<uint8_t> tcp_buffer; // Temporary storage for incoming TCP bytes

    for (Client &client : this->clients_) {
      if (client.disconnected) continue;

      ssize_t read_len = client.socket->read(&buf, sizeof(buf));
      if (read_len > 0) {
        // Add new bytes to our temporary buffer
        for (int i = 0; i < read_len; i++) tcp_buffer.push_back(buf[i]);

        // 1. Check if we have the 4-byte shortcut
        if (tcp_buffer.size() == 4) {
          ESP_LOGD(TAG, "TCP 4-byte shortcut detected");
          this->send_info(tcp_buffer[0], tcp_buffer[1], tcp_buffer[2], tcp_buffer[3]);
          tcp_buffer.clear();
        }
      }
      else if (read_len == 0) {
        client.disconnected = true;
        tcp_buffer.clear();
      }

      // Safety: Don't let the temp buffer grow forever if garbage is sent
      if (tcp_buffer.size() > 32) tcp_buffer.clear();
    }
  }
  
  void cleanup() {
    this->clients_.erase(std::remove_if(this->clients_.begin(), this->clients_.end(),
                         [](const Client &c) { return c.disconnected; }), this->clients_.end());
  }

  size_t buf_index(size_t pos) { return pos & (this->buf_size_ - 1); }
  size_t buf_ahead(size_t pos) { return (pos | (this->buf_size_ - 1)) - pos + 1; }
  static char print_(uint8_t c) { return (c >= 32 && c < 127) ? (char)c : ' '; }
  static std::string trim_(std::string s) {
    size_t f = s.find_first_not_of(' ');
    return (f == std::string::npos) ? "" : s.substr(f, s.find_last_not_of(' ') - f + 1);
  }

  uart::UARTComponent *stream_{nullptr};
  text_sensor::TextSensor *upper_{nullptr};
  text_sensor::TextSensor *lower_{nullptr};
  sensor::Sensor *volume_{nullptr};
  uint16_t port_{4001};
  size_t buf_size_{1024};
  std::unique_ptr<uint8_t[]> buf_{};
  size_t buf_head_{0};
  size_t buf_tail_{0};
  std::unique_ptr<socket::Socket> socket_{};
  std::vector<Client> clients_{};
};

AVR645 *AVR645::instance = nullptr;

}  // namespace avr645
}  // namespace esphome
