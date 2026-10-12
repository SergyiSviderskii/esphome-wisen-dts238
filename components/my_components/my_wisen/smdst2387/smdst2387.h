#pragma once
 
#include <cinttypes>
#include <vector>
 
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace smdst2387 {

// Type message 3 byte
enum class Smdst2387MessageType: uint8_t {
  RESPONSE = 0x01,
  SEND = 0x02,
  MODULECONTROL = 0XFE,
};


// Сommands 5 byte
enum class Smdst2387SubcommandType: uint8_t {
  RESP_POWERANDTOTAL = 0x01,
  RESP_MEASUREMENTDATA = 0x0B,
  RESP_LIMITANDPURCHASEDATA = 0x08,
  SEND_LIMITDATA = 0x03,
  SEND_PURCHASEDATA = 0x0D,
  SEND_POWERCUT = 0x09,
  SEND_DELAY = 0x0C,
  SEND_RESET = 0x05,
  SEND_GETDATA_POWERCUT = 0x00,
  SEND_GETDATA_MEASUREMENTDATA = 0x0A,
  SEND_GETDATA_LIMITANDPURCHASEDATA = 0x02,
};


struct Smdst2387Command{
  std::vector<uint8_t> payload;
 };


class SmDST2387 : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void send_command(const Smdst2387Command &command);
  int reset_generic = 0;
  int measurement[22];
  int powerandtotal [8];
  int limitanddatapurcha [7];
  optional<bool> smtime {};
  bool smtimetiming;
  int timetiming [3];
  bool trigger_pass = false;
  std::vector<uint8_t> command_payload;

 protected:
  Smdst2387MessageType init_state_ = Smdst2387MessageType::SEND; 
  void handle_char_(uint8_t c);
  void process_command_queue_();
  void handle_command_(const uint8_t *buffer);
  void handle_datapoints_(const uint8_t *buffer);
  void send_raw_command_(Smdst2387Command command);  
  bool validate_message_();  
  std::vector<uint8_t> rx_message_{};
  uint32_t last_command_timestamp_ = 0;
  uint32_t last_rx_char_timestamp_ = 0;  
  std::vector<Smdst2387Command> command_queue_;
  bool init_failed_{false};
  int init_retries_{0};  
  std::string product_ = "" ;  
  optional<Smdst2387MessageType> expected_response_{}; 
};

}  // namespace smdst2387
}  // namespace esphome  
