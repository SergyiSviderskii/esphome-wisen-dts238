#include "smdst2387.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace smdst2387 {
 
static const char *const TAG = "SmWisen";

static const int COMMAND_DELAY = 10;
static const int RECEIVE_TIMEOUT = 300;
static const int MAX_RETRIES = 5; 

void SmDST2387::setup() {    
  this->set_interval("heartbeat", 15000, [this] { if (reset_generic == 0 || reset_generic == 5) {send_command(Smdst2387Command{.payload = {0x48, 0x06, 0x02, 0x00, 0x00}});}});
}

void SmDST2387::loop() {
  while (this->available()) {
    uint8_t c;
    this->read_byte(&c);
    this->handle_char_(c);
  }
  process_command_queue_();
}    

void SmDST2387::handle_char_(uint8_t c) {
  this->rx_message_.push_back(c);
  if (!this->validate_message_()) {
    this->rx_message_.clear();
  } else {
    this->last_rx_char_timestamp_ = millis();
  }
}

bool SmDST2387::validate_message_() {
  uint32_t at = this->rx_message_.size() - 1;
  auto *data = &this->rx_message_[0];
  uint8_t new_byte = data[at];

  // Byte 0: HEADER (always 0x48)
  if (at == 0) {
    if (new_byte == 0x48) {
      return true;
    } else {
        return false;
    }
  } 
    
  // Byte 1: Frame length (0x06 <= length <= 0xFE)
  uint8_t length = data[1];
  if (at == 1) {
    if (0x06 <= length && length <= 0xFE ) {
      return true;
    } else {
        return false;
    }
  } 
    
  // 0x01 device report frame Device --> Module --> APP; Module, return original frame
  // 0x02 Module sends frame APP-->Module-->Device; Device, return original frame
  // 0xFE Module working frame Device --> Module; Module, depending on frame command
  // 0xFF error frame module <——> device; nobody
  uint8_t type = data[2];
  if (at == 2) {
    if (type == 0x01 || type == 0x02 || type == 0xFE || type == 0XFF) {
      return true;
    } else {
        return false;
    }  
  } 
   
  // frame number    
  uint8_t number = data[3];
  if (at == 3)
    return true;
 
  // wait until all data is read
  if (at < length - 1) {
    return true;            // no validation for these fields
  }
  
  // CHECKSUM - sum of all bytes (including header) modulo 256 
  uint8_t rx_checksum = new_byte;
  uint8_t calc_checksum = 0;
  for (uint8_t i = 0; i < length - 1; i++)
    calc_checksum += data[i];

  if (rx_checksum != calc_checksum) {
    ESP_LOGW(TAG, "SM Received invalid message checksum %02X!=%02X", rx_checksum, calc_checksum);
    return false;                          
  }
  
   ESP_LOGD(TAG, "Received: FRAME=[%s]", format_hex_pretty(rx_message_).c_str());
  
  // valid message
  const uint8_t *message_data = data; 
  this->handle_command_(message_data);

  return false;    // return false to reset rx buffer
  
}

void  SmDST2387::handle_command_(const uint8_t *buffer) {
  Smdst2387MessageType message_type = (Smdst2387MessageType) buffer[2]; 

  if (this->expected_response_.has_value() && this->expected_response_ == message_type) {
    this->expected_response_.reset();
    this->command_queue_.erase(command_queue_.begin());
    this->init_retries_ = 0;
  }
  
  
  switch (message_type) {
    case Smdst2387MessageType::SEND:
      ESP_LOGV(TAG, "MCU Heartbeat (0x%02X)", buffer[0]);
      if (buffer[0] == 0) {
        ESP_LOGI(TAG, "MCU restarted");
        this->init_state_ = Smdst2387MessageType::SEND;      
      }
      if (this->init_state_ == Smdst2387MessageType::SEND) {
        this->init_state_ = Smdst2387MessageType::RESPONSE;    
      }
      break;
    case Smdst2387MessageType::MODULECONTROL:   
    case Smdst2387MessageType::RESPONSE: { 
      std::vector<uint8_t> data;    
      uint8_t length = buffer[1]; 
      data.clear();
      for (uint8_t i = 0; i < length - 1; i++)
        data.push_back(buffer[i]);
      send_command(Smdst2387Command{.payload = data});
      this->handle_datapoints_(buffer);
      break;
    }
    default:
      ESP_LOGE(TAG, "Invalid command (0x%02X) received", message_type);    
  }
}
   

void SmDST2387::process_command_queue_() {
  uint32_t now = millis();
  uint32_t delay = now - this->last_command_timestamp_;

  if (now - this->last_rx_char_timestamp_ > RECEIVE_TIMEOUT) {
    this->rx_message_.clear();
  }
  
  if (this->expected_response_.has_value() && delay > RECEIVE_TIMEOUT) {
    this->expected_response_.reset();
    if (this->init_state_ != Smdst2387MessageType::RESPONSE) {
      if (++this->init_retries_ >= MAX_RETRIES) {
        this->init_failed_ = true;
        ESP_LOGE(TAG, "Initialization failed at init_state %u", static_cast<uint8_t>(this->init_state_));
        this->command_queue_.erase(command_queue_.begin());
        this->init_retries_ = 0;
      }
    } else {
      this->command_queue_.erase(command_queue_.begin());
    }
  }

  if (delay > COMMAND_DELAY && !this->command_queue_.empty() && this->rx_message_.empty() && !this->expected_response_.has_value()) {    
    this->send_raw_command_(command_queue_.front());
    if (!this->expected_response_.has_value()) 
      this->command_queue_.erase(command_queue_.begin());
  }
} 


void SmDST2387::handle_datapoints_(const uint8_t *buffer) {
  Smdst2387SubcommandType report_frames = (Smdst2387SubcommandType)buffer[4];
  
  //device report frames
  switch (report_frames) {
    // meter detail, frame length 0x43
    case Smdst2387SubcommandType::RESP_MEASUREMENTDATA: {
        
      measurement[0] = (buffer[58] << 24) | (buffer[59] << 16) | (buffer[60] << 8) | buffer[61];   // Import active energy
      measurement[1] = (buffer[62] << 24) | (buffer[63] << 16) | (buffer[64] << 8) | buffer[65];   // Export active energy
      measurement[2] = (buffer[54] << 24) | (buffer[55] << 16) | (buffer[56] << 8) | buffer[57];   // Contract active energy
      measurement[3] = (buffer[52] << 8) | buffer[53];                                             // Frequency
      
      measurement[4] = (buffer[14] << 8) | buffer[15];                                             // RMS voltage phase a
      measurement[5] = (buffer[5] << 16) | (buffer[6] << 8) | buffer[7];                           // RMS current phase a
      if (measurement[5] > 1000000 ) {measurement[5] = 1000000 - measurement[5];}               
      measurement[6] = (buffer[35] << 16) | (buffer[36] << 8) | buffer[37];                        // RMS active power phase a
      measurement[7] = (buffer[23]  << 16) | (buffer[24] << 8) | buffer[25];                       //  RMS reactive power phase a
      if (measurement[7] >= 1000000 ) {measurement[7] = 1000000 - measurement[7];}
      measurement[8] = (buffer[46] << 8) | buffer[47];                                             //  RMS power factor phase a

      measurement[9] = (buffer[16] << 8) | buffer[17];                                             // RMS voltage phase b
      measurement[10] = (buffer[8] << 16) | (buffer[9] << 8) | buffer[10];                         // RMS current phase b
      if (measurement[10] > 1000000 ) {measurement[10] = 1000000 - measurement[10];}  
      measurement[11] = (buffer[38] << 16) | (buffer[39] << 8) | buffer[40];                       // RMS active power phase b
      measurement[12] = (buffer[26] << 16) | (buffer[27] << 8) | buffer[28];                       //  RMS reactive power phase b  
      if (measurement[12] >= 1000000 ) {measurement[12] = 1000000 - measurement[12];}
      measurement[13] = (buffer[48] << 8) | buffer[49];                                            //  RMS power factor phase b
      
     
      measurement[14] = (buffer[18] << 8) | buffer[19];                                            // RMS voltage phase c
      measurement[15] = (buffer[11] << 16) | (buffer[12] << 8) | buffer[13];                       // RMS current phase c
      if (measurement[15] > 1000000 ) {measurement[15] = 1000000 - measurement[15];}  
      measurement[16] = (buffer[41] << 16) | (buffer[42] << 8) | buffer[43];                       // RMS active power phase c
      measurement[17] = (buffer[29] << 16) | (buffer[30] << 8) | buffer[31];                       //  RMS reactive power phase c
      if (measurement[17] >= 1000000 ) {measurement[17] = 1000000 - measurement[17];}
      measurement[18] = (buffer[50] << 8) | buffer[51];                                            //  RMS power factor phase c
      
      measurement[19]= (buffer[32] << 16) | (buffer[33] << 8) | buffer[34];                        // RMS active power group
      measurement[20] = (buffer[20] << 16) | (buffer[21] << 8) | buffer[22];                       // RMS reactive power group
      if (measurement[20] >= 1000000 ) {measurement[20] = 1000000 - measurement[20];}
      measurement[21] = (buffer[44] << 8) | buffer[45];                                            // RMS power factor group
      
      
      trigger_pass = true;      
      
    }  
    break;
    // power and total frame length 0x15
    case Smdst2387SubcommandType::RESP_POWERANDTOTAL: {
       
      powerandtotal[0] = buffer[6];                                                                   // Power
      powerandtotal[1] = (buffer[7] << 24) | (buffer[8] << 16) | (buffer[9] << 8) | buffer[10];       // Total active energy  
      powerandtotal[2] = (buffer[11] << 24) | (buffer[12] << 16) | (buffer[13] << 8) | buffer[14];    // Total active power

      this->set_timeout("metr_detail_1", 500, [this] {send_command(Smdst2387Command{.payload = {0x48, 0x06, 0x02, 0x00, 0x0A}});});
    }
    break;
    // setting parameters, frame length 0x19
    case  Smdst2387SubcommandType::RESP_LIMITANDPURCHASEDATA: {
      limitanddatapurcha [0] = (buffer[5] << 8) | buffer[6];                                                                            // Voltage high limit
      limitanddatapurcha [1] = (buffer[7] << 8) | buffer[8];                                                                            // Voltage low limit
      limitanddatapurcha [2] = (buffer[9] << 8) | buffer[10];                                                                           // Current high limit
       
      limitanddatapurcha [3] = (buffer[11] << 24) | (buffer[12] << 16) | (buffer[13] << 8) | buffer[14];                                // Purchase kWh
      limitanddatapurcha [4] = (buffer[15] << 24) | (buffer[16] << 16) | (buffer[17] << 8) | buffer[18];                                // Setting balance kWh
      limitanddatapurcha [5] = (buffer[19] << 24) | (buffer[20] << 16) | (buffer[21] << 8) | buffer[22];                                // Setting balance alarm kWh
      limitanddatapurcha [6] = buffer[23];                                                                                              // Prepayment fuction ON/OFF

      trigger_pass = true;
    }
    break;
    default:
    break; 
  }
}


// Write frame for transmission
void SmDST2387::send_command(const Smdst2387Command &command) {
  command_queue_.push_back(command);
  process_command_queue_();
}


// Frame transmission
void SmDST2387::send_raw_command_(Smdst2387Command command) { 
  this->write_array(command.payload);
  uint8_t checksum = 0;
  for (auto &data : command.payload)
    checksum += data;
  this->write_byte(checksum);
  this->expected_response_ = (Smdst2387MessageType) command.payload[2];
  this->last_command_timestamp_ = millis();
  command.payload.push_back(checksum);
  ESP_LOGD(TAG, "Sending: FRAME=[%s]", format_hex_pretty(command.payload).c_str());  
}

}  // namespace smdst2387
}  // namespace esphome
