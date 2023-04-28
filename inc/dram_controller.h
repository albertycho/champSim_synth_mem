/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DRAM_H
#define DRAM_H

#include <array>
#include <cmath>
#include <limits>
#include "champsim_constants.h"
#include "memory_class.h"
#include "operable.h"
#include "util.h"
#include <iostream>
#include <stdio.h>


struct M_REQUEST {
  bool valid = false;

  uint64_t event_cycle = 0;
  uint64_t address=0;
  PACKET pkt;
  //std::vector<PACKET>::iterator pkt;
};

struct DRAM_CHANNEL {
  //std::vector<PACKET> WQ{2048};
  //std::vector<PACKET> RQ{2048};
  
  //keep all queue
  std::vector<M_REQUEST> WQ{2048}; //just make it huge. Want to fit everything that comes
  std::vector<M_REQUEST> RQ{2048};
  //champsim::delay_queue<CH_REQUEST> ch_request{2048, 0};
  //std::array<CH_REQUEST, DRAM_RANKS* DRAM_BANKS>::iterator active_request = std::end(ch_request);
  uint64_t ch_latency = 180;

};

class MEMORY_CONTROLLER : public champsim::operable, public MemoryRequestConsumer
{
public:
  std::array<DRAM_CHANNEL, DRAM_CHANNELS> channels;

  MEMORY_CONTROLLER(double freq_scale) : champsim::operable(freq_scale), MemoryRequestConsumer(std::numeric_limits<unsigned>::max()) {}

  int add_rq(PACKET* packet) override;
  int add_wq(PACKET* packet) override;
  int add_pq(PACKET* packet) override;

  void operate() override;

  uint32_t get_occupancy(uint8_t queue_type, uint64_t address) override;
  uint32_t get_size(uint8_t queue_type, uint64_t address) override;

  uint32_t dram_get_channel(uint64_t address);
};

#endif
